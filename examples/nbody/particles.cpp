#include "nbody.hpp"

#include <format>
#include <numbers>

Particles::Particles(GRF& grf, const std::string& shadersFolderName, u32 flightFrames) : m_grf(grf) {
  const std::string dir = std::format("{}/{}", SHADERS, shadersFolderName);

  m_vertShader = m_grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", dir));
  m_fragShader = m_grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", dir));

  BlendState alphaBlend{
    .enable         = true,
    .srcColorFactor = BlendFactor::SrcAlpha,
    .dstColorFactor = BlendFactor::OneMinusSrcAlpha
  };

  m_pipeline = m_grf.createGraphicsPipeline(m_vertShader, m_fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { alphaBlend }
  });

  m_posRing = m_grf.createBufferRing(BufferIntent::FrequentUpdate, sizeof(vec2) * g_maxParticleCount);
  m_velRing = m_grf.createBufferRing(BufferIntent::FrequentUpdate, sizeof(vec2) * g_maxParticleCount);

  reset(flightFrames);
}

std::pair<Buffer&, Buffer&> Particles::buffers(u32 frameIndex) {
  return { m_posRing[frameIndex], m_velRing[frameIndex] };
}

void Particles::render(CommandBuffer& cmd, u32 frameIndex, Data particleData) {
  auto [posBuf, velBuf] = buffers(frameIndex);
  particleData.posBufAddr = posBuf.address();
  particleData.velBufAddr = velBuf.address();
  particleData.particleRadius = g_particleRadius;

  cmd.bindPipeline(m_pipeline);
  cmd.push(particleData);
  cmd.draw(6 * particleData.particleCount);
}

void Particles::reset(u32 flightFrames) {
  std::vector<vec2> zeros(g_maxParticleCount, vec2(0.0));
  for (u32 i = 0; i < flightFrames; ++i) {
    m_posRing[i].write(zeros);
    m_velRing[i].write(zeros);
  }
}

u32 Particles::spawn(vec2 center, u32 particleCount, u32 flightFrames, f32 spawnRadius) {
  static constexpr f32  pi            = std::numbers::pi_v<f32>;
  static const u32      spawnDensity  = 150;
  static const f32      minSep        = 2.0f * g_particleRadius;
  static const f32      minSepSq      = minSep * minSep;
  static const u32      maxAttempts   = 50;

  u32 target = spawnDensity * pi * spawnRadius * spawnRadius;
  target = std::min(target, g_maxParticleCount - particleCount);

  std::vector<vec2> positions;
  positions.reserve(target);

  RealDist dist01(0.0, 1.0);
  RealDist distTheta(0.0, 2.0 * pi);

  for (u32 i = 0; i < target; ++i) {
    bool placed = false;
    for (u32 attempt = 0; attempt < maxAttempts; ++attempt) {
      f32 r = spawnRadius * std::sqrt(dist01(m_rng));
      f32 theta = distTheta(m_rng);
      vec2 candidate = center - vec2(r * cos(theta), r * sin(theta));

      bool valid = true;
      for (const auto& p : positions) {
        vec2 d = candidate - p;
        if (d.x * d.x + d.y * d.y < minSepSq) {
          valid = false;
          break;
        }
      }

      if (valid) {
        positions.push_back(candidate);
        placed = true;
        break;
      }
    }
    if (!placed) break;
  }

  for (u32 i = 0; i < flightFrames; ++i)
    m_posRing[i].write(positions, sizeof(vec2) * particleCount);

  return positions.size();
}