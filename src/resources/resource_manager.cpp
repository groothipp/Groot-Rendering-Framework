#include "internal/resources/resource_manager.hpp"
#include "internal/log.hpp"

namespace grf {

ResourceManager::ResourceManager(
  std::unique_ptr<Allocator>& allocator, Queue& transferQueue, vk::Device& device)
: m_allocator(allocator), m_transferQueue(transferQueue), m_device(device) {
  m_bufferUpdateFence = m_device.createFence(vk::FenceCreateInfo{
    .flags = vk::FenceCreateFlagBits::eSignaled
  });

  m_imageUpdateFence = m_device.createFence(vk::FenceCreateInfo{
    .flags = vk::FenceCreateFlagBits::eSignaled
  });

  m_transferPool = m_device.createCommandPool(vk::CommandPoolCreateInfo{
    .flags            = vk::CommandPoolCreateFlagBits::eTransient,
    .queueFamilyIndex = m_transferQueue.index
  });
}

ResourceManager::~ResourceManager() {
  if (m_transferPool != nullptr)
    destroy();
}

void ResourceManager::destroy() {
  if (m_bufferCmd != nullptr || m_imageCmd != nullptr)
    waitForUpdates();

  m_device.destroyFence(m_bufferUpdateFence);
  m_device.destroyFence(m_imageUpdateFence);
  m_device.destroyCommandPool(m_transferPool);

  m_bufferUpdateFence = nullptr;
  m_imageUpdateFence = nullptr;
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
    m_bufferUpdateFuture = std::async(std::launch::async,
      [this, u = std::move(u)]() mutable {
        updateBuffers(std::move(u));
      }
    );
    m_bufferUpdates.clear();
  }

  if (!m_imageUpdates.empty()) {
    auto u = std::move(m_imageUpdates);
    m_imageUpdateFuture = std::async(std::launch::async,
      [this, u = std::move(u)]() mutable {
        updateImages(std::move(u));
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

  if (m_device.waitForFences({ m_bufferUpdateFence, m_imageUpdateFence }, true, g_resourceFenceTimeout) != vk::Result::eSuccess)
    GRF_PANIC("Hung waiting for resource updates");

  m_allocator->destroyStagingBuffers();
  m_device.freeCommandBuffers(m_transferPool, { m_bufferCmd, m_imageCmd });

  m_bufferCmd = nullptr;
  m_imageCmd = nullptr;
}

void ResourceManager::updateBuffers(std::vector<BufferUpdateInfo> infos) {
  m_device.resetFences(m_bufferUpdateFence);

  m_bufferCmd = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
    .commandPool        = m_transferPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  })[0];

  m_bufferCmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

  for (const auto& info : infos) {
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

  m_transferQueue.queue.submit(vk::SubmitInfo{
    .commandBufferCount = 1,
    .pCommandBuffers    = &m_bufferCmd
  }, m_bufferUpdateFence);
}

void ResourceManager::updateImages(std::vector<ImageUpdateInfo> infos) {
  m_device.resetFences(m_imageUpdateFence);

  m_imageCmd = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
    .commandPool        = m_transferPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  })[0];

  m_imageCmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

  for (auto& info: infos) {
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

  m_transferQueue.queue.submit(vk::SubmitInfo{
    .commandBufferCount = 1,
    .pCommandBuffers    = &m_imageCmd
  }, m_imageUpdateFence);
}

}