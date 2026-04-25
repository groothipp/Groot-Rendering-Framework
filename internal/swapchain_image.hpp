#pragma once

#include "public/resources.hpp"

#include <vulkan/vulkan.hpp>

namespace grf {

class SwapchainImage::Impl {
public:
  vk::Image     m_image;
  vk::ImageView m_view;
  uint32_t      m_heapIndexStorage = 0xFFFFFFFF;

public:
  Impl(vk::Image, vk::ImageView, uint32_t = 0xFFFFFFFF);
};

}