#include "nbody.hpp"

#include <format>
#include <numbers>

Particles::Particles(GRF& grf, const std::string& shadersFolderName, u32 flightFrames) : m_grf(grf) {
  const std::string dir = std::format("{}/{}", SHADERS, shadersFolderName);

  m_vertShader = m_grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", dir));
  m_fragShader = m_grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", dir));

  BlendState alphaBlend{
    .srcColorFactor = BlendFactor::SrcAlpha,
    .dstColorFactor = BlendFactor::OneMinusSrcAlpha,
    .alphaOp        = BlendOp::Add
  };

  m_pipeline = m_grf.createGraphicsPipeline(m_vertShader, m_fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { alphaBlend }
  });

  m_posRing = m_grf.createBufferRing(BufferIntent::FrequentUpdate, sizeof(vec2) * g_maxParticleCount);
  m_velRing = m_grf.createBufferRing(BufferIntent::SingleUpdate, sizeof(vec2) * g_maxParticleCount);

  reset(flightFrames);
}

std::pair<Buffer&, Buffer&> Particles::prevBuffers(u32 prevFrameIndex) {
  return { m_posRing[prevFrameIndex], m_velRing[prevFrameIndex] };
}

std::pair<Buffer&, Buffer&> Particles::buffers(u32 frameIndex) {
  return { m_posRing[frameIndex], m_velRing[frameIndex] };
}

void Particles::render(CommandBuffer& cmd, u32 frameIndex, Data particleData) {
  static const f32 particleRadius = 0.008;

  auto [posBuf, velBuf] = buffers(frameIndex);
  particleData.posBufAddr = posBuf.address();
  particleData.velBufAddr = velBuf.address();
  particleData.particleRadius = particleRadius;

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

u32 Particles::spawn(vec2 center, u32 particleCount, u32 flightFrames) {
  static constexpr f32  pi            = std::numbers::pi_v<f32>;
  static const u32      spawnDensity  = 500;
  static const u32      minSpawnCount = 10;
  static const u32      maxSpawnCount = 100;
  static const f32      minRadius     = std::sqrt(minSpawnCount / (spawnDensity * pi));
  static const f32      maxRadius     = std::sqrt(maxSpawnCount / (spawnDensity * pi));

  if (particleCount >= g_maxParticleCount) return 0;

  const float radius = RealDist(minRadius, maxRadius)(m_rng);

  u32 count = spawnDensity * pi * radius * radius;
  count = std::min(count, g_maxParticleCount - particleCount);

  std::vector<vec2> positions;
  positions.reserve(count);

  for (u32 i = 0; i < count; ++i) {
    f32 r = std::sqrt(RealDist(0.0, radius)(m_rng));
    f32 theta = RealDist(0.0, 2.0 * pi)(m_rng);

    positions.emplace_back(center + vec2(r * cos(theta), r * sin(theta)));
  }

  for (u32 i = 0; i < flightFrames; ++i)
    m_posRing[i].write(positions, sizeof(vec2) * particleCount);

  return positions.size();
}