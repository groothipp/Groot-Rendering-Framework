#pragma once

#include "public/sync.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>

namespace grf {

class ResourceManager;

class Fence::Impl {
public:
  std::weak_ptr<ResourceManager>  m_resourceManager;
  vk::Fence                       m_fence = nullptr;
  std::array<uint64_t, 3>         m_lastUseValues = { 0, 0, 0 };

public:
  Impl(std::weak_ptr<ResourceManager>, vk::Fence);
  ~Impl();
};

class Semaphore::Impl {
public:
  std::weak_ptr<ResourceManager>  m_resourceManager;
  vk::Semaphore                   m_semaphore = nullptr;
  std::array<uint64_t, 3>         m_lastUseValues = { 0, 0, 0 };

public:
  Impl(std::weak_ptr<ResourceManager>, vk::Semaphore);
  ~Impl();
};

}
