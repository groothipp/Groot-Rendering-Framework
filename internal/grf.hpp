#pragma once

#include "public/grf.hpp"

#include "internal/gsl/shader_manager.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/allocator.hpp"
#include "internal/descriptor_heap.hpp"
#include "internal/swapchain_image.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <memory>
#include <chrono>

namespace grf {

class GRF::Impl {
public:
  using FenceMap = std::unordered_map<uint64_t, std::shared_ptr<Fence::Impl>>;
  using SemaphoreMap = std::unordered_map<uint64_t, std::shared_ptr<Semaphore::Impl>>;
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

  vk::SwapchainKHR                  m_swapchain = nullptr;
  SwapchainImages                   m_swapchainImages;

  FenceMap                          m_fences;
  SemaphoreMap                      m_semaphores;
  uint64_t                          m_nextSyncIndex = 0;

  vk::PipelineLayout                m_pipelineLayout = nullptr;

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
};

}