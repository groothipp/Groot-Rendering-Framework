#include "internal/descriptor_heap.hpp"
#include "internal/grf.hpp"
#include "internal/log.hpp"

#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_beta.h>

#include <set>
#include <sstream>

namespace grf {

namespace {

std::tuple<uint32_t, uint32_t, uint32_t> parseVersionString(const std::string& versionString) {
  std::stringstream ss(versionString);
  std::string token;

  uint32_t parts[3] = { 0, 0, 0 };
  for (size_t i = 0; i < 3 && std::getline(ss, token, '.'); ++i) {
    parts[i] = static_cast<uint32_t>(std::stoul(token));
  }

  return { parts[0], parts[1], parts[2] };
}

}

void GRF::Impl::createWindow() {
#if defined(__linux__)
  glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
#endif

  if (!glfwInit()) GRF_PANIC("Failed to initialize GLFW");

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

  auto [width, height] = m_settings.windowSize;
  m_window = glfwCreateWindow(width, height, m_settings.windowTitle.c_str(), nullptr, nullptr);
  if (!m_window) GRF_PANIC("Failed to create window");
}

void GRF::Impl::createInstance() {
  auto [grfMajor, grfMinor, grfPatch] = parseVersionString(GRF_VERSION);
  auto [appMajor, appMinor, appPatch] = parseVersionString(m_settings.applicationVersion);

  vk::ApplicationInfo appInfo{
    .pApplicationName   = m_settings.windowTitle.c_str(),
    .applicationVersion = VK_MAKE_VERSION(appMajor, appMinor, appPatch),
    .pEngineName        = "Groot Rendering Framework",
    .engineVersion      = VK_MAKE_VERSION(grfMajor, grfMinor, grfPatch),
    .apiVersion         = VK_API_VERSION_1_4
  };

  uint32_t glfwExtensionCount;
  const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

  vk::InstanceCreateFlags flags;
  for (const auto& ext : vk::enumerateInstanceExtensionProperties()) {
    if (std::string(ext.extensionName) != VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) continue;

    extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

    break;
  }

  const char * validationLayerName = "VK_LAYER_KHRONOS_validation";

#ifdef GRF_DEBUG
  uint32_t layerCount = 1;
#else
  uint32_t layerCount = 0;
#endif

  m_instance = vk::createInstance(vk::InstanceCreateInfo{
    .flags                    = flags,
    .pApplicationInfo         = &appInfo,
    .enabledLayerCount        = layerCount,
    .ppEnabledLayerNames      = &validationLayerName,
    .enabledExtensionCount    = static_cast<uint32_t>(extensions.size()),
    .ppEnabledExtensionNames  = extensions.data(),
  });
}

void GRF::Impl::createSurface() {
  vk::SurfaceKHR::CType cSurface;
  if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &cSurface) != VK_SUCCESS)
    GRF_PANIC("Failed to create surface");

  m_surface = vk::SurfaceKHR(cSurface);
}

void GRF::Impl::chooseGPU(const std::vector<const char *>& requiredExtensions) {
  for (const auto& gpu : m_instance.enumeratePhysicalDevices()) {
    std::set<std::string> extensions;
    for (const auto& extension : gpu.enumerateDeviceExtensionProperties())
      extensions.emplace(extension.extensionName);

    bool hasAllExtensions = true;
    for (const auto& extension : requiredExtensions) {
      if (extensions.contains(extension)) continue;
      hasAllExtensions = false;
      break;
    }
    if (!hasAllExtensions) continue;

    auto featureChain = gpu.getFeatures2<
      vk::PhysicalDeviceFeatures2,
      vk::PhysicalDeviceVulkan12Features,
      vk::PhysicalDeviceVulkan13Features
    >();

    const auto& f2 = featureChain.get<vk::PhysicalDeviceFeatures2>();
    const auto& f12 = featureChain.get<vk::PhysicalDeviceVulkan12Features>();
    const auto& f13 = featureChain.get<vk::PhysicalDeviceVulkan13Features>();

    if (
      !f2.features.samplerAnisotropy                    ||
      !f2.features.shaderStorageImageReadWithoutFormat  ||
      !f2.features.shaderStorageImageWriteWithoutFormat ||
      !f12.bufferDeviceAddress                          ||
      !f12.timelineSemaphore                            ||
      !f12.descriptorIndexing                           ||
      !f12.runtimeDescriptorArray                       ||
      !f12.shaderSampledImageArrayNonUniformIndexing    ||
      !f12.shaderStorageImageArrayNonUniformIndexing    ||
      !f12.descriptorBindingSampledImageUpdateAfterBind ||
      !f12.descriptorBindingStorageImageUpdateAfterBind ||
      !f12.descriptorBindingUpdateUnusedWhilePending    ||
      !f12.descriptorBindingPartiallyBound              ||
      !f12.descriptorBindingVariableDescriptorCount     ||
      !f12.hostQueryReset                               ||
      !f12.shaderSubgroupExtendedTypes                  ||
      !f13.dynamicRendering                             ||
      !f13.synchronization2
    ) continue;

    m_gpu = gpu;

    bool hasRTExtensions =
      extensions.contains(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
      extensions.contains(VK_KHR_RAY_QUERY_EXTENSION_NAME)              &&
      extensions.contains(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    if (hasRTExtensions) {
      auto rtFeatures = gpu.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayQueryFeaturesKHR
      >();
      const auto& fAS = rtFeatures.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
      const auto& fRQ = rtFeatures.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
      m_rayTracingSupported = fAS.accelerationStructure && fRQ.rayQuery;
    }

    getQueueFamilyIndices();
    break;
  }

  if (m_gpu == nullptr)
    GRF_PANIC("No supported GPU found");
}

void GRF::Impl::getQueueFamilyIndices()  {
  uint8_t graphics = 0xFF, compute = 0xFF, transfer = 0xFF;
  auto queueFamilies = m_gpu.getQueueFamilyProperties();
  for (uint8_t i = 0; i < queueFamilies.size(); ++i) {
    auto& queueFamily = queueFamilies[i];

    if (
      graphics == 0xFF                                        &&
      (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) &&
      m_gpu.getSurfaceSupportKHR(i, m_surface)
    ) {
      graphics = i;
      continue;
    }

    if (
      compute == 0xFF                                         &&
      (queueFamily.queueFlags & vk::QueueFlagBits::eCompute)  &&
      !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
    ) {
      compute = i;
      continue;
    }

    if (
      transfer == 0xFF                                        &&
      (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
      !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
    ) {
      transfer = i;
      continue;
    }

    if (graphics != 0xFF && compute != 0xFF && transfer != 0xFF) break;
  }

  if (compute == 0xFF)
    compute = graphics;

  if (transfer == 0xFF)
    transfer = graphics;

  m_graphicsQueue.index = graphics;
  m_computeQueue.index = compute;
  m_transferQueue.index = transfer;
}

void GRF::Impl::createDevice(std::vector<const char *>& requiredExtensions) {
  const float queuePriority = 1.0f;

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos = {
    vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = m_graphicsQueue.index,
      .queueCount       = 1,
      .pQueuePriorities = &queuePriority
    }
  };

  if (m_computeQueue.index != m_graphicsQueue.index) {
    queueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = m_computeQueue.index,
      .queueCount       = 1,
      .pQueuePriorities = &queuePriority
    });
  }

  if (m_transferQueue.index != m_graphicsQueue.index) {
    queueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = m_transferQueue.index,
      .queueCount       = 1,
      .pQueuePriorities = &queuePriority
    });
  }

  for (const auto& extension : m_gpu.enumerateDeviceExtensionProperties()) {
    if (std::string(extension.extensionName) != VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) continue;

    requiredExtensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    break;
  }

  if (m_rayTracingSupported) {
    requiredExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    requiredExtensions.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    requiredExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  }

  vk::PhysicalDeviceRayQueryFeaturesKHR fRQ{
    .rayQuery = true
  };

  vk::PhysicalDeviceAccelerationStructureFeaturesKHR fAS{
    .pNext                 = &fRQ,
    .accelerationStructure = true
  };

  vk::PhysicalDeviceVulkan13Features f13{
    .pNext            = m_rayTracingSupported ? static_cast<void *>(&fAS) : nullptr,
    .synchronization2 = true,
    .dynamicRendering = true
  };

  vk::PhysicalDeviceVulkan12Features f12{
    .pNext                                        = &f13,
    .descriptorIndexing                           = true,
    .shaderSampledImageArrayNonUniformIndexing    = true,
    .shaderStorageImageArrayNonUniformIndexing    = true,
    .descriptorBindingSampledImageUpdateAfterBind = true,
    .descriptorBindingStorageImageUpdateAfterBind = true,
    .descriptorBindingUpdateUnusedWhilePending    = true,
    .descriptorBindingPartiallyBound              = true,
    .descriptorBindingVariableDescriptorCount     = true,
    .runtimeDescriptorArray                       = true,
    .shaderSubgroupExtendedTypes                  = true,
    .hostQueryReset                               = true,
    .timelineSemaphore                            = true,
    .bufferDeviceAddress                          = true
  };

  vk::PhysicalDeviceFeatures2 f2{
    .pNext = &f12,
    .features = {
      .samplerAnisotropy                    = true,
      .shaderStorageImageReadWithoutFormat  = true,
      .shaderStorageImageWriteWithoutFormat = true
    }
  };

  m_device = m_gpu.createDevice(vk::DeviceCreateInfo{
    .pNext                    = &f2,
    .queueCreateInfoCount     = static_cast<uint32_t>(queueCreateInfos.size()),
    .pQueueCreateInfos        = queueCreateInfos.data(),
    .enabledExtensionCount    = static_cast<uint32_t>(requiredExtensions.size()),
    .ppEnabledExtensionNames  = requiredExtensions.data(),
  });
}

void GRF::Impl::createSwapchain() {
  const vk::SurfaceCapabilitiesKHR caps = m_gpu.getSurfaceCapabilitiesKHR(m_surface);

  const vk::Format requestedFormat = static_cast<vk::Format>(m_settings.swapchainFormat);
  vk::SurfaceFormatKHR chosenFormat{ .format = vk::Format::eUndefined };
  for (const auto& candidate : m_gpu.getSurfaceFormatsKHR(m_surface)) {
    if (candidate.format == requestedFormat) {
      chosenFormat = candidate;
      break;
    }
  }
  if (chosenFormat.format == vk::Format::eUndefined)
    GRF_PANIC("Requested swapchain format {} is not supported by the surface", vk::to_string(requestedFormat));

  vk::PresentModeKHR requestedPresentMode;
  switch (m_settings.presentMode) {
    case PresentMode::VSync:     requestedPresentMode = vk::PresentModeKHR::eFifo;      break;
    case PresentMode::Mailbox:   requestedPresentMode = vk::PresentModeKHR::eMailbox;   break;
    case PresentMode::Immediate: requestedPresentMode = vk::PresentModeKHR::eImmediate; break;
  }
  vk::PresentModeKHR chosenPresentMode = vk::PresentModeKHR::eFifo;
  for (const auto& candidate : m_gpu.getSurfacePresentModesKHR(m_surface)) {
    if (candidate == requestedPresentMode) {
      chosenPresentMode = candidate;
      break;
    }
  }
  if (chosenPresentMode != requestedPresentMode)
    log::warning("Requested present mode {} not supported, falling back to FIFO",
      vk::to_string(requestedPresentMode));

  vk::Extent2D chosenExtent;
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    chosenExtent = caps.currentExtent;
  } else {
    auto [w, h] = m_settings.windowSize;
    chosenExtent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
    chosenExtent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  uint32_t imageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
    imageCount = caps.maxImageCount;

  const vk::FormatProperties formatProps = m_gpu.getFormatProperties(chosenFormat.format);
  const bool formatSupportsStorage = static_cast<bool>(
    formatProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eStorageImage
  );

  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
  if (caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
    usage |= vk::ImageUsageFlagBits::eTransferDst;
  if ((caps.supportedUsageFlags & vk::ImageUsageFlagBits::eStorage) && formatSupportsStorage)
    usage |= vk::ImageUsageFlagBits::eStorage;

  m_swapchainExtent = chosenExtent;
  m_swapchainFormat = chosenFormat.format;

  m_swapchain = m_device.createSwapchainKHR(vk::SwapchainCreateInfoKHR{
    .surface          = m_surface,
    .minImageCount    = imageCount,
    .imageFormat      = chosenFormat.format,
    .imageColorSpace  = chosenFormat.colorSpace,
    .imageExtent      = chosenExtent,
    .imageArrayLayers = 1,
    .imageUsage       = usage,
    .imageSharingMode = vk::SharingMode::eExclusive,
    .preTransform     = caps.currentTransform,
    .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
    .presentMode      = chosenPresentMode,
    .clipped          = vk::True,
    .oldSwapchain     = nullptr,
  });

  const bool storageSupported = static_cast<bool>(usage & vk::ImageUsageFlagBits::eStorage);

  auto imgs = m_device.getSwapchainImagesKHR(m_swapchain);
  for (uint32_t i = 0; i < imgs.size(); ++i) {
    const auto& img = imgs[i];
    auto view = m_device.createImageView(vk::ImageViewCreateInfo{
      .image    = img,
      .viewType = vk::ImageViewType::e2D,
      .format   = chosenFormat.format,
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .levelCount = 1,
        .layerCount = 1,
      },
    });

    uint32_t heapIndex = 0xFFFFFFFF;
    if (storageSupported)
      heapIndex = m_descriptorHeap->addImg2DStorageOnly(view);

    m_swapchainImages.emplace_back(std::make_shared<SwapchainImage::Impl>(img, view, i, heapIndex));
  }
}

void GRF::Impl::createTimelineSemaphores() {
  for (Queue * q : { &m_graphicsQueue, &m_computeQueue, &m_transferQueue }) {
    vk::SemaphoreTypeCreateInfo type{
      .semaphoreType = vk::SemaphoreType::eTimeline,
      .initialValue  = 0
    };
    q->timeline = m_device.createSemaphore(vk::SemaphoreCreateInfo{ .pNext = &type });
    q->nextValue = 1;
  }
}

void GRF::Impl::createCommandPools() {
  std::array<uint32_t, 3> indices = {
    m_graphicsQueue.index, m_computeQueue.index, m_transferQueue.index
  };
  for (size_t i = 0; i < 3; ++i) {
    m_commandPools[i] = m_device.createCommandPool(vk::CommandPoolCreateInfo{
      .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = indices[i]
    });
  }
}

void GRF::Impl::createPipelineLayout() {
  auto props = m_gpu.getProperties();
  m_pushConstantSize = props.limits.maxPushConstantsSize;

  const vk::DescriptorSetLayout& setLayout = m_descriptorHeap->layout();

  vk::PushConstantRange range{
    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
    .size       = props.limits.maxPushConstantsSize
  };

  m_pipelineLayout = m_device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
    .setLayoutCount         = 1,
    .pSetLayouts            = &setLayout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges    = &range
  });
}

}
