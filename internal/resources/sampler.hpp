#pragma once

#include "public/resources.hpp"
#include "public/types.hpp"

#include <memory>
#include <vulkan/vulkan.hpp>

namespace grf {

class ResourceManager;

class Sampler::Impl {
public:
  std::weak_ptr<ResourceManager> m_resourceManager;

  uint64_t                m_id = 0xFFFFFFFFFFFFFFFF;
  vk::Sampler             m_sampler = nullptr;
  uint32_t                m_index = 0xFFFFFFFF;
  uint64_t                m_lastUseFrame = 0;

  vk::Filter              m_magFilter = vk::Filter::eLinear;
  vk::Filter              m_minFilter = vk::Filter::eLinear;
  vk::SamplerAddressMode  m_uMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode  m_vMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode  m_wMode = vk::SamplerAddressMode::eRepeat;
  bool                    m_anisotropicFiltering = true;

public:
  Impl(std::weak_ptr<ResourceManager>, uint64_t, vk::Sampler, const SamplerSettings&);
  ~Impl();
};

}
