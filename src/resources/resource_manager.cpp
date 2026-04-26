#include "internal/resources/resource_manager.hpp"
#include "internal/log.hpp"

#include <algorithm>
#include <iterator>

namespace grf {

ResourceManager::ResourceManager(
  std::unique_ptr<Allocator>& allocator,
  std::unique_ptr<DescriptorHeap>& descriptorHeap,
  Queue& graphics, Queue& compute, Queue& transfer,
  vk::Device& device
) : m_allocator(allocator),
    m_descriptorHeap(descriptorHeap),
    m_queues{ &graphics, &compute, &transfer },
    m_device(device) {
  m_transferPool = m_device.createCommandPool(vk::CommandPoolCreateInfo{
    .flags            = vk::CommandPoolCreateFlagBits::eTransient,
    .queueFamilyIndex = transfer.index
  });
}

ResourceManager::~ResourceManager() {
  if (m_transferPool != nullptr)
    destroy();
}

void ResourceManager::destroy() {
  if (m_bufferCmd != nullptr || m_imageCmd != nullptr)
    waitForUpdates();

  drainAll();

  m_device.destroyCommandPool(m_transferPool);

  m_transferPool = nullptr;
}

void ResourceManager::writeBuffer(const BufferUpdateInfo& info){
  m_bufferUpdates.emplace_back(info);
}

void ResourceManager::readBuffer(vk::DeviceAddress address, std::span<std::byte> data, std::size_t offset) {
  m_allocator->readBuffer(address, data, offset);
}

void ResourceManager::writeImage(const ImageWriteInfo& info) {
  m_imageUpdates.emplace_back(ImageUpdateInfo{
    .img                = info.img,
    .data               = std::vector<std::byte>(info.data.begin(), info.data.end()),
    .region             = vk::BufferImageCopy{
      .imageSubresource = {
        .aspectMask     = vk::ImageAspectFlagBits::eColor,
        .baseArrayLayer = static_cast<uint32_t>(info.face),
        .layerCount     = 1
      },
      .imageOffset      = {
        .x = 0,
        .y = 0,
        .z = info.depth
      },
      .imageExtent      = {
        .width  = info.img->m_width,
        .height = info.img->m_height,
        .depth  = 1
      }
    },
    .layout             = info.layout,
    .isCubemap          = info.isCubemap
  });
}

void ResourceManager::beginUpdates() {
  if (m_bufferCmd != nullptr || m_imageCmd != nullptr)
    waitForUpdates();

  if (!m_bufferUpdates.empty()) {
    auto u = std::move(m_bufferUpdates);
    m_bufferUpdateValue = reserveValue(QueueType::Transfer);
    uint64_t v = m_bufferUpdateValue;
    m_bufferUpdateFuture = std::async(std::launch::async,
      [this, u = std::move(u), v]() mutable {
        updateBuffers(std::move(u), v);
      }
    );
    m_bufferUpdates.clear();
  }

  if (!m_imageUpdates.empty()) {
    auto u = std::move(m_imageUpdates);
    m_imageUpdateValue = reserveValue(QueueType::Transfer);
    uint64_t v = m_imageUpdateValue;
    m_imageUpdateFuture = std::async(std::launch::async,
      [this, u = std::move(u), v]() mutable {
        updateImages(std::move(u), v);
      }
    );
    m_imageUpdates.clear();
  }
}

void ResourceManager::waitForUpdates() {
  if (m_bufferUpdateFuture.valid())
    m_bufferUpdateFuture.get();

  if (m_imageUpdateFuture.valid())
    m_imageUpdateFuture.get();

  uint64_t maxValue = std::max(m_bufferUpdateValue, m_imageUpdateValue);
  if (maxValue > 0) {
    vk::Semaphore timeline = m_queues[static_cast<size_t>(QueueType::Transfer)]->timeline;
    vk::SemaphoreWaitInfo waitInfo{
      .semaphoreCount = 1,
      .pSemaphores    = &timeline,
      .pValues        = &maxValue
    };
    if (m_device.waitSemaphores(waitInfo, g_resourceFenceTimeout) != vk::Result::eSuccess)
      GRF_PANIC("Hung waiting for resource updates");
  }

  m_allocator->destroyStagingBuffers();
  if (m_bufferCmd != nullptr || m_imageCmd != nullptr) {
    std::vector<vk::CommandBuffer> bufs;
    if (m_bufferCmd != nullptr) bufs.push_back(m_bufferCmd);
    if (m_imageCmd  != nullptr) bufs.push_back(m_imageCmd);
    m_device.freeCommandBuffers(m_transferPool, bufs);
  }

  m_bufferCmd = nullptr;
  m_imageCmd = nullptr;
  m_bufferUpdateValue = 0;
  m_imageUpdateValue = 0;
}

void ResourceManager::updateBuffers(std::vector<BufferUpdateInfo> infos, uint64_t value) {
  m_bufferCmd = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
    .commandPool        = m_transferPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  })[0];

  m_bufferCmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

  for (const auto& info : infos) {
    info.buf->m_lastUseValues[static_cast<size_t>(QueueType::Transfer)] = value;

    auto res = m_allocator->writeBuffer(info.buf->m_address, info.data, info.offset);
    if (!res.has_value()) continue;

    auto src = res.value();

    m_bufferCmd.copyBuffer(src, info.buf->m_buffer, vk::BufferCopy{
      .srcOffset  = 0,
      .dstOffset  = info.offset,
      .size       = info.data.size()
    });
  }

  m_bufferCmd.end();

  Queue* xq = m_queues[static_cast<size_t>(QueueType::Transfer)];

  vk::TimelineSemaphoreSubmitInfo timelineInfo{
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues    = &value
  };

  xq->queue.submit(vk::SubmitInfo{
    .pNext                = &timelineInfo,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &m_bufferCmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &xq->timeline
  });
}

void ResourceManager::updateImages(std::vector<ImageUpdateInfo> infos, uint64_t value) {
  m_imageCmd = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
    .commandPool        = m_transferPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  })[0];

  m_imageCmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

  for (auto& info: infos) {
    info.img->m_lastUseValues[static_cast<size_t>(QueueType::Transfer)] = value;

    vk::PipelineStageFlags srcStage;
    vk::AccessFlags        srcAccess;
    switch (info.img->m_layout) {
      case vk::ImageLayout::eUndefined:
        srcStage  = vk::PipelineStageFlagBits::eTopOfPipe;
        srcAccess = {};
        break;
      case vk::ImageLayout::eShaderReadOnlyOptimal:
        srcStage  = vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader;
        srcAccess = vk::AccessFlagBits::eShaderRead;
        break;
      case vk::ImageLayout::eGeneral:
        srcStage  = vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader;
        srcAccess = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
        break;
      case vk::ImageLayout::eTransferDstOptimal:
        srcStage  = vk::PipelineStageFlagBits::eTransfer;
        srcAccess = vk::AccessFlagBits::eTransferWrite;
        break;
      default:
        srcStage  = vk::PipelineStageFlagBits::eAllCommands;
        srcAccess = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        break;
    }

    vk::ImageMemoryBarrier transferBarrier{
      .srcAccessMask    = srcAccess,
      .dstAccessMask    = vk::AccessFlagBits::eTransferWrite,
      .oldLayout        = info.img->m_layout,
      .newLayout        = vk::ImageLayout::eTransferDstOptimal,
      .image            = info.img->m_image,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .levelCount = 1,
        .layerCount = info.isCubemap ? 6u : 1u
      }
    };

    m_imageCmd.pipelineBarrier(
      srcStage,
      vk::PipelineStageFlagBits::eTransfer,
      {}, {}, {},
      transferBarrier
    );

    vk::Buffer buffer = m_allocator->stage(info.data);

    m_imageCmd.copyBufferToImage(buffer, info.img->m_image, vk::ImageLayout::eTransferDstOptimal, info.region);

    vk::ImageMemoryBarrier layoutBarrier{
      .srcAccessMask    = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask    = vk::AccessFlagBits::eShaderRead,
      .oldLayout        = vk::ImageLayout::eTransferDstOptimal,
      .newLayout        = info.layout,
      .image            = info.img->m_image,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .levelCount = 1,
        .layerCount = info.isCubemap ? 6u : 1u
      }
    };

    m_imageCmd.pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader,
      {}, {}, {},
      layoutBarrier
    );

    info.img->m_layout = info.layout;
  }

  m_imageCmd.end();

  Queue* xq = m_queues[static_cast<size_t>(QueueType::Transfer)];

  vk::TimelineSemaphoreSubmitInfo timelineInfo{
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues    = &value
  };

  xq->queue.submit(vk::SubmitInfo{
    .pNext                = &timelineInfo,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &m_imageCmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = &xq->timeline
  });
}

void ResourceManager::scheduleDestruction(const Grave& grave) {
  std::lock_guard<std::mutex> g(m_graveyardMutex);
  m_graveyard.emplace_back(grave);
}

bool ResourceManager::readyToDrain(
  const std::array<uint64_t, 3>& retire,
  const std::array<uint64_t, 3>& current
) const {
  for (size_t q = 0; q < 3; ++q)
    if (retire[q] > current[q]) return false;
  return true;
}

void ResourceManager::executeDrain(const Grave& grave) {
  switch (grave.kind) {
    case ResourceKind::Buffer:
      m_allocator->destroyBuffer(grave);
      break;
    case ResourceKind::Image:
      m_allocator->destroyImage(grave);
      m_descriptorHeap->releaseSlot(grave.storageBinding, grave.storageSlot);
      m_descriptorHeap->releaseSlot(grave.sampledBinding, grave.sampledSlot);
      break;
    case ResourceKind::Sampler:
      m_allocator->destroySampler(grave);
      m_descriptorHeap->releaseSlot(grave.sampledBinding, grave.sampledSlot);
      break;
    case ResourceKind::Pipeline:
      m_device.destroyPipeline(grave.pipeline);
      break;
    case ResourceKind::Fence:
      m_device.destroyFence(grave.fence);
      break;
    case ResourceKind::Semaphore:
      m_device.destroySemaphore(grave.semaphore);
      break;
    case ResourceKind::CommandBuffer:
      m_device.freeCommandBuffers(grave.commandPool, { grave.commandBuffer });
      break;
  }
}

void ResourceManager::drain() {
  auto current = currentTimelineValues();

  std::vector<Grave> ready;
  {
    std::lock_guard<std::mutex> g(m_graveyardMutex);
    auto it = std::partition(m_graveyard.begin(), m_graveyard.end(),
      [this, &current](const Grave& gr) { return !readyToDrain(gr.retireValues, current); });
    ready.assign(std::make_move_iterator(it), std::make_move_iterator(m_graveyard.end()));
    m_graveyard.erase(it, m_graveyard.end());
  }

  for (const auto& grave : ready)
    executeDrain(grave);
}

void ResourceManager::drainAll() {
  std::vector<Grave> ready;
  {
    std::lock_guard<std::mutex> g(m_graveyardMutex);
    ready = std::move(m_graveyard);
    m_graveyard.clear();
  }

  for (const auto& grave : ready)
    executeDrain(grave);
}

uint64_t ResourceManager::reserveValue(QueueType qt) {
  Queue* q = m_queues[static_cast<size_t>(qt)];
  return q->nextValue++;
}

void ResourceManager::signalTimeline(QueueType qt, uint64_t value) {
  Queue* q = m_queues[static_cast<size_t>(qt)];
  vk::SemaphoreSignalInfo info{
    .semaphore = q->timeline,
    .value     = value
  };
  m_device.signalSemaphore(info);
}

Queue& ResourceManager::queue(QueueType qt) {
  return *m_queues[static_cast<size_t>(qt)];
}

std::array<uint64_t, 3> ResourceManager::currentTimelineValues() {
  std::array<uint64_t, 3> values = { 0, 0, 0 };
  for (size_t q = 0; q < 3; ++q)
    values[q] = m_device.getSemaphoreCounterValue(m_queues[q]->timeline);
  return values;
}

}
