#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <cstdint>

namespace grf {

enum class ResourceKind {
  Buffer,
  Image,
  Sampler,
  Pipeline
};

struct Grave {
  ResourceKind      kind;
  uint64_t          retireAt = 0;

  vk::Buffer        buffer = nullptr;
  vk::Image         image = nullptr;
  vk::ImageView     view = nullptr;
  vk::Sampler       sampler = nullptr;
  vk::Pipeline      pipeline = nullptr;
  VmaAllocation     allocation = nullptr;

  vk::DeviceAddress address = 0;
  uint64_t          imageId = 0xFFFFFFFFFFFFFFFF;
  uint64_t          samplerId = 0xFFFFFFFFFFFFFFFF;
};

}
