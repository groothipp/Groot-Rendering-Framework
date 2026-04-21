#pragma once

#include "./buffer.hpp"
#include "./image.hpp"

#include "public/structs.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <unordered_map>

namespace grf {

class Allocator {
  using BufferMap = std::unordered_map<uint64_t, std::shared_ptr<Buffer::Impl>>;
  using ImageMap = std::unordered_map<uint64_t, std::shared_ptr<Image::Impl>>;
  using SamplerMap = std::unordered_map<uint64_t, std::shared_ptr<Sampler::Impl>>;

  bool          m_anisotropySupport = false;
  float         m_maxAnisotropy = 1.0;

  vk::Device&   m_device;
  VmaAllocator  m_allocator = nullptr;
  BufferMap     m_buffers;
  ImageMap      m_images;
  SamplerMap    m_samplers;

  uint64_t      m_nextImageID = 0;
  uint64_t      m_nextSamplerID = 0;

public:
  Allocator(const vk::Instance&, const vk::PhysicalDevice&, vk::Device&, uint32_t);
  ~Allocator();

  void destroy();
  Buffer allocateBuffer(vk::DeviceSize, BufferIntent);
  std::shared_ptr<Image::Impl> allocateImage(
    vk::ImageType, vk::Format, vk::ImageUsageFlags,
    uint32_t, uint32_t, uint32_t depth = 1
  );
  std::shared_ptr<Image::Impl> allocateCubemap(vk::Format, uint32_t, uint32_t);
  std::shared_ptr<Sampler::Impl> createSampler(const SamplerSettings&);

private:
  std::pair<VmaMemoryUsage, VmaAllocationCreateFlags> getVMAflags(BufferIntent) const;
};

}