#include "internal/profiler.hpp"
#include "internal/log.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <numeric>

namespace grf {

namespace {

constexpr std::size_t kZoneRollingWindow = 120;

uint32_t slotFirstQuery(uint32_t slot) {
  return slot * Profiler::Impl::kMaxZonesPerFrame * 2;
}

uint32_t slotQueryCount() {
  return Profiler::Impl::kMaxZonesPerFrame * 2;
}

}

Profiler::Impl::Impl(vk::Device device, vk::PhysicalDevice gpu, uint32_t flightFrames)
: m_device(device), m_flightFrames(flightFrames)
{
  auto props = gpu.getProperties();
  m_timestampPeriodNs = static_cast<double>(props.limits.timestampPeriod);

  uint32_t validBits = 0;
  auto qfProps = gpu.getQueueFamilyProperties();
  for (const auto& q : qfProps) {
    if (q.timestampValidBits > validBits) validBits = q.timestampValidBits;
  }
  if (validBits == 0)
    GRF_PANIC("GPU does not support timestamp queries");
  m_timestampMask = (validBits == 64) ? ~uint64_t{0} : ((uint64_t{1} << validBits) - 1);

  m_pool = m_device.createQueryPool(vk::QueryPoolCreateInfo{
    .queryType  = vk::QueryType::eTimestamp,
    .queryCount = kMaxZonesPerFrame * 2 * flightFrames
  });

  m_device.resetQueryPool(m_pool, 0, kMaxZonesPerFrame * 2 * flightFrames);

  m_pending.resize(flightFrames);
  m_graphBuffer.resize(kHistoryLength, 0.0f);
}

Profiler::Impl::~Impl() {
  m_device.destroyQueryPool(m_pool);
}

void Profiler::Impl::beginFrame(double dtSeconds, uint32_t frameSlot) {
  m_currentSlot = frameSlot;

  if (m_framesElapsed >= m_flightFrames) {
    auto& zones = m_pending[m_currentSlot];
    if (!zones.empty()) {
      const uint32_t first = slotFirstQuery(m_currentSlot);
      const uint32_t count = slotQueryCount();
      std::vector<uint64_t> raw(count, 0);

      auto res = m_device.getQueryPoolResults(
        m_pool, first, count,
        raw.size() * sizeof(uint64_t), raw.data(), sizeof(uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
      );

      if (res == vk::Result::eSuccess) {
        for (const auto& z : zones) {
          uint32_t localBegin = z.beginIdx - first;
          uint32_t localEnd   = z.endIdx   - first;
          uint64_t b = raw[localBegin] & m_timestampMask;
          uint64_t e = raw[localEnd]   & m_timestampMask;
          uint64_t ticks = (e >= b) ? (e - b) : 0;
          double   ns    = static_cast<double>(ticks) * m_timestampPeriodNs;
          double   ms    = ns / 1e6;

          auto& s = m_stats[z.name];
          s.recent.push_back(ms);
          while (s.recent.size() > kZoneRollingWindow) s.recent.pop_front();
          s.avgMs = std::accumulate(s.recent.begin(), s.recent.end(), 0.0) / s.recent.size();
        }
      }
    }
  }

  m_device.resetQueryPool(m_pool, slotFirstQuery(m_currentSlot), slotQueryCount());
  m_pending[m_currentSlot].clear();
  m_zoneCounter = 0;

  double dtMs = dtSeconds * 1000.0;
  m_frameTimesMs.push_back(dtMs);
  while (m_frameTimesMs.size() > kHistoryLength) m_frameTimesMs.pop_front();

  if (!m_frameTimesMs.empty()) {
    m_avgFrameTimeMs =
      std::accumulate(m_frameTimesMs.begin(), m_frameTimesMs.end(), 0.0) / m_frameTimesMs.size();
  }

  std::fill(m_graphBuffer.begin(), m_graphBuffer.end(), 0.0f);
  std::size_t offset = kHistoryLength - m_frameTimesMs.size();
  for (std::size_t i = 0; i < m_frameTimesMs.size(); ++i) {
    m_graphBuffer[offset + i] = static_cast<float>(m_frameTimesMs[i]);
  }

  ++m_framesElapsed;
}

std::pair<uint32_t, uint32_t> Profiler::Impl::allocateZone(std::string name) {
  if (m_zoneCounter >= kMaxZonesPerFrame)
    GRF_PANIC("Profiler: exceeded {} zones per frame", kMaxZonesPerFrame);

  uint32_t base     = slotFirstQuery(m_currentSlot);
  uint32_t beginIdx = base + m_zoneCounter * 2;
  uint32_t endIdx   = beginIdx + 1;
  ++m_zoneCounter;

  m_pending[m_currentSlot].push_back(PendingZone{
    .name     = std::move(name),
    .beginIdx = beginIdx,
    .endIdx   = endIdx
  });

  return { beginIdx, endIdx };
}

Profiler::Profiler(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Profiler::~Profiler() = default;

void Profiler::render() {
  ImGui::Begin("Profiler");

  const double fps = (m_impl->m_avgFrameTimeMs > 0.0) ? (1000.0 / m_impl->m_avgFrameTimeMs) : 0.0;

  char fpsText[64];
  std::snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
  char avgText[64];
  std::snprintf(avgText, sizeof(avgText), "Avg: %.2f ms", m_impl->m_avgFrameTimeMs);

  ImGui::TextUnformatted(fpsText);
  ImGui::SameLine();
  float avail    = ImGui::GetContentRegionAvail().x;
  float avgWidth = ImGui::CalcTextSize(avgText).x;
  if (avail > avgWidth)
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - avgWidth));
  ImGui::TextUnformatted(avgText);

  float maxVal = 0.0f;
  for (float v : m_impl->m_graphBuffer) if (v > maxVal) maxVal = v;
  if (maxVal < 16.0f) maxVal = 16.0f;

  ImGui::PlotLines(
    "##frametime",
    m_impl->m_graphBuffer.data(),
    static_cast<int>(m_impl->m_graphBuffer.size()),
    0,
    nullptr,
    0.0f,
    maxVal * 1.1f,
    ImVec2(-FLT_MIN, 80.0f)
  );

  ImGui::Separator();

  if (m_impl->m_stats.empty()) {
    ImGui::TextDisabled("(no profile zones recorded yet)");
  } else {
    if (ImGui::BeginTable("zones", 2,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Zone",        ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Avg (ms)",    ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableHeadersRow();

      for (const auto& [name, s] : m_impl->m_stats) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.3f", s.avgMs);
      }

      ImGui::EndTable();
    }
  }

  ImGui::End();
}

}
