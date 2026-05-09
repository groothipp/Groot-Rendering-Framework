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

RadixSortPass::RadixSortPass(GRF& grf, const std::string& shadersTreeFolder) : m_grf(grf) {
  std::string dir = std::format("{}/radix_sort", shadersTreeFolder);

  m_histShader = m_grf.compileShader(ShaderType::Compute, std::format("{}/histogram.gsl", dir));
  m_scatterShader = m_grf.compileShader(ShaderType::Compute, std::format("{}/scatter.gsl", dir));

  m_histPipeline = m_grf.createComputePipeline(m_histShader);
  m_scatterPipeline = m_grf.createComputePipeline(m_scatterShader);

  m_histRing = m_grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_numWorkgroups * g_bins);
}

void RadixSortPass::dispatch(
  CommandBuffer& cmd, u32 frameIndex, u32 particleCount, u64 mortonBufAddr, u64 indexBufAddr
) {
  auto [blocks, _, _] = m_histShader.threadGroup();
  const u32 blocksPerWorkgroup =
    (particleCount + g_numWorkgroups * blocks - 1) / (g_numWorkgroups * blocks);

  Buffer mortonScratch = m_grf.createBuffer(BufferIntent::GPUOnly, sizeof(u32) * particleCount);
  Buffer indexScratch = m_grf.createBuffer(BufferIntent::GPUOnly, sizeof(u32) * particleCount);

  Buffer& histBuf = m_histRing[frameIndex];

  u64 keysInAddr = mortonBufAddr;
  u64 keysOutAddr = mortonScratch.address();
  u64 valsInAddr = indexBufAddr;
  u64 valsOutAddr = indexScratch.address();

  for (u32 pass = 0; pass < 4; ++pass) {
    u32 shift = pass * 8;

    cmd.bindPipeline(m_histPipeline);
    cmd.push(HistData{
      .keysBufAddr        = keysInAddr,
      .histBufAddr        = histBuf.address(),
      .particleCount      = particleCount,
      .shift              = shift,
      .workgroups         = g_numWorkgroups,
      .blocksPerWorkgroup = blocksPerWorkgroup
    });
    cmd.dispatch(g_numWorkgroups);

    cmd.barrier(histBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    cmd.bindPipeline(m_scatterPipeline);
    cmd.push(ScatterData{
      .keysInAddr         = keysInAddr,
      .valsInAddr         = valsInAddr,
      .keysOutAddr        = keysOutAddr,
      .valsOutAddr        = valsOutAddr,
      .particleCount      = particleCount,
      .shift              = shift,
      .workgroups         = g_numWorkgroups,
      .blocksPerWorkgroup = blocksPerWorkgroup
    });
    cmd.dispatch(g_numWorkgroups);

    cmd.barrier();

    std::swap(keysInAddr, keysOutAddr);
    std::swap(valsInAddr, valsOutAddr);
  }
}

BuildPass::BuildPass(GRF& grf, const std::string& shadersTreeFolder) {
  m_buildShader = grf.compileShader(ShaderType::Compute, std::format("{}/build.gsl", shadersTreeFolder));
  m_buildPipeline = grf.createComputePipeline(m_buildShader);

  m_parentRing = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticleCount);
  m_childRing = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(uvec2) * g_maxParticleCount);
}

Buffer& BuildPass::childBuffer(u32 frameIndex) {
  return m_childRing[frameIndex];
}

Buffer& BuildPass::parentBuffer(u32 frameIndex) {
  return m_parentRing[frameIndex];
}

void BuildPass::dispatch(CommandBuffer& cmd, u32 frameIndex, u32 particleCount, u64 mortonBufAddr) {
  auto [x, y, z] = m_buildShader.threadGroup();

  cmd.bindPipeline(m_buildPipeline);
  cmd.push(Data{
    .mortonBufAddr =  mortonBufAddr,
    .parentBufAddr = m_parentRing[frameIndex].address(),
    .childBufAddr  = m_childRing[frameIndex].address(),
    .particleCount = particleCount
  });
  cmd.dispatch((particleCount + x - 1) / x);
}

LVBHTree::LVBHTree(GRF& grf, const std::string& shadersFolderName)
: m_boundsPass(BoundsPass(grf, std::format("{}/{}", SHADERS, shadersFolderName))),
  m_encodePass(EncodePass(grf, std::format("{}/{}", SHADERS, shadersFolderName))),
  m_radixSortPass(RadixSortPass(grf, std::format("{}/{}", SHADERS, shadersFolderName))),
  m_buildPass(BuildPass(grf, std::format("{}/{}", SHADERS, shadersFolderName)))
{}

void LVBHTree::construct(CommandBuffer& cmd, u32 frameIndex, u32 particleCount, u64 posBufAddr) {
  m_boundsPass.dispatch(cmd, frameIndex, particleCount, posBufAddr);

  Buffer& boundsBuf = m_boundsPass.boundsBuffer(frameIndex);
  cmd.barrier(boundsBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

  m_encodePass.dispatch(cmd, frameIndex, EncodePass::Data{
    .posBufAddr     = posBufAddr,
    .boundsBufAddr  = boundsBuf.address(),
    .particleCount  = particleCount
  });

  Buffer& mortonBuf = m_encodePass.mortonBuffer(frameIndex);
  Buffer& indexBuf = m_encodePass.indexBuffer(frameIndex);

  cmd.barrier(mortonBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
  cmd.barrier(indexBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

  m_radixSortPass.dispatch(cmd, frameIndex, particleCount, mortonBuf.address(), indexBuf.address());
  m_buildPass.dispatch(cmd, frameIndex, particleCount, mortonBuf.address());
}