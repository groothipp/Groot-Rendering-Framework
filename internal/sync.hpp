#pragma once

#include "public/sync.hpp"

#include <vulkan/vulkan.hpp>

namespace grf {

class Fence::Impl {
public:
  uint64_t  m_id = 0xFFFFFFFFFFFFFFFF;
  vk::Fence m_fence = nullptr;

public:
  Impl(uint64_t, vk::Fence);
};

class Semaphore::Impl {
public:
  uint64_t      m_id = 0xFFFFFFFFFFFFFFFF;
  vk::Semaphore m_semaphore = nullptr;

public:
  Impl(uint64_t, vk::Semaphore);
};

}