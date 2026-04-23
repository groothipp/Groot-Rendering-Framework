#include "internal/resource_manager.hpp"
#include "internal/log.hpp"
#include "internal/structs.hpp"

namespace grf {

ResourceManager::ResourceManager(
  std::unique_ptr<Allocator>& allocator, Queue& transferQueue, vk::Device& device)
: m_allocator(allocator), m_transferQueue(transferQueue), m_device(device) {
  m_bufferUpdateFence = m_device.createFence(vk::FenceCreateInfo{
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
  if (m_transferCmd != nullptr)
    waitForUpdates();

  m_device.destroyFence(m_bufferUpdateFence);
  m_device.destroyCommandPool(m_transferPool);

  m_bufferUpdateFence = nullptr;
  m_transferPool = nullptr;
}

void ResourceManager::writeBuffer(
  vk::DeviceAddress address, std::span<const std::byte> data, std::size_t offset
){
  m_bufferUpdates[address] = {
    std::vector<std::byte>(data.begin(), data.end()), offset
  };
}

void ResourceManager::beginUpdates() {
  if (m_transferCmd != nullptr)
    waitForUpdates();

  if (m_bufferUpdates.empty()) return;

  auto u = std::move(m_bufferUpdates);

  m_bufferUpdateFuture = std::async(std::launch::async,
    [this, u = std::move(u)]() mutable {
      updateBuffers(std::move(u));
    }
  );
}

void ResourceManager::waitForUpdates() {
  if (m_bufferUpdateFuture.valid())
    m_bufferUpdateFuture.get();

  if (m_device.waitForFences(m_bufferUpdateFence, true, 1000000000ul) != vk::Result::eSuccess)
    GRF_PANIC("Hung waiting for resource updates");

  m_allocator->destroyStagingBuffers();
  m_device.freeCommandBuffers(m_transferPool, m_transferCmd);
  m_transferCmd = nullptr;
}

void ResourceManager::updateBuffers(BufferUpdateMap map) {
  m_device.resetFences(m_bufferUpdateFence);

  m_transferCmd = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
    .commandPool        = m_transferPool,
    .level              = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = 1
  })[0];

  m_transferCmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

  for (const auto& [address, update] : map) {
    const auto& [data, offset] = update;
    auto res = m_allocator->writeBuffer(address, data, offset);

    if (!res.has_value()) continue;
    auto [dst, src] = res.value();

    m_transferCmd.copyBuffer(src, dst, vk::BufferCopy{
      .srcOffset  = 0,
      .dstOffset  = offset,
      .size       = data.size()
    });
  }

  m_transferCmd.end();

  m_transferQueue.queue.submit(vk::SubmitInfo{
    .commandBufferCount = 1,
    .pCommandBuffers    = &m_transferCmd
  }, m_bufferUpdateFence);
}

}