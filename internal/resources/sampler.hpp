#pragma once

#include "public/sampler.hpp"
#include "public/structs.hpp"

#include <vulkan/vulkan.hpp>

namespace grf {

class Sampler::Impl {
public:
  uint64_t                m_id = 0xFFFFFFFFFFFFFFFF;
  vk::Sampler             m_sampler = nullptr;
  uint32_t                m_index = 0xFFFFFFFF;

  vk::Filter              m_magFilter = vk::Filter::eLinear;
  vk::Filter              m_minFilter = vk::Filter::eLinear;
  vk::SamplerAddressMode  m_uMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode  m_vMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode  m_wMode = vk::SamplerAddressMode::eRepeat;
  bool                    m_anisotropicFiltering = true;

public:
  explicit Impl(uint64_t, vk::Sampler, const SamplerSettings&);
};
}