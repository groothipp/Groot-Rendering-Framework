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

void CommandBuffer::setTrackedLayout(const TransitionImage& img, Layout layout) {
  auto vkLayout = static_cast<vk::ImageLayout>(layout);
  std::visit([vkLayout](const auto& v) {
    using V = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<V, SwapchainImage>) {
      v.m_impl->m_layout = vkLayout;
    } else {
      v.m_img->m_layout = vkLayout;
    }
  }, img);
}

ImageBits CommandBuffer::extractAny(const TransitionImage& img) {
  return std::visit([](const auto& v) -> ImageBits {
    using V = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<V, SwapchainImage>) {
      return ImageBits{
        .image         = v.m_impl->m_image,
        .view          = v.m_impl->m_view,
        .aspect        = vk::ImageAspectFlagBits::eColor,
        .currentLayout = v.m_impl->m_layout
      };
    } else if constexpr (std::is_same_v<V, DepthImage>) {
      auto img = v.m_img;
      return ImageBits{
        .image         = img->m_image,
        .view          = img->m_view,
        .aspect        = vk::ImageAspectFlagBits::eDepth,
        .layerCount    = 1,
        .extent        = { img->m_width, img->m_height, std::max(img->m_depth, 1u) },
        .currentLayout = img->m_layout,
        .impl          = img
      };
    } else if constexpr (std::is_same_v<V, Cubemap>) {
      auto img = v.m_img;
      return ImageBits{
        .image         = img->m_image,
        .view          = img->m_view,
        .aspect        = vk::ImageAspectFlagBits::eColor,
        .layerCount    = 6,
        .extent        = { img->m_width, img->m_height, 1 },
        .currentLayout = img->m_layout,
        .impl          = img
      };
    } else {
      auto img = v.m_img;
      return ImageBits{
        .image         = img->m_image,
        .view          = img->m_view,
        .aspect        = vk::ImageAspectFlagBits::eColor,
        .layerCount    = 1,
        .extent        = { img->m_width, img->m_height, std::max(img->m_depth, 1u) },
        .currentLayout = img->m_layout,
        .impl          = img
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

  vk::PipelineBindPoint bindPoints[2];
  uint32_t bindPointCount = 0;
  switch (m_impl->m_queueType) {
    case QueueType::Graphics:
      bindPoints[bindPointCount++] = vk::PipelineBindPoint::eGraphics;
      bindPoints[bindPointCount++] = vk::PipelineBindPoint::eCompute;
      break;
    case QueueType::Compute:
      bindPoints[bindPointCount++] = vk::PipelineBindPoint::eCompute;
      break;
    case QueueType::Transfer:
      break;
  }
  for (uint32_t i = 0; i < bindPointCount; ++i) {
    m_impl->m_buffer.bindDescriptorSets(
      bindPoints[i], m_impl->m_pipelineLayout,
      0, 1, &m_impl->m_descriptorSet, 0, nullptr
    );
  }

  if (
    m_impl->m_profiler != nullptr             &&
    m_impl->m_profiler->m_slotResetPending    &&
    m_impl->m_queueType != QueueType::Transfer
  ) {
    const uint32_t slot  = m_impl->m_profiler->m_currentSlot;
    const uint32_t first = slot * Profiler::Impl::kMaxZonesPerFrame * 2;
    const uint32_t count = Profiler::Impl::kMaxZonesPerFrame * 2;
    m_impl->m_buffer.resetQueryPool(m_impl->m_profiler->m_pool, first, count);
    m_impl->m_profiler->m_slotResetPending = false;
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
  }
  setTrackedLayout(img, to);

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
  }
  setTrackedLayout(img, to);

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

void CommandBuffer::barrier() {
  vk::MemoryBarrier2 mem{
    .srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands,
    .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    .dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands,
    .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite
  };

  m_impl->m_buffer.pipelineBarrier2(vk::DependencyInfo{
    .memoryBarrierCount = 1,
    .pMemoryBarriers    = &mem
  });
}

namespace {

std::pair<vk::PipelineStageFlags2, vk::AccessFlags2> bufferAccessMasks(BufferAccess a) {
  switch (a) {
    case BufferAccess::ShaderRead:
      return {
        vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderRead
      };
    case BufferAccess::ShaderWrite:
      return {
        vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderWrite
      };
    case BufferAccess::TransferRead:
      return { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead };
    case BufferAccess::TransferWrite:
      return { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite };
    case BufferAccess::IndirectRead:
      return { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead };
    case BufferAccess::IndexRead:
      return { vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead };
  }
}

}

void CommandBuffer::barrier(const Buffer& buf, BufferAccess from, BufferAccess to) {
  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  buf.m_impl->m_lastUseValues[qIdx] = std::max(buf.m_impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  auto [srcStage, srcAccess] = bufferAccessMasks(from);
  auto [dstStage, dstAccess] = bufferAccessMasks(to);

  vk::BufferMemoryBarrier2 bmb{
    .srcStageMask        = srcStage,
    .srcAccessMask       = srcAccess,
    .dstStageMask        = dstStage,
    .dstAccessMask       = dstAccess,
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .buffer              = buf.m_impl->m_buffer,
    .offset              = 0,
    .size                = vk::WholeSize
  };

  m_impl->m_buffer.pipelineBarrier2(vk::DependencyInfo{
    .bufferMemoryBarrierCount = 1,
    .pBufferMemoryBarriers    = &bmb
  });
}

void CommandBuffer::copyBuffer(const Buffer& src, const Buffer& dst) {
  std::size_t size = std::min(src.m_impl->m_size, dst.m_impl->m_size);
  copyBuffer(src, dst, size, 0, 0);
}

void CommandBuffer::copyBuffer(const Buffer& src, const Buffer& dst,
                               std::size_t size,
                               std::size_t srcOffset,
                               std::size_t dstOffset) {
  if (size == 0) return;
  if (srcOffset + size > src.m_impl->m_size)
    GRF_PANIC("copyBuffer: src range [{}, {}) exceeds src buffer size {}",
              srcOffset, srcOffset + size, src.m_impl->m_size);
  if (dstOffset + size > dst.m_impl->m_size)
    GRF_PANIC("copyBuffer: dst range [{}, {}) exceeds dst buffer size {}",
              dstOffset, dstOffset + size, dst.m_impl->m_size);

  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  src.m_impl->m_lastUseValues[qIdx] = std::max(src.m_impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);
  dst.m_impl->m_lastUseValues[qIdx] = std::max(dst.m_impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  vk::BufferCopy2 region{
    .srcOffset = srcOffset,
    .dstOffset = dstOffset,
    .size      = size
  };

  m_impl->m_buffer.copyBuffer2(vk::CopyBufferInfo2{
    .srcBuffer   = src.m_impl->m_buffer,
    .dstBuffer   = dst.m_impl->m_buffer,
    .regionCount = 1,
    .pRegions    = &region
  });
}

void CommandBuffer::copyBufferToImage(const Buffer& src, const TransitionImage& dst) {
  ImageBits    bits   = extractAny(dst);
  vk::Extent3D extent = bits.extent.width != 0 ? bits.extent
    : vk::Extent3D{ m_impl->m_swapchainExtent.width, m_impl->m_swapchainExtent.height, 1 };

  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  src.m_impl->m_lastUseValues[qIdx] = std::max(src.m_impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);
  if (bits.impl != nullptr)
    bits.impl->m_lastUseValues[qIdx] = std::max(bits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  vk::BufferImageCopy2 region{
    .bufferOffset      = 0,
    .bufferRowLength   = 0,
    .bufferImageHeight = 0,
    .imageSubresource  = {
      .aspectMask     = bits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = bits.layerCount
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = extent
  };

  m_impl->m_buffer.copyBufferToImage2(vk::CopyBufferToImageInfo2{
    .srcBuffer      = src.m_impl->m_buffer,
    .dstImage       = bits.image,
    .dstImageLayout = bits.currentLayout,
    .regionCount    = 1,
    .pRegions       = &region
  });
}

void CommandBuffer::copyImageToBuffer(const TransitionImage& src, const Buffer& dst) {
  ImageBits    bits   = extractAny(src);
  vk::Extent3D extent = bits.extent.width != 0 ? bits.extent
    : vk::Extent3D{ m_impl->m_swapchainExtent.width, m_impl->m_swapchainExtent.height, 1 };

  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  dst.m_impl->m_lastUseValues[qIdx] = std::max(dst.m_impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);
  if (bits.impl != nullptr)
    bits.impl->m_lastUseValues[qIdx] = std::max(bits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  vk::BufferImageCopy2 region{
    .bufferOffset      = 0,
    .bufferRowLength   = 0,
    .bufferImageHeight = 0,
    .imageSubresource  = {
      .aspectMask     = bits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = bits.layerCount
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = extent
  };

  m_impl->m_buffer.copyImageToBuffer2(vk::CopyImageToBufferInfo2{
    .srcImage       = bits.image,
    .srcImageLayout = bits.currentLayout,
    .dstBuffer      = dst.m_impl->m_buffer,
    .regionCount    = 1,
    .pRegions       = &region
  });
}

void CommandBuffer::copyImage(const TransitionImage& src, const TransitionImage& dst) {
  ImageBits sBits = extractAny(src);
  ImageBits dBits = extractAny(dst);

  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  if (sBits.impl != nullptr)
    sBits.impl->m_lastUseValues[qIdx] = std::max(sBits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);
  if (dBits.impl != nullptr)
    dBits.impl->m_lastUseValues[qIdx] = std::max(dBits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  const vk::Extent3D fallback{ m_impl->m_swapchainExtent.width, m_impl->m_swapchainExtent.height, 1 };
  vk::Extent3D srcExtent = sBits.extent.width != 0 ? sBits.extent : fallback;
  vk::Extent3D dstExtent = dBits.extent.width != 0 ? dBits.extent : fallback;
  vk::Extent3D extent{
    std::min(srcExtent.width,  dstExtent.width),
    std::min(srcExtent.height, dstExtent.height),
    std::min(srcExtent.depth,  dstExtent.depth)
  };
  uint32_t layerCount = std::min(sBits.layerCount, dBits.layerCount);

  vk::ImageCopy2 region{
    .srcSubresource = {
      .aspectMask     = sBits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = layerCount
    },
    .srcOffset = { 0, 0, 0 },
    .dstSubresource = {
      .aspectMask     = dBits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = layerCount
    },
    .dstOffset = { 0, 0, 0 },
    .extent    = extent
  };

  m_impl->m_buffer.copyImage2(vk::CopyImageInfo2{
    .srcImage       = sBits.image,
    .srcImageLayout = sBits.currentLayout,
    .dstImage       = dBits.image,
    .dstImageLayout = dBits.currentLayout,
    .regionCount    = 1,
    .pRegions       = &region
  });
}

void CommandBuffer::blitImage(const TransitionImage& src, const TransitionImage& dst,
                              Filter filter) {
  ImageBits sBits = extractAny(src);
  ImageBits dBits = extractAny(dst);

  const auto qIdx = static_cast<size_t>(m_impl->m_queueType);
  if (sBits.impl != nullptr)
    sBits.impl->m_lastUseValues[qIdx] = std::max(sBits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);
  if (dBits.impl != nullptr)
    dBits.impl->m_lastUseValues[qIdx] = std::max(dBits.impl->m_lastUseValues[qIdx], m_impl->m_reservedValue);

  const vk::Extent3D fallback{ m_impl->m_swapchainExtent.width, m_impl->m_swapchainExtent.height, 1 };
  vk::Extent3D srcExtent = sBits.extent.width != 0 ? sBits.extent : fallback;
  vk::Extent3D dstExtent = dBits.extent.width != 0 ? dBits.extent : fallback;
  uint32_t     layerCount = std::min(sBits.layerCount, dBits.layerCount);

  vk::ImageBlit2 region{
    .srcSubresource = {
      .aspectMask     = sBits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = layerCount
    },
    .srcOffsets = std::array<vk::Offset3D, 2>{
      vk::Offset3D{ 0, 0, 0 },
      vk::Offset3D{
        static_cast<int32_t>(srcExtent.width),
        static_cast<int32_t>(srcExtent.height),
        static_cast<int32_t>(srcExtent.depth)
      }
    },
    .dstSubresource = {
      .aspectMask     = dBits.aspect,
      .mipLevel       = 0,
      .baseArrayLayer = 0,
      .layerCount     = layerCount
    },
    .dstOffsets = std::array<vk::Offset3D, 2>{
      vk::Offset3D{ 0, 0, 0 },
      vk::Offset3D{
        static_cast<int32_t>(dstExtent.width),
        static_cast<int32_t>(dstExtent.height),
        static_cast<int32_t>(dstExtent.depth)
      }
    }
  };

  m_impl->m_buffer.blitImage2(vk::BlitImageInfo2{
    .srcImage       = sBits.image,
    .srcImageLayout = sBits.currentLayout,
    .dstImage       = dBits.image,
    .dstImageLayout = dBits.currentLayout,
    .regionCount    = 1,
    .pRegions       = &region,
    .filter         = static_cast<vk::Filter>(filter)
  });
}

}
