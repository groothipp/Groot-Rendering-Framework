#include "nbody.hpp"

#include <format>

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