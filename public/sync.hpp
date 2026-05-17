#pragma once

#include <cstdint>

namespace grf {

class GRF;
class CommandBuffer;
class ResourceManager;
class SwapchainImage;

class Sync {
  friend class GRF;
  friend class CommandBuffer;
  friend class ResourceManager;
  friend class SwapchainImage;

  // VkSemaphore handle stored as opaque uint64 to avoid including <vulkan/vulkan.hpp>
  // in public headers. Non-dispatchable Vulkan handles are 64-bit on all platforms.
  uint64_t m_handle = 0;
  uint64_t m_value  = 0;
  uint8_t  m_kind   = 0;  // 0 = invalid, 1 = timeline, 2 = binary

public:
  Sync() = default;
  bool valid() const { return m_kind != 0; }
};

}
