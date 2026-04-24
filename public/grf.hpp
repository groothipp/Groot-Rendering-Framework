#pragma once

#include "./buffer.hpp"
#include "./enums.hpp"
#include "./img.hpp"
#include "./sampler.hpp"
#include "./shader.hpp"
#include "./structs.hpp"
#include "./swapchain_image.hpp"
#include "./sync.hpp"
#include "./tex.hpp"

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
  void endFrame();

  Shader compileShader(ShaderType, const std::string&);

  Buffer createBuffer(BufferIntent, std::size_t);
  Tex2D createTex2D(Format, uint32_t, uint32_t);
  Tex3D createTex3D(Format, uint32_t, uint32_t, uint32_t);
  Cubemap createCubemap(Format, uint32_t, uint32_t);
  Img2D createImg2D(Format, uint32_t, uint32_t);
  Img3D createImg3D(Format, uint32_t, uint32_t, uint32_t);
  Sampler createSampler(const SamplerSettings&);

  Fence createFence(bool signaled = false);
  Semaphore createSemaphore();
  void waitFences(const std::vector<Fence>&);
};

ImageData readImage(const std::string&);

}