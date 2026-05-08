#include "nbody.hpp"

#include <format>

BoundsPass::BoundsPass(GRF& grf, const std::string& shadersTreeFolder) : m_grf(grf) {
  std::string dir = std::format("{}/bounds", shadersTreeFolder);

  m_tileShader = m_grf.compileShader(ShaderType::Compute, std::format("{}/tile.gsl", dir));
  m_sceneShader = m_grf.compileShader(ShaderType::Compute, std::format("{}/scene.gsl", dir));

  m_tilePipeline = m_grf.createComputePipeline(m_tileShader);
  m_scenePipeline = m_grf.createComputePipeline(m_sceneShader);

  m_boundsRing = m_grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4));
}

Buffer& BoundsPass::boundsBuffer(u32 frameIndex) {
  return m_boundsRing[frameIndex];
}

void BoundsPass::dispatch(CommandBuffer& cmd, u32 frameIndex, u32 particleCount, u64 posBufAddr) {
  auto [x, y, z] = m_tileShader.threadGroup();
  u32 tiles = (particleCount + x - 1) / x;

  Buffer scratch = m_grf.createBuffer(BufferIntent::GPUOnly, sizeof(u32) * tiles);

  cmd.bindPipeline(m_tilePipeline);
  cmd.push(TileData{
    .posBufAddr     = posBufAddr,
    .scratchBufAddr = scratch.address(),
    .particleCount  = particleCount
  });
  cmd.dispatch(tiles);

  cmd.barrier(scratch, BufferAccess::ShaderWrite, BufferAccess::ShaderWrite);

  cmd.bindPipeline(m_scenePipeline);
  cmd.push(SceneData{
    .scratchBufAddr = scratch.address(),
    .boundsBufAddr  = m_boundsRing[frameIndex].address(),
    .tileCount      = tiles
  });
  cmd.dispatch(1);
}

LVBHTree::LVBHTree(GRF& grf, const std::string& shadersFolderName)
: m_boundsPass(BoundsPass(grf, std::format("{}/{}", SHADERS, shadersFolderName)))
{}

void LVBHTree::construct(CommandBuffer& cmd, u32 frameIndex, u32 particleCount, u64 posBufAddr) {
  m_boundsPass.dispatch(cmd, frameIndex, particleCount, posBufAddr);
}