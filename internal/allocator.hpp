#pragma once

#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/sampler.hpp"

#include "public/structs.hpp"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <optional>
#include <mutex>

namespace grf {

class ResourceManager;

struct ImageAllocInfo{
  vk::ImageType       type;
  vk::Format          format;
  vk::ImageUsageFlags usage;
  uint32_t            width;
  uint32_t            height;
  uint32_t            depth = 1;
  bool                isCubemap = false;
};

class Allocator {
  using BufferMap = std::unordered_map<uint64_t, std::shared_ptr<Buffer::Impl>>;
  using ImageMap = std::unordered_map<uint64_t, std::shared_ptr<Image>>;
  using SamplerMap = std::unordered_map<uint64_t, std::shared_ptr<Sampler::Impl>>;
  using StagingMap = std::unordered_map<uint64_t, std::pair<VmaAllocation, vk::Buffer>>;

  std::unique_ptr<ResourceManager>& m_resourceManager;
  std::mutex                        m_mutex;

  bool                              m_anisotropySupport = false;
  float                             m_maxAnisotropy = 1.0;

  vk::Device&                       m_device;
  VmaAllocator                      m_allocator = nullptr;
  BufferMap                         m_buffers;
  ImageMap                          m_images;
  SamplerMap                        m_samplers;
  StagingMap                        m_staging;

  uint64_t                          m_nextImageID = 0;
  uint64_t                          m_nextSamplerID = 0;
  uint64_t                          m_nextStagingBuffer = 0;

public:
  Allocator(
    const vk::Instance&, const vk::PhysicalDevice&, vk::Device&,
    uint32_t, std::unique_ptr<ResourceManager>&
  );
  ~Allocator();

  void destroy();
  Buffer allocateBuffer(vk::DeviceSize, BufferIntent);
  std::shared_ptr<Image> allocateImage(const ImageAllocInfo&);
  std::shared_ptr<Sampler::Impl> createSampler(const SamplerSettings&);

  std::optional<vk::Buffer> writeBuffer(vk::DeviceAddress, std::span<const std::byte>, std::size_t);
  void readBuffer(vk::DeviceAddress, std::span<std::byte>, std::size_t);
  vk::Buffer stage(std::span<const std::byte>);

  void destroyStagingBuffers();

private:
  std::pair<VmaMemoryUsage, VmaAllocationCreateFlags> getVMAflags(BufferIntent) const;
};

}