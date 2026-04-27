#pragma once

#include "public/gui.hpp"

#include "internal/resources/resource_manager.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

namespace grf {

class GUI::Impl {
public:
  vk::Device         m_device          = nullptr;
  GLFWwindow *       m_window          = nullptr;
  VkFormat           m_colorFormat     = VK_FORMAT_UNDEFINED;
  bool               m_inFrame         = false;

  Impl(
    GLFWwindow *       window,
    vk::Instance       instance,
    vk::PhysicalDevice gpu,
    vk::Device         device,
    const Queue&       graphicsQueue,
    vk::Format         colorFormat,
    uint32_t           imageCount
  );
  ~Impl();

  void beginFrame();
};

}
