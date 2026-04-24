#include "internal/descriptor_heap.hpp"
#include "internal/log.hpp"

namespace grf {

DescriptorHeap::DescriptorHeap(const vk::PhysicalDevice& gpu, vk::Device& device) : m_device(device) {
  auto chain = gpu.getProperties2<
    vk::PhysicalDeviceProperties2,
    vk::PhysicalDeviceDescriptorIndexingProperties
  >();

  auto pIndex = chain.get<vk::PhysicalDeviceDescriptorIndexingProperties>();

  uint32_t maxTex = pIndex.maxDescriptorSetUpdateAfterBindSampledImages;
  uint32_t maxImg = pIndex.maxDescriptorSetUpdateAfterBindStorageImages;
  uint32_t maxSampler = pIndex.maxDescriptorSetUpdateAfterBindSamplers;

  createLayout(maxTex, maxImg, maxSampler);
  createPool(maxTex, maxImg, maxSampler);

  m_set = m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
    .descriptorPool     = m_pool,
    .descriptorSetCount = 1,
    .pSetLayouts        = &m_layout
  })[0];
}

DescriptorHeap::~DescriptorHeap() {
  if (m_pool != nullptr)
    destroy();
}

const vk::DescriptorSetLayout& DescriptorHeap::layout() const {
  return m_layout;
}

const vk::DescriptorSet& DescriptorHeap::set() const {
  return m_set;
}

void DescriptorHeap::destroy() {
  m_device.destroyDescriptorPool(m_pool);
  m_device.destroyDescriptorSetLayout(m_layout);

  m_pool = nullptr;
  m_layout = nullptr;
  m_maxVal = { 0 };
  m_nextIndex = { 0 };
}

void DescriptorHeap::addTex2D(std::shared_ptr<Image> impl) {
  if (m_nextIndex[g_tex2DBinding] == m_maxVal[g_tex2DBinding])
    GRF_PANIC("Failed to add Tex2D -- heap full");

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_tex2DBinding,
    .dstArrayElement  = m_nextIndex[g_tex2DBinding],
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = m_nextIndex[g_tex2DBinding]++;
}

void DescriptorHeap::addTex3D(std::shared_ptr<Image> impl) {
  if (m_nextIndex[g_tex3DBinding] == m_maxVal[g_tex3DBinding])
    GRF_PANIC("Failed to add Tex3D -- heap full");

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_tex3DBinding,
    .dstArrayElement  = m_nextIndex[g_tex3DBinding],
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = m_nextIndex[g_tex3DBinding]++;
}

void DescriptorHeap::addCubemap(std::shared_ptr<Image> impl) {
  if (m_nextIndex[g_cubemapBinding] == m_maxVal[g_cubemapBinding])
    GRF_PANIC("Failed to add Cubemap -- heap full");

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_cubemapBinding,
    .dstArrayElement  = m_nextIndex[g_cubemapBinding],
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = m_nextIndex[g_cubemapBinding]++;
}

void DescriptorHeap::addImg2D(std::shared_ptr<Image> impl) {
  if (m_nextIndex[g_img2DBinding] == m_maxVal[g_img2DBinding])
    GRF_PANIC("Failed to add Img2D -- heap full");

  vk::DescriptorImageInfo storageInfo{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eGeneral
  };

  vk::DescriptorImageInfo sampledInfo{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  std::array<vk::WriteDescriptorSet, 2> writes = {
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_img2DBinding,
      .dstArrayElement  = m_nextIndex[g_img2DBinding],
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .pImageInfo       = &storageInfo
    },
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_tex2DBinding,
      .dstArrayElement  = m_nextIndex[g_tex2DBinding],
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .pImageInfo       = &sampledInfo
    }
  };

  m_device.updateDescriptorSets(writes, nullptr);

  impl->m_heapIndexStorage = m_nextIndex[g_img2DBinding]++;
  impl->m_heapIndexSampled = m_nextIndex[g_tex2DBinding]++;
}

void DescriptorHeap::addImg3D(std::shared_ptr<Image> impl) {
  if (m_nextIndex[g_img3DBinding] == m_maxVal[g_img3DBinding])
    GRF_PANIC("Failed to add Img3D -- heap full");

  vk::DescriptorImageInfo storageInfo{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eGeneral
  };

  vk::DescriptorImageInfo sampledInfo{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  std::array<vk::WriteDescriptorSet, 2> writes = {
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_img3DBinding,
      .dstArrayElement  = m_nextIndex[g_img3DBinding],
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .pImageInfo       = &storageInfo
    },
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_tex3DBinding,
      .dstArrayElement  = m_nextIndex[g_tex3DBinding],
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .pImageInfo       = &sampledInfo
    }
  };

  m_device.updateDescriptorSets(writes, nullptr);

  impl->m_heapIndexStorage = m_nextIndex[g_img3DBinding]++;
  impl->m_heapIndexSampled = m_nextIndex[g_tex3DBinding]++;
}

void DescriptorHeap::addSampler(std::shared_ptr<Sampler::Impl> impl) {
  if (m_nextIndex[g_samplerBinding] == m_maxVal[g_samplerBinding])
    GRF_PANIC("Failed to add Sampler -- heap full");

  vk::DescriptorImageInfo info{
    .sampler    = impl->m_sampler
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_samplerBinding,
    .dstArrayElement  = m_nextIndex[g_samplerBinding],
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampler,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_index = m_nextIndex[g_samplerBinding]++;
}

void DescriptorHeap::createLayout(uint32_t maxTex, uint32_t maxImg, uint32_t maxSampler) {
  m_maxVal[g_tex2DBinding] = maxTex * 7 / 10;
  m_maxVal[g_tex3DBinding] = maxTex * 3 / 20;
  m_maxVal[g_cubemapBinding] = maxTex - m_maxVal[g_tex2DBinding] - m_maxVal[g_tex3DBinding];

  m_maxVal[g_img2DBinding] = maxImg * 4 / 5;
  m_maxVal[g_img3DBinding]= maxImg - m_maxVal[g_img2DBinding];

  m_maxVal[g_samplerBinding] = maxSampler;

  const std::array<vk::DescriptorSetLayoutBinding, 6> bindings{
    // grf_Tex2D[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_tex2DBinding,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .descriptorCount  = m_maxVal[g_tex2DBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    },
    // grf_Tex3D[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_tex3DBinding,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .descriptorCount  = m_maxVal[g_tex3DBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    },
    // grf_Cubemap[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_cubemapBinding,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .descriptorCount  = m_maxVal[g_cubemapBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    },
    // grf_Img2D[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_img2DBinding,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .descriptorCount  = m_maxVal[g_img2DBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    },
    // grf_Img3D[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_img3DBinding,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .descriptorCount  = m_maxVal[g_img3DBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    },
    // grf_Sampler[]
    vk::DescriptorSetLayoutBinding {
      .binding          = g_samplerBinding,
      .descriptorType   = vk::DescriptorType::eSampler,
      .descriptorCount  = m_maxVal[g_samplerBinding],
      .stageFlags       = vk::ShaderStageFlagBits::eAll
    }
  };

  constexpr vk::DescriptorBindingFlags commonFlags =
    vk::DescriptorBindingFlagBits::ePartiallyBound |
    vk::DescriptorBindingFlagBits::eUpdateAfterBind;

  std::array<vk::DescriptorBindingFlags, 6> bindingFlags;
  bindingFlags.fill(commonFlags);

  vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
    .bindingCount   = static_cast<uint32_t>(bindingFlags.size()),
    .pBindingFlags  = bindingFlags.data()
  };

  m_layout = m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
    .pNext        = &flagsInfo,
    .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings    = bindings.data()
  });
}

void DescriptorHeap::createPool(uint32_t maxTex, uint32_t maxImg, uint32_t maxSampler) {
  const std::array<vk::DescriptorPoolSize, 3> poolSizes = {
    vk::DescriptorPoolSize {
      .type             = vk::DescriptorType::eSampledImage,
      .descriptorCount  = maxTex,
    },
    vk::DescriptorPoolSize {
      .type             = vk::DescriptorType::eStorageImage,
      .descriptorCount  = maxImg,
    },
    vk::DescriptorPoolSize {
      .type             = vk::DescriptorType::eSampler,
      .descriptorCount  = maxSampler,
    }
  };

  m_pool = m_device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
    .flags          = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
    .maxSets        = 1,
    .poolSizeCount  = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes     = poolSizes.data()
  });
}

}