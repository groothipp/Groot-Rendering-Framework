#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>

namespace grf {

class ResourceManager;

class Pipeline {
public:
  std::weak_ptr<ResourceManager> m_resourceManager;

  vk::PipelineLayout  m_layout = nullptr;
  vk::Pipeline        m_pipeline = nullptr;
  std::array<uint64_t, 3> m_lastUseValues = { 0, 0, 0 };

public:
  Pipeline(std::weak_ptr<ResourceManager>, vk::PipelineLayout, vk::Pipeline);
  ~Pipeline();
};

}
