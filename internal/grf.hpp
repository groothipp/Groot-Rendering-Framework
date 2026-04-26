#pragma once

#include "public/grf.hpp"

#include "internal/gsl/shader_manager.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/allocator.hpp"
#include "internal/descriptor_heap.hpp"
#include "internal/gui.hpp"
#include "internal/input.hpp"
#include "internal/swapchain_image.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <memory>
#include <chrono>

namespace grf {

class GRF::Impl {
public:
  using SwapchainImages = std::vector<std::shared_ptr<SwapchainImage::Impl>>;
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::duration<double>;

  static constexpr uint64_t         g_timeout = 100000000ul;

  Settings                          m_settings;

  GLFWwindow *                      m_window = nullptr;
  vk::Instance                      m_instance = nullptr;
  vk::SurfaceKHR                    m_surface = nullptr;
  vk::PhysicalDevice                m_gpu = nullptr;
  vk::Device                        m_device = nullptr;
  Queue                             m_graphicsQueue;
  Queue                             m_computeQueue;
  Queue                             m_transferQueue;

  std::unique_ptr<Allocator>        m_allocator = nullptr;
  std::unique_ptr<DescriptorHeap>   m_descriptorHeap = nullptr;
  std::unique_ptr<ShaderManager>    m_shaderManager = nullptr;
  std::shared_ptr<ResourceManager>  m_resourceManager = nullptr;
  std::unique_ptr<Input>            m_input = nullptr;
  std::unique_ptr<GUI>              m_gui = nullptr;

  vk::SwapchainKHR                  m_swapchain = nullptr;
  vk::Extent2D                      m_swapchainExtent;
  vk::Format                        m_swapchainFormat = vk::Format::eUndefined;
  SwapchainImages                   m_swapchainImages;
  uint32_t                          m_pushConstantSize = 0;

  vk::PipelineLayout                m_pipelineLayout = nullptr;
  std::array<vk::CommandPool, 3>    m_commandPools = { nullptr, nullptr, nullptr };

  uint64_t                          m_frameIndex = 0;
  TimePoint                         m_startTime;
  TimePoint                         m_endTime;

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
  void createSwapchain();
  void createPipelineLayout();
  void createTimelineSemaphores();
  void createCommandPools();
};

}