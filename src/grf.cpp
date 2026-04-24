#include "public/enums.hpp"

#include "internal/allocator.hpp"
#include "internal/descriptor_heap.hpp"
#include "internal/grf.hpp"
#include "internal/log.hpp"
#include "vulkan/vulkan.hpp"

#include "external/stb/stb_image.h"

#include <vulkan/vulkan_beta.h>

#include <cstring>
#include <sstream>
#include <set>

std::tuple<uint32_t, uint32_t, uint32_t> parseVersionString(const std::string& versionString) {
  std::stringstream ss(versionString, '.');

  uint32_t major, minor, patch;
  ss >> major >> minor >> patch;

  return { major, minor, patch };
}

namespace grf {

GRF::GRF(const Settings& settings) {
  m_impl = std::make_unique<Impl>(settings);
}

GRF::~GRF() = default;

void GRF::run(std::function<void(double)> main) {
  while (!glfwWindowShouldClose(m_impl->m_window)) {
    glfwPollEvents();
    m_impl->m_resourceManager->beginUpdates();
    main(0.0);
  }
  m_impl->m_device.waitIdle();
}

void GRF::beginResourceUpdates() {
  m_impl->m_resourceManager->beginUpdates();
}

void GRF::waitForResourceUpdates() {
  m_impl->m_resourceManager->waitForUpdates();
}

Shader GRF::compileShader(ShaderType type, const std::string& path) {
  return m_impl->m_shaderManager->compile(type, path);
}

Buffer GRF::createBuffer(BufferIntent intent, std::size_t size) {
  return m_impl->m_allocator->allocateBuffer(size, intent);
}

Tex2D GRF::createTex2D(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e2D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e2D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addTex2D(img);

  return Tex2D(img);
}

Tex3D GRF::createTex3D(Format format, uint32_t width, uint32_t height, uint32_t depth) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e3D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height,
    .depth  = depth
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e3D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addTex3D(img);

  return Tex3D(img);
}

Cubemap GRF::createCubemap(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type       = vk::ImageType::e2D,
    .format     = static_cast<vk::Format>(format),
    .usage      = vk::ImageUsageFlagBits::eSampled      |
                  vk::ImageUsageFlagBits::eTransferSrc  |
                  vk::ImageUsageFlagBits::eTransferDst,
    .width      = width,
    .height     = height,
    .isCubemap  = true
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::eCube,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 6
    }
  });

  m_impl->m_descriptorHeap->addCubemap(img);

  return Cubemap(img);
}

Img2D GRF::createImg2D(Format format, uint32_t width, uint32_t height) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type = vk::ImageType::e2D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eStorage      |
              vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e2D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addImg2D(img);

  return Img2D(img);
}

Img3D GRF::createImg3D(Format format, uint32_t width, uint32_t height, uint32_t depth) {
  auto img = m_impl->m_allocator->allocateImage(ImageAllocInfo{
    .type   = vk::ImageType::e3D,
    .format = static_cast<vk::Format>(format),
    .usage  = vk::ImageUsageFlagBits::eStorage      |
              vk::ImageUsageFlagBits::eSampled      |
              vk::ImageUsageFlagBits::eTransferSrc  |
              vk::ImageUsageFlagBits::eTransferDst,
    .width  = width,
    .height = height,
    .depth  = depth
  });

  img->m_view = m_impl->m_device.createImageView(vk::ImageViewCreateInfo{
    .image      = img->m_image,
    .viewType   = vk::ImageViewType::e3D,
    .format     = img->m_format,
    .components = {
      .r = vk::ComponentSwizzle::eIdentity,
      .g = vk::ComponentSwizzle::eIdentity,
      .b = vk::ComponentSwizzle::eIdentity,
      .a = vk::ComponentSwizzle::eIdentity
    },
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  });

  m_impl->m_descriptorHeap->addImg3D(img);

  return Img3D(img);
}

Sampler GRF::createSampler(const SamplerSettings& settings) {
  auto impl = m_impl->m_allocator->createSampler(settings);
  m_impl->m_descriptorHeap->addSampler(impl);
  return Sampler(impl);
}

GRF::Impl::Impl(const Settings& settings) : m_settings(settings) {
  std::vector<const char *> requiredExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  createWindow();
  createInstance();
  createSurface();
  chooseGPU(requiredExtensions);
  createDevice(requiredExtensions);

  m_graphicsQueue.queue = m_device.getQueue(m_graphicsQueue.index, 0);
  m_computeQueue.queue = m_device.getQueue(m_computeQueue.index, 0);
  m_transferQueue.queue = m_device.getQueue(m_transferQueue.index, 0);

  vk::PhysicalDeviceProperties properties = m_gpu.getProperties();

  m_allocator = std::make_unique<Allocator>(
    m_instance, m_gpu, m_device, properties.apiVersion, m_resourceManager
  );
  m_descriptorHeap = std::make_unique<DescriptorHeap>(m_gpu, m_device);
  m_shaderManager = std::make_unique<ShaderManager>(m_device);
  m_resourceManager = std::make_unique<ResourceManager>(m_allocator, m_transferQueue, m_device);

  log::generic("Groot Rendering Framework {}\nvulkan {}.{}.{} on {}",
    std::string(GRF_VERSION),
    VK_VERSION_MAJOR(properties.apiVersion),
    VK_VERSION_MINOR(properties.apiVersion),
    VK_VERSION_PATCH(properties.apiVersion),
    std::string(properties.deviceName)
  );
}

GRF::Impl::~Impl() {
  m_resourceManager->destroy();
  m_shaderManager->destroy();
  m_descriptorHeap->destroy();
  m_allocator->destroy();

  m_device.destroy();

  m_instance.destroySurfaceKHR(m_surface);
  m_instance.destroy();

  glfwDestroyWindow(m_window);
  glfwTerminate();
}

void GRF::Impl::createWindow() {
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
      !f13.dynamicRendering                             ||
      !f13.synchronization2
    ) continue;

    m_gpu = gpu;
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

  vk::PhysicalDeviceVulkan13Features f13{
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

ImageData readImage(const std::string& path) {
  int width = 0, height = 0, channels = 0;
  stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  if (pixels == nullptr)
    GRF_PANIC("Failed to load image '{}': {}", path, stbi_failure_reason());

  const std::size_t byteCount = static_cast<std::size_t>(width) * height * 4;
  std::vector<std::byte> bytes(byteCount);
  std::memcpy(bytes.data(), pixels, byteCount);

  stbi_image_free(pixels);
  return {
    .bytes  = std::move(bytes),
    .width  = static_cast<uint32_t>(width),
    .height = static_cast<uint32_t>(height),
  };
}

}