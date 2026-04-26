#include "public/sync.hpp"
#include "public/types.hpp"

#include "internal/allocator.hpp"
#include "internal/cmd.hpp"
#include "internal/descriptor_heap.hpp"
#include "internal/grf.hpp"
#include "internal/log.hpp"
#include "internal/sync.hpp"
#include "internal/pipelines.hpp"

#include "external/stb/stb_image.h"
#include "vulkan/vulkan.hpp"

#include <cstring>

namespace grf {

GRF::GRF(const Settings& settings) {
  m_impl = std::make_unique<Impl>(settings);
}

GRF::~GRF() = default;

bool GRF::running(std::function<bool()> endCond) const {
  return !glfwWindowShouldClose(m_impl->m_window) && !endCond();
}

std::pair<uint32_t, double> GRF::beginFrame() {
  auto now = GRF::Impl::Clock::now();

  if (m_impl->m_startTime == GRF::Impl::TimePoint{}) [[unlikely]]
    m_impl->m_startTime = now;

  if (m_impl->m_endTime == GRF::Impl::TimePoint{}) [[unlikely]]
    m_impl->m_endTime = now;

  m_impl->m_input->m_impl->onPollBegin();
  glfwPollEvents();
  m_impl->m_gui->m_impl->beginFrame();
  m_impl->m_resourceManager->beginUpdates();

  double frameTime = GRF::Impl::Duration(m_impl->m_endTime - m_impl->m_startTime).count();
  m_impl->m_startTime = now;

  m_impl->m_profiler->m_impl->beginFrame(frameTime, m_impl->m_frameIndex);

  return { m_impl->m_frameIndex, frameTime };
}

Input& GRF::input() {
  return *m_impl->m_input;
}

GUI& GRF::gui() {
  return *m_impl->m_gui;
}

Profiler& GRF::profiler() {
  return *m_impl->m_profiler;
}

SwapchainImage GRF::nextSwapchainImage(const Semaphore& signalOnAcquire) {
  auto [res, index] = m_impl->m_device.acquireNextImageKHR(
    m_impl->m_swapchain, m_impl->g_timeout,
    signalOnAcquire.m_impl->m_semaphore, nullptr
  );
  if (res != vk::Result::eSuccess)
    GRF_PANIC("Failed to get next swapchain image: {}", vk::to_string(res));
  return SwapchainImage(m_impl->m_swapchainImages[index]);
}

void GRF::present(const SwapchainImage& image, std::span<const Semaphore> waits) {
  std::vector<vk::Semaphore> waitSemaphores;
  waitSemaphores.reserve(waits.size());
  for (const auto& s : waits)
    waitSemaphores.push_back(s.m_impl->m_semaphore);

  uint32_t index = image.m_impl->m_index;

  auto res = m_impl->m_graphicsQueue.queue.presentKHR(vk::PresentInfoKHR{
    .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
    .pWaitSemaphores    = waitSemaphores.data(),
    .swapchainCount     = 1,
    .pSwapchains        = &m_impl->m_swapchain,
    .pImageIndices      = &index
  });

  if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR)
    GRF_PANIC("Failed to present swapchain image: {}", vk::to_string(res));
}

void GRF::waitForResourceUpdates() {
  m_impl->m_resourceManager->waitForUpdates();
  m_impl->m_endTime = GRF::Impl::Clock::now();
}

void GRF::endFrame() {
  m_impl->m_frameIndex = (m_impl->m_frameIndex + 1) % m_impl->m_settings.flightFrames;
  m_impl->m_endTime = GRF::Impl::Clock::now();
  m_impl->m_resourceManager->drain();
}

Shader GRF::compileShader(ShaderType type, const std::string& path) {
  return m_impl->m_shaderManager->compile(type, path);
}

Buffer GRF::createBuffer(BufferIntent intent, std::size_t size) {
  return m_impl->m_allocator->allocateBuffer(size, intent);
}

Tex2D GRF::createTex2D(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e2D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e2D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addTex2D(img);

  return Tex2D(img);
}

Tex3D GRF::createTex3D(Format format, uint32_t width, uint32_t height, uint32_t depth) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e3D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height,
    .depth  = depth
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e3D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addTex3D(img);

  return Tex3D(img);
}

Cubemap GRF::createCubemap(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type       = vk::ImageType::e2D,
    .format     = static_cast<vk::Format>(format),
    .usage      = vk::ImageUsageFlagBits::eSampled      |
                  vk::ImageUsageFlagBits::eTransferSrc  |
                  vk::ImageUsageFlagBits::eTransferDst,
    .width      = width,
    .height     = height,
    .isCubemap  = true
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::eCube,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 6
    }
  });

  m_impl->m_descriptorHeap->addCubemap(img);

  return Cubemap(img);
}

Img2D GRF::createImg2D(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type = vk::ImageType::e2D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eStorage      |
              vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e2D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addImg2D(img);

  return Img2D(img);
}

Img3D GRF::createImg3D(Format format, uint32_t width, uint32_t height, uint32_t depth) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e3D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eStorage      |
              vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height,
    .depth  = depth
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e3D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addImg3D(img);

  return Img3D(img);
}

Sampler GRF::createSampler(const SamplerSettings& settings) {
  auto impl = m_impl->m_allocator->createSampler(settings);
  m_impl->m_descriptorHeap->addSampler(impl);
  return Sampler(impl);
}

DepthImage GRF::createDepthImage(Format format, uint32_t width, uint32_t height, bool sampled) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
  if (sampled) usage |= vk::ImageUsageFlagBits::eSampled;

  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e2D,
    .format = static_cast<vk::Format>(format),
    .usage  = usage,
    .width  = width,
    .height = height
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e2D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eDepth,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  if (sampled)
    m_impl->m_descriptorHeap->addTex2D(img);

  return DepthImage(img);
}

Ring<Buffer> GRF::createBufferRing(BufferIntent intent, std::size_t size) {
  auto ring = Ring<Buffer>(m_impl->m_settings.flightFrames);

  for (int i = 0; i < m_impl->m_settings.flightFrames; ++i) {
    auto buf = createBuffer(intent, size);
    ring.m_objs.push_back(buf);
  }

  return ring;
}

Ring<Img2D> GRF::createImg2DRing(Format format, uint32_t width, uint32_t height) {
  auto ring = Ring<Img2D>(m_impl->m_settings.flightFrames);

  for (int32_t i = 0; i < m_impl->m_settings.flightFrames; ++i) {
    auto img = createImg2D(format, width, height);
    ring.m_objs.push_back(img);
  }

  return ring;
}

Ring<Img3D> GRF::createImg3DRing(Format format, uint32_t width, uint32_t height, uint32_t depth) {
  auto ring = Ring<Img3D>(m_impl->m_settings.flightFrames);

  for (int32_t i = 0; i < m_impl->m_settings.flightFrames; ++i) {
    auto img = createImg3D(format, width, height, depth);
    ring.m_objs.push_back(img);
  }

  return ring;
}

GraphicsPipeline GRF::createGraphicsPipeline(
  Shader vertex, Shader fragment, const GraphicsPipelineSettings& settings
) {
  if (vertex.type() != ShaderType::Vertex)
    GRF_PANIC("createGraphicsPipeline: first shader must be a vertex shader");
  if (fragment.type() != ShaderType::Fragment)
    GRF_PANIC("createGraphicsPipeline: second shader must be a fragment shader");
  if (settings.colorFormats.empty())
    GRF_PANIC("createGraphicsPipeline: at least one color format is required");
  if (!settings.blends.empty() && settings.blends.size() != settings.colorFormats.size())
    GRF_PANIC("createGraphicsPipeline: blends.size() ({}) must equal colorFormats.size() ({})",
              settings.blends.size(), settings.colorFormats.size());

  std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
    vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eVertex,
      .module = m_impl->m_shaderManager->getModule(vertex),
      .pName  = "main"
    },
    vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eFragment,
      .module = m_impl->m_shaderManager->getModule(fragment),
      .pName  = "main"
    }
  };

  vk::PipelineVertexInputStateCreateInfo vertexInput{};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
    .topology = static_cast<vk::PrimitiveTopology>(settings.topology)
  };

  vk::PipelineViewportStateCreateInfo viewport{
    .viewportCount  = 1,
    .scissorCount   = 1
  };

  vk::PipelineRasterizationStateCreateInfo rasterization{
    .polygonMode  = static_cast<vk::PolygonMode>(settings.polygonMode),
    .cullMode     = static_cast<vk::CullModeFlagBits>(settings.cullMode),
    .frontFace    = static_cast<vk::FrontFace>(settings.frontFace),
    .lineWidth    = 1.0f
  };

  vk::PipelineMultisampleStateCreateInfo multisample{
    .rasterizationSamples = static_cast<vk::SampleCountFlagBits>(settings.sampleCount)
  };

  const bool hasDepth = settings.depthFormat != Format::undefined;
  vk::PipelineDepthStencilStateCreateInfo depthStencil{
    .depthTestEnable  = hasDepth && settings.depthTest,
    .depthWriteEnable = hasDepth && settings.depthWrite,
    .depthCompareOp   = static_cast<vk::CompareOp>(settings.depthCompareOp)
  };

  std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(settings.colorFormats.size());
  constexpr vk::ColorComponentFlags allChannels =
    vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  for (std::size_t i = 0; i < blendAttachments.size(); ++i) {
    if (settings.blends.empty()) {
      blendAttachments[i] = vk::PipelineColorBlendAttachmentState{
        .blendEnable    = false,
        .colorWriteMask = allChannels
      };
    } else {
      const auto& b = settings.blends[i];
      blendAttachments[i] = vk::PipelineColorBlendAttachmentState{
        .blendEnable          = b.enable,
        .srcColorBlendFactor  = static_cast<vk::BlendFactor>(b.srcColorFactor),
        .dstColorBlendFactor  = static_cast<vk::BlendFactor>(b.dstColorFactor),
        .colorBlendOp         = static_cast<vk::BlendOp>(b.colorOp),
        .srcAlphaBlendFactor  = static_cast<vk::BlendFactor>(b.srcAlphaFactor),
        .dstAlphaBlendFactor  = static_cast<vk::BlendFactor>(b.dstAlphaFactor),
        .alphaBlendOp         = static_cast<vk::BlendOp>(b.alphaOp),
        .colorWriteMask       = allChannels
      };
    }
  }

  vk::PipelineColorBlendStateCreateInfo colorBlend{
    .attachmentCount  = static_cast<uint32_t>(blendAttachments.size()),
    .pAttachments     = blendAttachments.data()
  };

  std::array<vk::DynamicState, 2> dynamicStates = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
  };
  vk::PipelineDynamicStateCreateInfo dynamic{
    .dynamicStateCount  = static_cast<uint32_t>(dynamicStates.size()),
    .pDynamicStates     = dynamicStates.data()
  };

  std::vector<vk::Format> colorFormats;
  colorFormats.reserve(settings.colorFormats.size());
  for (auto f : settings.colorFormats)
    colorFormats.emplace_back(static_cast<vk::Format>(f));

  vk::PipelineRenderingCreateInfo rendering{
    .colorAttachmentCount     = static_cast<uint32_t>(colorFormats.size()),
    .pColorAttachmentFormats  = colorFormats.data(),
    .depthAttachmentFormat    = static_cast<vk::Format>(settings.depthFormat)
  };

  auto res = m_impl->m_device.createGraphicsPipeline(nullptr, vk::GraphicsPipelineCreateInfo{
    .pNext                = &rendering,
    .stageCount           = static_cast<uint32_t>(stages.size()),
    .pStages              = stages.data(),
    .pVertexInputState    = &vertexInput,
    .pInputAssemblyState  = &inputAssembly,
    .pViewportState       = &viewport,
    .pRasterizationState  = &rasterization,
    .pMultisampleState    = &multisample,
    .pDepthStencilState   = &depthStencil,
    .pColorBlendState     = &colorBlend,
    .pDynamicState        = &dynamic,
    .layout               = m_impl->m_pipelineLayout
  });

  if (res.result != vk::Result::eSuccess)
    GRF_PANIC("Failed to create graphics pipeline: {}", vk::to_string(res.result));

  auto pipeline = std::make_shared<Pipeline>(
    std::weak_ptr<ResourceManager>(m_impl->m_resourceManager),
    m_impl->m_pipelineLayout, res.value
  );

  return GraphicsPipeline(pipeline);
}

ComputePipeline GRF::createComputePipeline(Shader shader) {
  auto res = m_impl->m_device.createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
    .stage  = {
      .stage  = vk::ShaderStageFlagBits::eCompute,
      .module = m_impl->m_shaderManager->getModule(shader),
      .pName  = "main"
    },
    .layout = m_impl->m_pipelineLayout,
  });

  if (!res.has_value())
    GRF_PANIC("Failed to create compute pipeline");

  auto pipeline = std::make_shared<Pipeline>(
    std::weak_ptr<ResourceManager>(m_impl->m_resourceManager),
    m_impl->m_pipelineLayout, res.value
  );

  return ComputePipeline(pipeline);
}

Fence GRF::createFence(bool signaled) {
  auto impl = std::make_shared<Fence::Impl>(
    std::weak_ptr<ResourceManager>(m_impl->m_resourceManager),
    m_impl->m_device.createFence(vk::FenceCreateInfo{
      .flags = signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlags()
    })
  );

  return Fence(impl);
}

Semaphore GRF::createSemaphore() {
  auto impl = std::make_shared<Semaphore::Impl>(
    std::weak_ptr<ResourceManager>(m_impl->m_resourceManager),
    m_impl->m_device.createSemaphore({})
  );

  return Semaphore(impl);
}

Ring<Fence> GRF::createFenceRing(bool signaled) {
  auto ring = Ring<Fence>(m_impl->m_settings.flightFrames);

  for (int32_t i = 0; i < m_impl->m_settings.flightFrames; ++i)
    ring.m_objs.emplace_back(createFence(signaled));

  return ring;
}

Ring<Semaphore> GRF::createSemaphoreRing() {
  auto ring = Ring<Semaphore>(m_impl->m_settings.flightFrames);

  for (int32_t i = 0; i < m_impl->m_settings.flightFrames; ++i)
    ring.m_objs.emplace_back(createSemaphore());

  return ring;
}

Ring<CommandBuffer> GRF::createCmdRing(QueueType qt) {
  auto ring = Ring<CommandBuffer>(m_impl->m_settings.flightFrames);

  vk::CommandPool pool = m_impl->m_commandPools[static_cast<size_t>(qt)];

  std::vector<vk::CommandBuffer> buffers = m_impl->m_device.allocateCommandBuffers(
    vk::CommandBufferAllocateInfo{
      .commandPool        = pool,
      .level              = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = m_impl->m_settings.flightFrames
    }
  );

  for (vk::CommandBuffer buf : buffers) {
    auto impl = std::make_shared<CommandBuffer::Impl>(
      std::weak_ptr<ResourceManager>(m_impl->m_resourceManager),
      m_impl->m_device,
      pool,
      buf,
      m_impl->m_pipelineLayout,
      m_impl->m_descriptorHeap->set(),
      m_impl->m_swapchainExtent,
      m_impl->m_pushConstantSize,
      qt,
      m_impl->m_profiler ? m_impl->m_profiler->m_impl.get() : nullptr
    );
    auto cmd = CommandBuffer(impl);
    ring.m_objs.push_back(cmd);
  }

  return ring;
}

void GRF::submit(
  const CommandBuffer& cmd,
  std::span<const Semaphore> waits,
  std::span<const Semaphore> signals,
  std::optional<Fence> signalFence
) {
  Queue& q = m_impl->m_resourceManager->queue(cmd.m_impl->m_queueType);

  std::vector<vk::Semaphore>          waitSemaphores;
  std::vector<vk::PipelineStageFlags> waitStages;
  std::vector<uint64_t>               waitValues;
  for (const auto& s : waits) {
    waitSemaphores.push_back(s.m_impl->m_semaphore);
    waitStages.push_back(vk::PipelineStageFlagBits::eAllCommands);
    waitValues.push_back(0);
  }

  std::vector<vk::Semaphore>  signalSemaphores;
  std::vector<uint64_t>       signalValues;
  signalSemaphores.push_back(q.timeline);
  signalValues.push_back(cmd.m_impl->m_reservedValue);
  for (const auto& s : signals) {
    signalSemaphores.push_back(s.m_impl->m_semaphore);
    signalValues.push_back(0);
  }

  vk::TimelineSemaphoreSubmitInfo timelineInfo{
    .waitSemaphoreValueCount    = static_cast<uint32_t>(waitValues.size()),
    .pWaitSemaphoreValues       = waitValues.data(),
    .signalSemaphoreValueCount  = static_cast<uint32_t>(signalValues.size()),
    .pSignalSemaphoreValues     = signalValues.data()
  };

  q.queue.submit(vk::SubmitInfo{
    .pNext                  = &timelineInfo,
    .waitSemaphoreCount     = static_cast<uint32_t>(waitSemaphores.size()),
    .pWaitSemaphores        = waitSemaphores.data(),
    .pWaitDstStageMask      = waitStages.data(),
    .commandBufferCount     = 1,
    .pCommandBuffers        = &cmd.m_impl->m_buffer,
    .signalSemaphoreCount   = static_cast<uint32_t>(signalSemaphores.size()),
    .pSignalSemaphores      = signalSemaphores.data()
  }, signalFence.has_value() ? signalFence->m_impl->m_fence : nullptr);

  cmd.m_impl->m_reservedValue = 0;
}

void GRF::waitFences(const std::vector<Fence>& fences) {
  if (fences.empty()) {
    log::warning("Attempted to wait on 0 fences");
    return;
  }

  std::vector<vk::Fence> f;
  for (const auto& fence : fences)
    f.emplace_back(fence.m_impl->m_fence);

  if (m_impl->m_device.waitForFences(f, true, m_impl->g_timeout) != vk::Result::eSuccess)
    GRF_PANIC("Hung waiting for fences");
}

void GRF::resetFences(const std::vector<Fence>& fences) {
  if (fences.empty()) return;

  std::vector<vk::Fence> f;
  for (const auto& fence : fences)
    f.emplace_back(fence.m_impl->m_fence);

  m_impl->m_device.resetFences(f);
}

GRF::Impl::Impl(const Settings& settings) : m_settings(settings) {
  std::vector<const char *> requiredExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  createWindow();
  createInstance();
  createSurface();
  chooseGPU(requiredExtensions);
  createDevice(requiredExtensions);

  m_graphicsQueue.queue = m_device.getQueue(m_graphicsQueue.index, 0);
  m_computeQueue.queue = m_device.getQueue(m_computeQueue.index, 0);
  m_transferQueue.queue = m_device.getQueue(m_transferQueue.index, 0);

  vk::PhysicalDeviceProperties properties = m_gpu.getProperties();

  m_allocator = std::make_unique<Allocator>(
    m_instance, m_gpu, m_device, properties.apiVersion, m_resourceManager
  );
  m_descriptorHeap = std::make_unique<DescriptorHeap>(m_gpu, m_device);
  m_shaderManager = std::make_unique<ShaderManager>(m_device);
  createTimelineSemaphores();

  m_resourceManager = std::make_shared<ResourceManager>(
    m_allocator, m_descriptorHeap, m_graphicsQueue, m_computeQueue, m_transferQueue, m_device
  );

  createSwapchain();
  createPipelineLayout();
  createCommandPools();

  m_input = std::unique_ptr<Input>(new Input(std::make_unique<Input::Impl>(m_window)));

  m_gui = std::unique_ptr<GUI>(new GUI(std::make_unique<GUI::Impl>(
    m_window,
    m_instance,
    m_gpu,
    m_device,
    m_graphicsQueue,
    m_swapchainFormat,
    static_cast<uint32_t>(m_swapchainImages.size())
  )));

  m_profiler = std::unique_ptr<Profiler>(new Profiler(std::make_unique<Profiler::Impl>(
    m_device, m_gpu, m_settings.flightFrames
  )));

  log::generic("Groot Rendering Framework {}\nvulkan {}.{}.{} on {}",
    std::string(GRF_VERSION),
    VK_VERSION_MAJOR(properties.apiVersion),
    VK_VERSION_MINOR(properties.apiVersion),
    VK_VERSION_PATCH(properties.apiVersion),
    std::string(properties.deviceName)
  );
}

GRF::Impl::~Impl() {
  m_device.waitIdle();

  m_gui.reset();
  m_profiler.reset();

  m_device.destroyPipelineLayout(m_pipelineLayout);

  m_resourceManager->destroy();
  m_resourceManager.reset();

  for (auto& pool : m_commandPools)
    if (pool != nullptr) m_device.destroyCommandPool(pool);

  for (Queue * q : { &m_graphicsQueue, &m_computeQueue, &m_transferQueue })
    if (q->timeline != nullptr) m_device.destroySemaphore(q->timeline);

  m_shaderManager->destroy();
  m_descriptorHeap->destroy();
  m_allocator->destroy();

  for (const auto& img: m_swapchainImages)
    m_device.destroyImageView(img->m_view);
  m_device.destroySwapchainKHR(m_swapchain);

  m_device.destroy();

  m_instance.destroySurfaceKHR(m_surface);
  m_instance.destroy();

  glfwDestroyWindow(m_window);
  glfwTerminate();
}

ImageData readImage(const std::string& path) {
  int width = 0, height = 0, channels = 0;
  stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  if (pixels == nullptr)
    GRF_PANIC("Failed to load image '{}': {}", path, stbi_failure_reason());

  const std::size_t byteCount = static_cast<std::size_t>(width) * height * 4;
  std::vector<std::byte> bytes(byteCount);
  std::memcpy(bytes.data(), pixels, byteCount);

  stbi_image_free(pixels);
  return {
    .bytes  = std::move(bytes),
    .width  = static_cast<uint32_t>(width),
    .height = static_cast<uint32_t>(height),
  };
}

}
