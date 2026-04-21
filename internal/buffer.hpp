#pragma once

#include "public/buffer.hpp"
#include "public/enums.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace grf {

class Buffer::Impl {
public:
  VmaAllocation     m_allocation = nullptr;
  vk::Buffer        m_buffer = nullptr;
  vk::DeviceAddress m_address = 0x0;
  vk::DeviceSize    m_size = 0;
  BufferIntent      m_intent = BufferIntent::FrequentUpdate;

public:
  Impl(VmaAllocation, vk::Buffer, vk::DeviceAddress, vk::DeviceSize, BufferIntent);
};

}