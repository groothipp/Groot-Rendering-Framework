#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace grf {

struct Queue {
  uint8_t index = 0xFF;
  vk::Queue queue = nullptr;
};

}