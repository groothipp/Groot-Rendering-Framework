#pragma once

#include "./pipelines.hpp"
#include "./resources.hpp"
#include "./ring.hpp"
#include "./shader.hpp"
#include "./sync.hpp"
#include "./types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

namespace grf {

class GRF {
  class Impl;
  std::unique_ptr<Impl> m_impl;

public:
  explicit GRF(const Settings& settings = Settings{});
  ~GRF();

  bool running(std::function<bool()> endCond = [](){ return false; }) const;
  std::pair<uint32_t, double> beginFrame();
  void waitForResourceUpdates();
  SwapchainImage nextSwapchainImage();
  void waitFences(const std::vector<Fence>&);
  void endFrame();

  Shader compileShader(ShaderType, const std::string&);

  Buffer createBuffer(BufferIntent, std::size_t);
  Tex2D createTex2D(Format, uint32_t, uint32_t);
  Tex3D createTex3D(Format, uint32_t, uint32_t, uint32_t);
  Cubemap createCubemap(Format, uint32_t, uint32_t);
  Img2D createImg2D(Format, uint32_t, uint32_t);
  Img3D createImg3D(Format, uint32_t, uint32_t, uint32_t);
  Sampler createSampler(const SamplerSettings&);
  Ring<Buffer> createBufferRing(BufferIntent, std::size_t);
  Ring<Img2D> createImg2DRing(Format, uint32_t, uint32_t);
  Ring<Img3D> createImg3DRing(Format, uint32_t, uint32_t, uint32_t);
  ComputePipeline createComputePipeline(Shader);

  Fence createFence(bool signaled = false);
  Semaphore createSemaphore();
  Ring<Fence> createFenceRing(bool signaled = false);
  Ring<Semaphore> createSemaphoreRing();
};

ImageData readImage(const std::string&);

}