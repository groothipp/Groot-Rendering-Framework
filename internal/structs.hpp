#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace grf {

struct Queue {
  uint8_t index = 0xFF;
  vk::Queue queue = nullptr;
};

struct CacheRecord {
  uint64_t mtime = 0;
  uint64_t size = 0;
  std::string bin = "";
};

}