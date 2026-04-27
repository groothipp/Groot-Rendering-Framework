#pragma once

#include "internal/allocator.hpp"
#include "internal/descriptor_heap.hpp"
#include "internal/graveyard.hpp"

#include "public/types.hpp"

#include <array>
#include <cstdint>
#include <future>
#include <mutex>
#include <vector>

namespace grf {

struct Queue {
  uint8_t       index = 0xFF;
  vk::Queue     queue = nullptr;
  vk::Semaphore timeline = nullptr;
  uint64_t      nextValue = 1;
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
  int32_t                     depth = 0;
  CubeFace                    face = CubeFace::Right;
  bool                        isCubemap = false;
};

class ResourceManager {
  static constexpr uint64_t           g_resourceFenceTimeout = 1000000000ul;

  std::unique_ptr<Allocator>&         m_allocator;
  std::unique_ptr<DescriptorHeap>&    m_descriptorHeap;
  std::array<Queue *, 3>              m_queues;
  vk::Device&                         m_device;

  vk::CommandPool                     m_bufferTransferPool = nullptr;
  vk::CommandPool                     m_imageTransferPool = nullptr;
  vk::CommandBuffer                   m_bufferCmd = nullptr;
  vk::CommandBuffer                   m_imageCmd = nullptr;

  std::vector<BufferUpdateInfo>       m_bufferUpdates;
  uint64_t                            m_bufferUpdateValue = 0;
  std::future<void>                   m_bufferUpdateFuture;

  std::vector<ImageUpdateInfo>        m_imageUpdates;
  uint64_t                            m_imageUpdateValue = 0;
  std::future<void>                   m_imageUpdateFuture;

  std::mutex                          m_graveyardMutex;
  std::vector<Grave>                  m_graveyard;

public:
  ResourceManager(
    std::unique_ptr<Allocator>&,
    std::unique_ptr<DescriptorHeap>&,
    Queue& graphics, Queue& compute, Queue& transfer,
    vk::Device&
  );
  ~ResourceManager();

  void destroy();

  void writeBuffer(const BufferUpdateInfo&);
  void readBuffer(vk::DeviceAddress, std::span<std::byte>, std::size_t);

  void writeImage(const ImageWriteInfo&);

  void beginUpdates();
  void waitForUpdates();

  void scheduleDestruction(const Grave&);
  void drain();
  void drainAll();

  uint64_t reserveValue(QueueType);
  void signalTimeline(QueueType, uint64_t);
  Queue& queue(QueueType);
  std::array<uint64_t, 3> currentTimelineValues();

private:
  void updateBuffers(std::vector<BufferUpdateInfo>, uint64_t value);
  void updateImages(std::vector<ImageUpdateInfo>, uint64_t value);
  bool readyToDrain(const std::array<uint64_t, 3>& retire,
                    const std::array<uint64_t, 3>& current) const;
  void executeDrain(const Grave&);
};

}
