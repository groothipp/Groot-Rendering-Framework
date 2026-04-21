#include "internal/allocator.hpp"
#include "internal/buffer.hpp"
#include "internal/image.hpp"
#include "internal/log.hpp"
#include "vulkan/vulkan.hpp"

namespace grf {

Allocator::Allocator(
  const vk::Instance& instance,
  const vk::PhysicalDevice& gpu,
  vk::Device& device,
  uint32_t apiVersion
) : m_device(device) {
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
    allocation,
    buffer,
    address,
    size,
    intent
  );

  m_buffers[address] = impl;
  return Buffer(impl);
}

std::shared_ptr<Image::Impl> Allocator::allocateImage(
  vk::ImageType type, vk::Format format, vk::ImageUsageFlags usage,
  uint32_t width, uint32_t height, uint32_t depth
) {
  vk::ImageCreateInfo imageCreateInfo{
    .imageType    = type,
    .format       = format,
    .extent       = {
      .width  = width,
      .height = height,
      .depth  = depth
    },
    .mipLevels    = 1,
    .arrayLayers  = 1,
    .samples      = vk::SampleCountFlagBits::e1,
    .tiling       = vk::ImageTiling::eOptimal,
    .usage        = usage
  };

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

  auto impl = std::make_shared<Image::Impl>(
    m_nextImageID,
    allocation,
    image,
    format,
    req.size,
    width,
    height,
    depth
  );

  m_images[m_nextImageID++] = impl;
  return impl;
}

std::shared_ptr<Image::Impl> Allocator::allocateCubemap(vk::Format format, uint32_t width, uint32_t height) {
  vk::ImageCreateInfo imageCreateInfo{
    .flags        = vk::ImageCreateFlagBits::eCubeCompatible,
    .imageType    = vk::ImageType::e2D,
    .format       = format,
    .extent       = {
      .width  = width,
      .height = height,
      .depth  = 1
    },
    .mipLevels    = 1,
    .arrayLayers  = 6,
    .samples      = vk::SampleCountFlagBits::e1,
    .tiling       = vk::ImageTiling::eOptimal,
    .usage        = vk::ImageUsageFlagBits::eSampled      |
                    vk::ImageUsageFlagBits::eTransferDst  |
                    vk::ImageUsageFlagBits::eTransferSrc
  };

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
    GRF_PANIC("Failed to allocate cubemap: {}", vk::to_string(res));

  vk::Image image(cImage);
  auto req = m_device.getImageMemoryRequirements(image);

  auto impl = std::make_shared<Image::Impl>(
    m_nextImageID,
    allocation,
    image,
    format,
    req.size,
    width,
    height,
    6
  );

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

std::pair<VmaMemoryUsage, VmaAllocationCreateFlags> Allocator::getVMAflags(BufferIntent intent) const {
  switch (intent) {
    case BufferIntent::GPUOnly:
    case BufferIntent::SingleUpdate:
      return { VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0 };
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