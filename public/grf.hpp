#pragma once

#include "./cmd.hpp"
#include "./gui.hpp"
#include "./input.hpp"
#include "./math.hpp"
#include "./pipelines.hpp"
#include "./profiler.hpp"
#include "./resources.hpp"
#include "./ring.hpp"
#include "./shader.hpp"
#include "./sync.hpp"
#include "./types.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <span>
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
  std::pair<uint32_t, float> beginFrame();
  std::pair<uint32_t, uint32_t> screenDims() const;
  void waitForResourceUpdates();
  void resizeCallback(std::function<void(uint32_t, uint32_t)> callback);

  void wait(const Sync&);
  void wait(std::span<const Sync>);
  Ring<Sync> createSyncRing();

  Input&    input();
  GUI&      gui();
  Profiler& profiler();
  SwapchainImage nextSwapchainImage();
  void present(const SwapchainImage&, std::span<const Sync> waits = {});
  void endFrame();

  Shader compileShader(ShaderType, const std::string&);

  Buffer createBuffer(BufferIntent, std::size_t);
  Tex2D createTex2D(Format, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Tex3D createTex3D(Format, uint32_t, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Cubemap createCubemap(Format, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Img2D createImg2D(Format, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Img3D createImg3D(Format, uint32_t, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Sampler createSampler(const SamplerSettings&);
  DepthImage createDepthImage(Format, uint32_t, uint32_t, bool sampled = false);
  Ring<Buffer> createBufferRing(BufferIntent, std::size_t);
  Ring<Img2D> createImg2DRing(Format, uint32_t, uint32_t, uint32_t mipLevels = 1);
  Ring<Img3D> createImg3DRing(Format, uint32_t, uint32_t, uint32_t, uint32_t mipLevels = 1);
  ComputePipeline createComputePipeline(Shader);
  GraphicsPipeline createGraphicsPipeline(Shader vertex, Shader fragment, const GraphicsPipelineSettings&);

  Ring<CommandBuffer> createCmdRing(QueueType);
  Sync submit(
    const CommandBuffer&,
    std::span<const Sync> waits = {}
  );
};

std::future<ImageData> readImage(const std::string&);

}