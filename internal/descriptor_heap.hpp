#pragma once

#include "internal/resources/image.hpp"
#include "internal/resources/sampler.hpp"

#include <vulkan/vulkan.hpp>

namespace grf {

class DescriptorHeap {
  static constexpr uint32_t g_tex2DBinding = 0;
  static constexpr uint32_t g_tex3DBinding = 1;
  static constexpr uint32_t g_cubemapBinding = 2;
  static constexpr uint32_t g_img2DBinding = 3;
  static constexpr uint32_t g_img3DBinding = 4;
  static constexpr uint32_t g_samplerBinding = 5;

  vk::Device& m_device;

  vk::DescriptorSetLayout m_layout = nullptr;
  vk::DescriptorPool m_pool = nullptr;
  vk::DescriptorSet m_set = nullptr;

  std::array<uint32_t, 6> m_maxVal = { 0 };
  std::array<uint32_t, 6> m_nextIndex = { 0 };

public:
  DescriptorHeap(const vk::PhysicalDevice&, vk::Device&);
  ~DescriptorHeap();

  const vk::DescriptorSetLayout& layout() const;
  const vk::DescriptorSet& set() const;

  void destroy();
  void addTex2D(std::shared_ptr<Image>);
  void addTex3D(std::shared_ptr<Image>);
  void addCubemap(std::shared_ptr<Image>);
  void addImg2D(std::shared_ptr<Image>);
  void addImg3D(std::shared_ptr<Image>);
  void addSampler(std::shared_ptr<Sampler::Impl>);
  uint32_t addImg2DStorageOnly(vk::ImageView);

private:
  void createLayout(uint32_t, uint32_t, uint32_t);
  void createPool(uint32_t, uint32_t, uint32_t);
};

}