#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>

namespace grf {

enum class ResourceKind {
  Buffer,
  Image,
  Sampler,
  Pipeline,
  Fence,
  Semaphore,
  CommandBuffer
};

struct Grave {
  ResourceKind            kind;
  std::array<uint64_t, 3> retireValues = { 0, 0, 0 };

  vk::Buffer              buffer = nullptr;
  vk::Image               image = nullptr;
  vk::ImageView           view = nullptr;
  vk::Sampler             sampler = nullptr;
  vk::Pipeline            pipeline = nullptr;
  vk::Fence               fence = nullptr;
  vk::Semaphore           semaphore = nullptr;
  vk::CommandBuffer       commandBuffer = nullptr;
  vk::CommandPool         commandPool = nullptr;
  VmaAllocation           allocation = nullptr;

  vk::DeviceAddress       address = 0;
  uint64_t                imageId = 0xFFFFFFFFFFFFFFFF;
  uint64_t                samplerId = 0xFFFFFFFFFFFFFFFF;

  uint32_t                storageBinding = 0xFFFFFFFF;
  uint32_t                storageSlot    = 0xFFFFFFFF;
  uint32_t                sampledBinding = 0xFFFFFFFF;
  uint32_t                sampledSlot    = 0xFFFFFFFF;
};

}
