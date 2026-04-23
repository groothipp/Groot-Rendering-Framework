#pragma once

#include "public/image.hpp"
#include "public/structs.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace grf {

class Image::Impl {
public:
  uint64_t        m_id = 0xFFFFFFFFFFFFFFFF;
  VmaAllocation   m_allocation = nullptr;
  vk::Image       m_image = nullptr;
  vk::ImageView   m_view = nullptr;
  vk::Format      m_format = vk::Format::eR16G16B16A16Sfloat;
  vk::DeviceSize  m_size = 0;
  uint32_t        m_index = 0xFFFFFFFF;
  uint32_t        m_width = 0;
  uint32_t        m_height = 0;
  uint32_t        m_depth = 0;

public:
  Impl(
    uint64_t, VmaAllocation, vk::Image, vk::Format,
    vk::DeviceSize, uint32_t, uint32_t, uint32_t
  );
};

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