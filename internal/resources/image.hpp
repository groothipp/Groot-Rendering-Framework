#pragma once

#include "public/types.hpp"

#include <cstdint>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace grf {

class ResourceManager;

struct ImageInfo{
  uint64_t        id;
  VmaAllocation   alloc;
  vk::Image       image;
  vk::Format      format;
  vk::DeviceSize  size;
  uint32_t        width;
  uint32_t        height;
  uint32_t        depth;
};

class Image {
public:
  std::weak_ptr<ResourceManager> m_resourceManager;

  uint64_t        m_id = 0xFFFFFFFFFFFFFFFF;
  VmaAllocation   m_allocation = nullptr;
  vk::Image       m_image = nullptr;
  vk::ImageView   m_view = nullptr;
  vk::Format      m_format = vk::Format::eR16G16B16A16Sfloat;
  vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
  QueueType       m_owner = QueueType::Graphics;
  vk::DeviceSize  m_size = 0;
  uint32_t        m_heapIndexStorage = 0xFFFFFFFF;
  uint32_t        m_heapIndexSampled = 0xFFFFFFFF;
  uint32_t        m_width = 0;
  uint32_t        m_height = 0;
  uint32_t        m_depth = 0;
  uint64_t        m_lastUseFrame = 0;

public:
  Image(std::weak_ptr<ResourceManager>, const ImageInfo&);
  ~Image();
};

}
