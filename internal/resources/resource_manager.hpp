#pragma once

#include "internal/allocator.hpp"

#include <cstdint>
#include <future>
#include <vector>

namespace grf {

struct Queue {
  uint8_t index = 0xFF;
  vk::Queue queue = nullptr;
};

struct BufferUpdateInfo {
  std::shared_ptr<Buffer::Impl> buf;
  std::vector<std::byte>        data;
  std::size_t                   offset;
};

struct ImageUpdateInfo {
  std::shared_ptr<Image>  img;
  std::vector<std::byte>  data;
  vk::BufferImageCopy     region;
  vk::ImageLayout         layout;
  bool                    isCubemap;
};

struct ImageWriteInfo {
  std::shared_ptr<Image>      img;
  std::span<const std::byte>  data;
  vk::ImageLayout             layout;
  int32_t                     depth = 1;
  CubeFace                    face = CubeFace::Right;
  bool                        isCubemap = false;
};

class ResourceManager {
  static constexpr uint64_t     g_resourceFenceTimeout = 1000000000ul;

  std::unique_ptr<Allocator>&   m_allocator;
  Queue&                        m_transferQueue;
  vk::Device&                   m_device;

  vk::CommandPool               m_transferPool = nullptr;
  vk::CommandBuffer             m_bufferCmd = nullptr;
  vk::CommandBuffer             m_imageCmd = nullptr;

  std::vector<BufferUpdateInfo> m_bufferUpdates;
  vk::Fence                     m_bufferUpdateFence = nullptr;
  std::future<void>             m_bufferUpdateFuture;

  std::vector<ImageUpdateInfo>  m_imageUpdates;
  vk::Fence                     m_imageUpdateFence = nullptr;
  std::future<void>             m_imageUpdateFuture;

public:
  ResourceManager(std::unique_ptr<Allocator>&, Queue&, vk::Device&);
  ~ResourceManager();

  void destroy();

  void writeBuffer(const BufferUpdateInfo&);
  void readBuffer(vk::DeviceAddress, std::span<std::byte>, std::size_t);

  void writeImage(const ImageWriteInfo&);

  void beginUpdates();
  void waitForUpdates();

private:
  void updateBuffers(std::vector<BufferUpdateInfo>);
  void updateImages(std::vector<ImageUpdateInfo>);
};

}