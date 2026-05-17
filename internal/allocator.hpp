#pragma once

#include "internal/graveyard.hpp"
#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/sampler.hpp"

#include "public/types.hpp"

#include "external/vma/include/vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>

#include <array>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <cstdint>
#include <vector>

namespace grf {

class ResourceManager;

struct ImageAllocInfo{
  vk::ImageType       type;
  vk::Format          format;
  vk::ImageUsageFlags usage;
  uint32_t            width;
  uint32_t            height;
  uint32_t            depth = 1;
  uint32_t            mipLevels = 1;
  bool                isCubemap = false;
};

struct StagingHandle {
  vk::Buffer      buffer;
  vk::DeviceSize  offset;
};

class Allocator {
  using BufferMap = std::unordered_map<uint64_t, std::weak_ptr<Buffer::Impl>>;
  using ImageMap = std::unordered_map<uint64_t, std::weak_ptr<Image>>;
  using SamplerMap = std::unordered_map<uint64_t, std::weak_ptr<Sampler::Impl>>;
  using StagingMap = std::unordered_map<uint64_t, std::pair<VmaAllocation, vk::Buffer>>;

  std::shared_ptr<ResourceManager>& m_resourceManager;
  std::recursive_mutex              m_mutex;

  bool                              m_anisotropySupport = false;
  float                             m_maxAnisotropy = 1.0;

  vk::Device&                       m_device;
  VmaAllocator                      m_allocator = nullptr;
  BufferMap                         m_buffers;
  ImageMap                          m_images;
  SamplerMap                        m_samplers;
  StagingMap                        m_staging;

  vk::Buffer                        m_stagingRing = nullptr;
  VmaAllocation                     m_stagingRingAlloc = nullptr;
  std::byte *                       m_stagingRingMapped = nullptr;
  vk::DeviceSize                    m_stagingRingSize = 0;
  vk::DeviceSize                    m_stagingRingHead = 0;

  std::vector<uint32_t>             m_imageSharingFamilies;
  vk::SharingMode                   m_imageSharingMode = vk::SharingMode::eExclusive;

  uint64_t                          m_nextImageID = 0;
  uint64_t                          m_nextSamplerID = 0;
  uint64_t                          m_nextStagingBuffer = 0;

public:
  Allocator(
    const vk::Instance&, const vk::PhysicalDevice&, vk::Device&,
    uint32_t, std::shared_ptr<ResourceManager>&,
    vk::DeviceSize stagingRingSize,
    std::array<uint32_t, 3> queueFamilies
  );
  ~Allocator();

  void destroy();
  Buffer allocateBuffer(vk::DeviceSize, BufferIntent);
  std::shared_ptr<Image> allocateImage(const ImageAllocInfo&);
  std::shared_ptr<Sampler::Impl> createSampler(const SamplerSettings&);

  std::optional<StagingHandle> writeBuffer(vk::DeviceAddress, std::span<const std::byte>, std::size_t);
  void readBuffer(vk::DeviceAddress, std::span<std::byte>, std::size_t);
  StagingHandle stage(std::span<const std::byte>);

  void destroyBuffer(const Grave&);
  void destroyImage(const Grave&);
  void destroySampler(const Grave&);

  void destroyStagingBuffers();

private:
  std::pair<VmaMemoryUsage, VmaAllocationCreateFlags> getVMAflags(BufferIntent) const;
};

}
