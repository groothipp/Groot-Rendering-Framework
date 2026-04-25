#pragma once

#include <vulkan/vulkan.hpp>

namespace grf {

class Pipeline {
public:
  uint64_t            m_id = 0xFFFFFFFFFFFFFFFF;
  vk::PipelineLayout  m_layout = nullptr;
  vk::Pipeline        m_pipeline = nullptr;

public:
  Pipeline(uint64_t, vk::PipelineLayout, vk::Pipeline);
};

}