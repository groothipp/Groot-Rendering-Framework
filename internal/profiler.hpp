#pragma once

#include "public/profiler.hpp"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace grf {

class Profiler::Impl {
public:
  static constexpr uint32_t kMaxZonesPerFrame = 64;
  static constexpr std::size_t kHistoryLength = 240;

  vk::Device      m_device   = nullptr;
  vk::QueryPool   m_pool     = nullptr;
  uint32_t        m_flightFrames = 0;
  uint32_t        m_currentSlot  = 0;
  uint32_t        m_zoneCounter  = 0;
  uint64_t        m_framesElapsed = 0;
  bool            m_slotResetPending = false;
  double          m_timestampPeriodNs = 1.0;
  uint64_t        m_timestampMask     = ~uint64_t{0};

  struct PendingZone {
    std::string name;
    uint32_t    beginIdx;
    uint32_t    endIdx;
  };
  std::vector<std::vector<PendingZone>> m_pending;

  struct ZoneStats {
    std::deque<double> recent;
    double             avgMs = 0.0;
  };
  std::map<std::string, ZoneStats> m_stats;

  std::deque<double> m_frameTimesMs;
  std::vector<float> m_graphBuffer;
  double             m_avgFrameTimeMs = 0.0;

  Impl(vk::Device, vk::PhysicalDevice, uint32_t flightFrames);
  ~Impl();

  void beginFrame(double dtSeconds, uint32_t frameSlot);
  std::pair<uint32_t, uint32_t> allocateZone(std::string name);
};

}
