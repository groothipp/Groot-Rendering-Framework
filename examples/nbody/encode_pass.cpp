#include "nbody.hpp"

#include <format>

EncodePass::EncodePass(GRF& grf, const std::string& shadersTreeFolder) {
  m_encodeShader = grf.compileShader(ShaderType::Compute, std::format("{}/encode.gsl", shadersTreeFolder));
  m_encodePipeline = grf.createComputePipeline(m_encodeShader);

  m_mortonRing = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticleCount);
  m_indexRing = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticleCount);
}

Buffer& EncodePass::mortonBuffer(u32 frameIndex) {
  return m_mortonRing[frameIndex];
}

Buffer& EncodePass::indexBuffer(u32 frameIndex) {
  return m_indexRing[frameIndex];
}

void EncodePass::dispatch(CommandBuffer& cmd, u32 frameIndex, Data data) {
  data.mortonBufAddr = m_mortonRing[frameIndex].address();
  data.indexBufAddr = m_indexRing[frameIndex].address();

  auto [x, y, z] = m_encodeShader.threadGroup();

  cmd.bindPipeline(m_encodePipeline);
  cmd.push(data);
  cmd.dispatch((data.particleCount + x - 1) / x);
}