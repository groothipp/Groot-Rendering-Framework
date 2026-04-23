#pragma once

#include "./allocator.hpp"
#include "./structs.hpp"

#include <unordered_map>
#include <future>
#include <vector>

namespace grf {

class ResourceManager {
  using BufferUpdateMap = std::unordered_map<
    vk::DeviceAddress, std::pair<std::vector<std::byte>, std::size_t>
  >;

  std::unique_ptr<Allocator>& m_allocator;
  Queue&                      m_transferQueue;
  vk::Device&                 m_device;

  vk::CommandPool             m_transferPool = nullptr;
  vk::CommandBuffer           m_transferCmd = nullptr;

  BufferUpdateMap             m_bufferUpdates;

  vk::Fence                   m_bufferUpdateFence = nullptr;
  std::future<void>           m_bufferUpdateFuture;

public:
  ResourceManager(std::unique_ptr<Allocator>&, Queue&, vk::Device&);
  ~ResourceManager();

  void destroy();

  void writeBuffer(vk::DeviceAddress, std::span<const std::byte>, std::size_t);
  void beginUpdates();
  void waitForUpdates();

private:
  void updateBuffers(BufferUpdateMap);
};

}