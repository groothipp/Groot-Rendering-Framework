#pragma once

#include "./structs.hpp"
#include "./shader_manager.hpp"
#include "./allocator.hpp"
#include "./descriptor_heap.hpp"

#include "public/gpu.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <memory>

namespace grf {

class GPU::Impl {
public:
  Settings                        m_settings;

  GLFWwindow *                    m_window = nullptr;
  vk::Instance                    m_instance = nullptr;
  vk::SurfaceKHR                  m_surface = nullptr;
  vk::PhysicalDevice              m_gpu = nullptr;
  vk::Device                      m_device = nullptr;
  Queue                           m_graphicsQueue;
  Queue                           m_computeQueue;
  Queue                           m_transferQueue;

  std::unique_ptr<Allocator>      m_allocator = nullptr;
  std::unique_ptr<DescriptorHeap> m_descriptorHeap = nullptr;
  std::unique_ptr<ShaderManager>  m_shaderManager = nullptr;


public:
  explicit Impl(const Settings&);
  ~Impl();

private:
  void createWindow();
  void createInstance();
  void createSurface();
  void chooseGPU(const std::vector<const char *>&);
  void getQueueFamilyIndices();
  void createDevice(std::vector<const char *>&);
};

}