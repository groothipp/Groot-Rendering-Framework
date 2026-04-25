#pragma once

#include "public/resources.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace grf {

class ResourceManager;

class Buffer::Impl {
public:
  std::unique_ptr<ResourceManager>& m_resourceManager;

  VmaAllocation     m_allocation = nullptr;
  vk::Buffer        m_buffer = nullptr;
  vk::DeviceAddress m_address = 0x0;
  vk::DeviceSize    m_size = 0;
  BufferIntent      m_intent = BufferIntent::FrequentUpdate;

public:
  Impl(
    std::unique_ptr<ResourceManager>&, VmaAllocation, vk::Buffer,
    vk::DeviceAddress, vk::DeviceSize, BufferIntent
  );
};

}