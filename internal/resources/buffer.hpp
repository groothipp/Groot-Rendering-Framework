#pragma once

#include "public/resources.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <memory>

namespace grf {

class ResourceManager;

class Buffer::Impl {
public:
  std::weak_ptr<ResourceManager>  m_resourceManager;

  VmaAllocation     m_allocation = nullptr;
  vk::Buffer        m_buffer = nullptr;
  vk::DeviceAddress m_address = 0x0;
  vk::DeviceSize    m_size = 0;
  BufferIntent      m_intent = BufferIntent::FrequentUpdate;
  uint64_t          m_lastUseFrame = 0;

public:
  Impl(
    std::weak_ptr<ResourceManager>, VmaAllocation, vk::Buffer,
    vk::DeviceAddress, vk::DeviceSize, BufferIntent
  );
  ~Impl();
};

}
