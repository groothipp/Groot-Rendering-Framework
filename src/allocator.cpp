#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/allocator.hpp"
#include "internal/log.hpp"
#include <mutex>

namespace grf {

Allocator::Allocator(
  const vk::Instance& instance,
  const vk::PhysicalDevice& gpu,
  vk::Device& device,
  uint32_t apiVersion,
  std::unique_ptr<ResourceManager>& resourceManager
) : m_device(device), m_resourceManager(resourceManager) {
  VmaAllocatorCreateInfo createInfo{
    .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
    .physicalDevice   = gpu,
    .device           = device,
    .instance         = instance,
    .vulkanApiVersion = apiVersion
  };

  if (vmaCreateAllocator(&createInfo, &m_allocator) != VK_SUCCESS)
    GRF_PANIC("Failed to create allocator");

  auto feats = gpu.getFeatures();
  auto props = gpu.getProperties();

  m_anisotropySupport = feats.samplerAnisotropy;
  m_maxAnisotropy = props.limits.maxSamplerAnisotropy;
}

Allocator::~Allocator() {
  if (m_allocator != nullptr)
    destroy();
}

void Allocator::destroy() {
  for (auto& [address, buffer] : m_buffers)
    vmaDestroyBuffer(m_allocator, buffer->m_buffer, buffer->m_allocation);

  for (auto& [id, image] : m_images) {
    m_device.destroyImageView(image->m_view);
    vmaDestroyImage(m_allocator, image->m_image, image->m_allocation);
  }

  for (auto& [id, sampler] : m_samplers)
    m_device.destroySampler(sampler->m_sampler);

  for (auto& [id, allocInfo] : m_staging) {
    auto [allocation, buffer] = allocInfo;
    vmaDestroyBuffer(m_allocator, buffer, allocation);
  }

  vmaDestroyAllocator(m_allocator);

  m_buffers.clear();
  m_images.clear();
  m_samplers.clear();
  m_allocator = nullptr;
}

Buffer Allocator::allocateBuffer(vk::DeviceSize size, BufferIntent intent) {
  vk::BufferCreateInfo bufferCreateInfo{
    .size         = size,
    .usage        = vk::BufferUsageFlagBits::eStorageBuffer       |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress |
                    vk::BufferUsageFlagBits::eTransferSrc         |
                    vk::BufferUsageFlagBits::eTransferDst         |
                    vk::BufferUsageFlagBits::eIndirectBuffer
  };

  auto [usage, flags] = getVMAflags(intent);
  VmaAllocationCreateInfo allocationCreateInfo {
    .flags = flags,
    .usage = usage
  };

  VkBuffer cBuffer;
  VmaAllocation allocation;
  auto res = vk::Result(vmaCreateBuffer(
    m_allocator,
    reinterpret_cast<const VkBufferCreateInfo *>(&bufferCreateInfo),
    &allocationCreateInfo,
    &cBuffer,
    &allocation,
    nullptr
  ));

  if (res != vk::Result::eSuccess)
    GRF_PANIC("Failed to create buffer: {}", vk::to_string(res));

  vk::Buffer buffer(cBuffer);
  vk::DeviceAddress address = m_device.getBufferAddress({ .buffer = buffer });

  auto impl = std::make_shared<Buffer::Impl>(
    m_resourceManager,
    allocation,
    buffer,
    address,
    size,
    intent
  );

  {
    std::lock_guard<std::mutex> g(m_mutex);
    m_buffers[address] = impl;
  }
  return Buffer(impl);
}

std::shared_ptr<Image> Allocator::allocateImage(const ImageAllocInfo& info) {
  vk::ImageCreateInfo imageCreateInfo{
    .imageType    = info.type,
    .format       = info.format,
    .extent       = {
      .width  = info.width,
      .height = info.height,
      .depth  = info.depth
    },
    .mipLevels    = 1,
    .arrayLayers  = 1,
    .samples      = vk::SampleCountFlagBits::e1,
    .tiling       = vk::ImageTiling::eOptimal,
    .usage        = info.usage
  };
  if (info.isCubemap) {
    imageCreateInfo.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    imageCreateInfo.arrayLayers = 6;
  }

  VmaAllocationCreateInfo allocationCreateInfo{
    .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
  };

  VmaAllocation allocation;
  VkImage cImage;
  auto res = vk::Result(vmaCreateImage(
    m_allocator,
    reinterpret_cast<VkImageCreateInfo *>(&imageCreateInfo),
    &allocationCreateInfo,
    &cImage,
    &allocation,
    nullptr
  ));

  if (res != vk::Result::eSuccess)
    GRF_PANIC("Failed to allocate image: {}", vk::to_string(res));

  vk::Image image(cImage);
  auto req = m_device.getImageMemoryRequirements(image);

  auto impl = std::make_shared<Image>(m_resourceManager, ImageInfo{
    .id     = m_nextImageID,
    .alloc  = allocation,
    .image  = image,
    .format = info.format,
    .size   = req.size,
    .width  = info.width,
    .height = info.height,
    .depth  = info.depth
  });

  m_images[m_nextImageID++] = impl;
  return impl;
}

std::shared_ptr<Sampler::Impl> Allocator::createSampler(const SamplerSettings& settings) {
  if (settings.anisotropicFiltering && !m_anisotropySupport)
    log::warning("GPU does not support anisotropic filter. Sampler will not be created with it enabled");

  SamplerSettings s = settings;
  s.anisotropicFiltering = m_anisotropySupport && settings.anisotropicFiltering;

  vk::Sampler sampler = m_device.createSampler(vk::SamplerCreateInfo{
    .magFilter    = static_cast<vk::Filter>(s.magFilter),
    .minFilter    = static_cast<vk::Filter>(s.minFilter),
    .addressModeU = static_cast<vk::SamplerAddressMode>(s.uMode),
    .addressModeV = static_cast<vk::SamplerAddressMode>(s.vMode),
    .addressModeW = static_cast<vk::SamplerAddressMode>(s.wMode),
    .anisotropyEnable = s.anisotropicFiltering,
    .maxAnisotropy    = m_maxAnisotropy
  });

  auto impl = std::make_shared<Sampler::Impl>(m_nextSamplerID, sampler, s);

  m_samplers[m_nextSamplerID++] = impl;
  return impl;
}

std::optional<vk::Buffer> Allocator::writeBuffer(
  vk::DeviceAddress address, std::span<const std::byte> data, std::size_t offset
) {
  std::lock_guard<std::mutex> g(m_mutex);

  if (!m_buffers.contains(address))
    GRF_PANIC("Failed to write buffer. {:#X} not found in allocation map", address);

  std::shared_ptr<Buffer::Impl> impl = m_buffers.at(address);

  VkMemoryPropertyFlags cFlags;
  vmaGetAllocationMemoryProperties(m_allocator, impl->m_allocation, &cFlags);
  vk::MemoryPropertyFlags flags(cFlags);

  if (flags & vk::MemoryPropertyFlagBits::eHostVisible) {
    vmaCopyMemoryToAllocation(
      m_allocator,
      data.data(),
      impl->m_allocation,
      offset, data.size()
    );
    return std::nullopt;
  }

  return std::make_optional(stage(data));
}

void Allocator::readBuffer(vk::DeviceAddress address, std::span<std::byte> data, std::size_t offset) {
  std::lock_guard<std::mutex> g(m_mutex);

  if (!m_buffers.contains(address))
    GRF_PANIC("Failed to read buffer. {:#X} not found in allocation map", address);

  auto impl = m_buffers.at(address);

  vmaCopyAllocationToMemory(m_allocator, impl->m_allocation, offset, data.data(), data.size());
}

vk::Buffer Allocator::stage(std::span<const std::byte> data) {
  std::lock_guard<std::mutex> g(m_mutex);

  vk::BufferCreateInfo bufferCreateInfo{
    .size   = data.size(),
    .usage  = vk::BufferUsageFlagBits::eTransferSrc,
  };

  VmaAllocationCreateInfo allocationCreateInfo{
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    .usage = VMA_MEMORY_USAGE_AUTO
  };

  VkBuffer cBuffer;
  VmaAllocation allocation;
  auto res = vk::Result(vmaCreateBuffer(
    m_allocator,
    reinterpret_cast<VkBufferCreateInfo *>(&bufferCreateInfo),
    &allocationCreateInfo,
    &cBuffer,
    &allocation,
    nullptr
  ));

  if (res != vk::Result::eSuccess)
    GRF_PANIC("Failed to allocate staging buffer: {}", vk::to_string(res));

  m_staging[m_nextStagingBuffer] = std::make_pair(allocation, vk::Buffer(cBuffer));
  vmaCopyMemoryToAllocation(
    m_allocator,
    data.data(),
    allocation,
    0,
    data.size()
  );

  return m_staging.at(m_nextStagingBuffer++).second;
}

void Allocator::destroyStagingBuffers() {
  for (auto& [id, allocInfo] : m_staging) {
    auto [allocation, buffer] = allocInfo;
    vmaDestroyBuffer(m_allocator, buffer, allocation);
  }
  m_staging.clear();
  m_nextStagingBuffer = 0;
}

std::pair<VmaMemoryUsage, VmaAllocationCreateFlags> Allocator::getVMAflags(BufferIntent intent) const {
  switch (intent) {
    case BufferIntent::GPUOnly:
    case BufferIntent::SingleUpdate:
      return {
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
      };
    case BufferIntent::FrequentUpdate:
      return {
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
      };
    case BufferIntent::Readable:
      return {
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
      };
  }
}

}