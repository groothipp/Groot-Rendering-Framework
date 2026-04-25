#pragma once

#include "public/resources.hpp"

#include <vulkan/vulkan.hpp>

namespace grf {

class SwapchainImage::Impl {
public:
  vk::Image m_image;
  vk::ImageView m_view;

public:
  Impl(vk::Image, vk::ImageView);
};

}