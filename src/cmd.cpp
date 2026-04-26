#include "public/cmd.hpp"

#include "internal/cmd.hpp"
#include "internal/graveyard.hpp"
#include "internal/log.hpp"
#include "internal/pipelines.hpp"
#include "internal/profiler.hpp"
#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/swapchain_image.hpp"

namespace grf {

ImageBits CommandBuffer::extractColor(const ColorAttachment& a) {
  return std::visit([](const auto& v) -> ImageBits {
    using V = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<V, SwapchainImage>) {
      return ImageBits{
        .image  = v.m_impl->m_image,
        .view   = v.m_impl->m_view,
        .aspect = vk::ImageAspectFlagBits::eColor
      };
    } else {
      auto img = v.m_img;
      return ImageBits{
        .image      = img->m_image,
        .view       = img->m_view,
        .aspect     = vk::ImageAspectFlagBits::eColor,
        .layerCount = 1,
        .impl       = img
      };
    }
  }, a.img);
}

ImageBits CommandBuffer::extractDepth(const DepthAttachment& a) {
  return ImageBits{
    .image      = a.img.m_img->m_image,
    .view       = a.img.m_img->m_view,
    .aspect     = vk::ImageAspectFlagBits::eDepth,
    .layerCount = 1,
    .impl       = a.img.m_img
  };
}

ImageBits CommandBuffer::extractAny(const TransitionImage& img) {
  return std::visit([](const auto& v) -> ImageBits {
    using V = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<V, SwapchainImage>) {
      return ImageBits{
        .image  = v.m_impl->m_image,
        .view   = v.m_impl->m_view,
        .aspect = vk::ImageAspectFlagBits::eColor
      };
    } else if constexpr (std::is_same_v<V, DepthImage>) {
      return ImageBits{
        .image      = v.m_img->m_image,
        .view       = v.m_img->m_view,
        .aspect     = vk::ImageAspectFlagBits::eDepth,
        .layerCount = 1,
        .impl       = v.m_img
      };
    } else if constexpr (std::is_same_v<V, Cubemap>) {
      auto img = v.m_img;
      return ImageBits{
        .image      = img->m_image,
        .view       = img->m_view,
        .aspect     = vk::ImageAspectFlagBits::eColor,
        .layerCount = 6,
        .impl       = img
      };
    } else {
      auto img = v.m_img;
      return ImageBits{
        .image      = img->m_image,
        .view       = img->m_view,
        .aspect     = vk::ImageAspectFlagBits::eColor,
        .layerCount = 1,
        .impl       = img
      };
    }
  }, img);
}

namespace {

vk::AttachmentLoadOp toVk(LoadOp op) {
  switch (op) {
    case LoadOp::Load:     return vk::AttachmentLoadOp::eLoad;
    case LoadOp::Clear:    return vk::AttachmentLoadOp::eClear;
    case LoadOp::DontCare: return vk::AttachmentLoadOp::eDontCare;
  }
}

vk::AttachmentStoreOp toVk(StoreOp op) {
  switch (op) {
    case StoreOp::Store:    return vk::AttachmentStoreOp::eStore;
    case StoreOp::DontCare: return vk::AttachmentStoreOp::eDontCare;
  }
}

}

CommandBuffer::CommandBuffer(std::shared_ptr<Impl> impl) : m_impl(impl) {}

CommandBuffer::Impl::Impl(
  std::weak_ptr<ResourceManager> rm,
  vk::Device device,
  vk::CommandPool pool,
  vk::CommandBuffer buffer,
  vk::PipelineLayout pipelineLayout,
  vk::DescriptorSet descriptorSet,
  vk::Extent2D swapchainExtent,
  uint32_t pushConstantSize,
  QueueType qt,
  Profiler::Impl * profiler
) : m_resourceManager(rm),
    m_device(device),
    m_pool(pool),
    m_buffer(buffer),
    m_pipelineLayout(pipelineLayout),
    m_descriptorSet(descriptorSet),
    m_swapchainExtent(swapchainExtent),
    m_pushConstantSize(pushConstantSize),
    m_queueType(qt),
    m_profiler(profiler)
{}

CommandBuffer::Impl::~Impl() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  std::array<uint64_t, 3> retire = { 0, 0, 0 };
  retire[static_cast<size_t>(m_queueType)] = m_reservedValue;

  rm->scheduleDestruction(Grave{
    .kind          = ResourceKind::CommandBuffer,
    .retireValues  = retire,
    .commandBuffer = m_buffer,
    .commandPool   = m_pool
  });
}

void CommandBuffer::begin() {
  auto rm = m_impl->m_resourceManager.lock();
  if (rm == nullptr) GRF_PANIC("CommandBuffer::begin called after framework shutdown");

  if (m_impl->m_reservedValue != 0)
    rm->signalTimeline(m_impl->m_queueType, m_impl->m_reservedValue);

  m_impl->m_reservedValue = rm->reserveValue(m_impl->m_queueType);

  m_impl->m_buffer.reset();
  m_impl->m_buffer.begin(vk::CommandBufferBeginInfo{
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
  });

  m_impl->m_zoneStack.clear();

  vk::PipelineBindPoint bindPoints[] = {
    vk::PipelineBindPoint::eGraphics,
    vk::PipelineBindPoint::eCompute
  };
  for (auto bp : bindPoints) {
    m_impl->m_buffer.bindDescriptorSets(
      bp, m_impl->m_pipelineLayout,
      0, 1, &m_impl->m_descriptorSet, 0, nullptr
    );
  }
}

void CommandBuffer::end() {
  m_impl->m_buffer.end();
}

void CommandBuffer::bindPipeline(const ComputePipeline& p) {
  p.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(p.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, p.m_impl->m_pipeline);
}

void CommandBuffer::bindPipeline(const GraphicsPipeline& p) {
  p.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(p.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p.m_impl->m_pipeline);
}

void CommandBuffer::bindIndexBuffer(const Buffer& buf, IndexFormat fmt, std::size_t offset) {
  buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.bindIndexBuffer(
    buf.m_impl->m_buffer, offset,
    fmt == IndexFormat::U32 ? vk::IndexType::eUint32 : vk::IndexType::eUint16
  );
}

void CommandBuffer::pushBytes(std::span<const std::byte> data, uint32_t offset) {
  if (offset + data.size() > m_impl->m_pushConstantSize)
    GRF_PANIC("Push constant range overrun: {} + {} > {}",
              offset, data.size(), m_impl->m_pushConstantSize);

  m_impl->m_buffer.pushConstants(
    m_impl->m_pipelineLayout,
    vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
    offset,
    static_cast<uint32_t>(data.size()),
    data.data()
  );
}

void CommandBuffer::beginProfile(const std::string& name) {
  if (m_impl->m_profiler == nullptr) return;

  auto [beginIdx, endIdx] = m_impl->m_profiler->allocateZone(name);
  m_impl->m_zoneStack.push_back(endIdx);

  m_impl->m_buffer.writeTimestamp2(
    vk::PipelineStageFlagBits2::eAllCommands,
    m_impl->m_profiler->m_pool,
    beginIdx
  );
}

void CommandBuffer::endProfile() {
  if (m_impl->m_profiler == nullptr) return;
  if (m_impl->m_zoneStack.empty())
    GRF_PANIC("CommandBuffer::endProfile called without a matching beginProfile");

  uint32_t endIdx = m_impl->m_zoneStack.back();
  m_impl->m_zoneStack.pop_back();

  m_impl->m_buffer.writeTimestamp2(
    vk::PipelineStageFlagBits2::eAllCommands,
    m_impl->m_profiler->m_pool,
    endIdx
  );
}

void CommandBuffer::beginRendering(
  std::span<const ColorAttachment> colors,
  const std::optional<DepthAttachment>& depth
) {
  std::vector<vk::RenderingAttachmentInfo> colorInfos;
  colorInfos.reserve(colors.size());
  for (const auto& c : colors) {
    auto bits = extractColor(c);
    if (bits.impl != nullptr)
      bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
        = std::max(bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);

    vk::ClearValue cv{ .color = vk::ClearColorValue(c.clearValue) };
    colorInfos.push_back(vk::RenderingAttachmentInfo{
      .imageView   = bits.view,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = toVk(c.loadOp),
      .storeOp     = toVk(c.storeOp),
      .clearValue  = cv
    });
  }

  vk::RenderingAttachmentInfo depthInfo{};
  if (depth.has_value()) {
    auto bits = extractDepth(*depth);
    bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
      = std::max(bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);

    vk::ClearValue cv{ .depthStencil = vk::ClearDepthStencilValue{ .depth = depth->clearValue } };
    depthInfo = vk::RenderingAttachmentInfo{
      .imageView   = bits.view,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp      = toVk(depth->loadOp),
      .storeOp     = toVk(depth->storeOp),
      .clearValue  = cv
    };
  }

  vk::Rect2D area{ .offset = { 0, 0 }, .extent = m_impl->m_swapchainExtent };

  m_impl->m_buffer.beginRendering(vk::RenderingInfo{
    .renderArea           = area,
    .layerCount           = 1,
    .colorAttachmentCount = static_cast<uint32_t>(colorInfos.size()),
    .pColorAttachments    = colorInfos.data(),
    .pDepthAttachment     = depth.has_value() ? &depthInfo : nullptr
  });

  vk::Viewport viewport{
    .x        = 0.0f,
    .y        = 0.0f,
    .width    = static_cast<float>(m_impl->m_swapchainExtent.width),
    .height   = static_cast<float>(m_impl->m_swapchainExtent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f
  };
  m_impl->m_buffer.setViewport(0, 1, &viewport);
  m_impl->m_buffer.setScissor(0, 1, &area);
}

void CommandBuffer::endRendering() {
  m_impl->m_buffer.endRendering();
}

void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount,
                         uint32_t firstVertex, uint32_t firstInstance) {
  m_impl->m_buffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
  m_impl->m_buffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::drawIndirect(const Buffer& buf, std::size_t offset, uint32_t drawCount, uint32_t stride) {
  buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.drawIndirect(buf.m_impl->m_buffer, offset, drawCount, stride);
}

void CommandBuffer::drawIndexedIndirect(const Buffer& buf, std::size_t offset, uint32_t drawCount, uint32_t stride) {
  buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.drawIndexedIndirect(buf.m_impl->m_buffer, offset, drawCount, stride);
}

void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) {
  m_impl->m_buffer.dispatch(x, y, z);
}

void CommandBuffer::dispatchIndirect(const Buffer& buf, std::size_t offset) {
  buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
    = std::max(buf.m_impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
  m_impl->m_buffer.dispatchIndirect(buf.m_impl->m_buffer, offset);
}

void CommandBuffer::transition(const TransitionImage& img, Layout from, Layout to) {
  auto bits = extractAny(img);
  if (bits.impl != nullptr) {
    bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
      = std::max(bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
    bits.impl->m_layout = static_cast<vk::ImageLayout>(to);
  }

  vk::ImageMemoryBarrier2 barrier{
    .srcStageMask     = vk::PipelineStageFlagBits2::eAllCommands,
    .srcAccessMask    = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    .dstStageMask     = vk::PipelineStageFlagBits2::eAllCommands,
    .dstAccessMask    = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    .oldLayout        = static_cast<vk::ImageLayout>(from),
    .newLayout        = static_cast<vk::ImageLayout>(to),
    .image            = bits.image,
    .subresourceRange = {
      .aspectMask = bits.aspect,
      .levelCount = 1,
      .layerCount = bits.layerCount
    }
  };

  m_impl->m_buffer.pipelineBarrier2(vk::DependencyInfo{
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers    = &barrier
  });
}

void CommandBuffer::release(const TransitionImage& img, Layout from, Layout to, QueueType dstQueue) {
  auto rm = m_impl->m_resourceManager.lock();
  if (rm == nullptr) return;
  auto bits = extractAny(img);
  if (bits.impl != nullptr)
    bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
      = std::max(bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);

  vk::ImageMemoryBarrier2 barrier{
    .srcStageMask        = vk::PipelineStageFlagBits2::eAllCommands,
    .srcAccessMask       = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    .dstStageMask        = {},
    .dstAccessMask       = {},
    .oldLayout           = static_cast<vk::ImageLayout>(from),
    .newLayout           = static_cast<vk::ImageLayout>(to),
    .srcQueueFamilyIndex = rm->queue(m_impl->m_queueType).index,
    .dstQueueFamilyIndex = rm->queue(dstQueue).index,
    .image               = bits.image,
    .subresourceRange    = {
      .aspectMask = bits.aspect,
      .levelCount = 1,
      .layerCount = bits.layerCount
    }
  };

  m_impl->m_buffer.pipelineBarrier2(vk::DependencyInfo{
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers    = &barrier
  });
}

void CommandBuffer::acquire(const TransitionImage& img, Layout from, Layout to, QueueType srcQueue) {
  auto rm = m_impl->m_resourceManager.lock();
  if (rm == nullptr) return;
  auto bits = extractAny(img);
  if (bits.impl != nullptr) {
    bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)]
      = std::max(bits.impl->m_lastUseValues[static_cast<size_t>(m_impl->m_queueType)], m_impl->m_reservedValue);
    bits.impl->m_layout = static_cast<vk::ImageLayout>(to);
  }

  vk::ImageMemoryBarrier2 barrier{
    .srcStageMask        = {},
    .srcAccessMask       = {},
    .dstStageMask        = vk::PipelineStageFlagBits2::eAllCommands,
    .dstAccessMask       = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    .oldLayout           = static_cast<vk::ImageLayout>(from),
    .newLayout           = static_cast<vk::ImageLayout>(to),
    .srcQueueFamilyIndex = rm->queue(srcQueue).index,
    .dstQueueFamilyIndex = rm->queue(m_impl->m_queueType).index,
    .image               = bits.image,
    .subresourceRange    = {
      .aspectMask = bits.aspect,
      .levelCount = 1,
      .layerCount = bits.layerCount
    }
  };

  m_impl->m_buffer.pipelineBarrier2(vk::DependencyInfo{
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers    = &barrier
  });
}

}
