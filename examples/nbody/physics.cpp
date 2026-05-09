#include "nbody.hpp"

#include <format>

Physics::Physics(GRF& grf, const std::string& shadersFolderName) {
  std::string dir = std::format("{}/{}", SHADERS, shadersFolderName);

  m_forceShader = grf.compileShader(ShaderType::Compute, std::format("{}/force.gsl", dir));
  m_forcePipeline = grf.createComputePipeline(m_forceShader);

  m_comRing = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4) * 2 * (g_maxParticleCount - 1));
}

Buffer& Physics::comBuffer(u32 frameIndex) {
  return m_comRing[frameIndex];
}

void Physics::dispatch(CommandBuffer& cmd, u32 frameIndex, Data data) {
  data.comBufAddr = m_comRing[frameIndex].address();

  cmd.bindPipeline(m_forcePipeline);
  cmd.push(data);

  auto [x, y, z] = m_forceShader.threadGroup();
  cmd.dispatch((data.particleCount + x - 1) / x);
}