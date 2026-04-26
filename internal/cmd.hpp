#pragma once

#include "public/cmd.hpp"

#include <memory>
#include <vulkan/vulkan.hpp>

namespace grf {

class Image;
class ResourceManager;

struct ImageBits {
  vk::Image               image = nullptr;
  vk::ImageView           view  = nullptr;
  vk::ImageAspectFlags    aspect = vk::ImageAspectFlagBits::eColor;
  uint32_t                layerCount = 1;
  std::shared_ptr<Image>  impl;
};

class CommandBuffer::Impl {
public:
  std::weak_ptr<ResourceManager>  m_resourceManager;
  vk::Device                      m_device = nullptr;
  vk::CommandPool                 m_pool = nullptr;
  vk::CommandBuffer               m_buffer = nullptr;
  vk::PipelineLayout              m_pipelineLayout = nullptr;
  vk::DescriptorSet               m_descriptorSet = nullptr;
  vk::Extent2D                    m_swapchainExtent;
  uint32_t                        m_pushConstantSize = 0;
  QueueType                       m_queueType = QueueType::Graphics;
  uint64_t                        m_reservedValue = 0;

public:
  Impl(
    std::weak_ptr<ResourceManager>,
    vk::Device,
    vk::CommandPool,
    vk::CommandBuffer,
    vk::PipelineLayout,
    vk::DescriptorSet,
    vk::Extent2D swapchainExtent,
    uint32_t pushConstantSize,
    QueueType
  );
  ~Impl();
};

}
