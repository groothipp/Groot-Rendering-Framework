#include "nbody.hpp"

#include <format>

LVBHTree::LVBHTree(GRF& grf, const std::string& shadersFolderName)
: m_boundsPass(BoundsPass(grf, std::format("{}/{}", SHADERS, shadersFolderName))),
  m_encodePass(EncodePass(grf, std::format("{}/{}", SHADERS, shadersFolderName))),
  m_radixSortPass(RadixSortPass(grf, std::format("{}/{}", SHADERS, shadersFolderName)))
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
}