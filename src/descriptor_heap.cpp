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
  for (auto& list : m_freeSlots) list.clear();
}

void DescriptorHeap::addTex2D(std::shared_ptr<Image> impl) {
  uint32_t slot = acquireSlot(g_tex2DBinding);

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_tex2DBinding,
    .dstArrayElement  = slot,
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = slot;
  impl->m_sampledBinding   = g_tex2DBinding;
}

void DescriptorHeap::addTex3D(std::shared_ptr<Image> impl) {
  uint32_t slot = acquireSlot(g_tex3DBinding);

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_tex3DBinding,
    .dstArrayElement  = slot,
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = slot;
  impl->m_sampledBinding   = g_tex3DBinding;
}

void DescriptorHeap::addCubemap(std::shared_ptr<Image> impl) {
  uint32_t slot = acquireSlot(g_cubemapBinding);

  vk::DescriptorImageInfo info{
    .imageView    = impl->m_view,
    .imageLayout  = vk::ImageLayout::eShaderReadOnlyOptimal
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_cubemapBinding,
    .dstArrayElement  = slot,
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampledImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_heapIndexSampled = slot;
  impl->m_sampledBinding   = g_cubemapBinding;
}

void DescriptorHeap::addImg2D(std::shared_ptr<Image> impl) {
  uint32_t storageSlot = acquireSlot(g_img2DBinding);
  uint32_t sampledSlot = acquireSlot(g_tex2DBinding);

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
      .dstArrayElement  = storageSlot,
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .pImageInfo       = &storageInfo
    },
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_tex2DBinding,
      .dstArrayElement  = sampledSlot,
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .pImageInfo       = &sampledInfo
    }
  };

  m_device.updateDescriptorSets(writes, nullptr);

  impl->m_heapIndexStorage = storageSlot;
  impl->m_storageBinding   = g_img2DBinding;
  impl->m_heapIndexSampled = sampledSlot;
  impl->m_sampledBinding   = g_tex2DBinding;
}

void DescriptorHeap::addImg3D(std::shared_ptr<Image> impl) {
  uint32_t storageSlot = acquireSlot(g_img3DBinding);
  uint32_t sampledSlot = acquireSlot(g_tex3DBinding);

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
      .dstArrayElement  = storageSlot,
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eStorageImage,
      .pImageInfo       = &storageInfo
    },
    vk::WriteDescriptorSet{
      .dstSet           = m_set,
      .dstBinding       = g_tex3DBinding,
      .dstArrayElement  = sampledSlot,
      .descriptorCount  = 1,
      .descriptorType   = vk::DescriptorType::eSampledImage,
      .pImageInfo       = &sampledInfo
    }
  };

  m_device.updateDescriptorSets(writes, nullptr);

  impl->m_heapIndexStorage = storageSlot;
  impl->m_storageBinding   = g_img3DBinding;
  impl->m_heapIndexSampled = sampledSlot;
  impl->m_sampledBinding   = g_tex3DBinding;
}

uint32_t DescriptorHeap::addImg2DStorageOnly(vk::ImageView view) {
  uint32_t slot = acquireSlot(g_img2DBinding);

  vk::DescriptorImageInfo info{
    .imageView    = view,
    .imageLayout  = vk::ImageLayout::eGeneral
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_img2DBinding,
    .dstArrayElement  = slot,
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eStorageImage,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  return slot;
}

void DescriptorHeap::addSampler(std::shared_ptr<Sampler::Impl> impl) {
  uint32_t slot = acquireSlot(g_samplerBinding);

  vk::DescriptorImageInfo info{
    .sampler    = impl->m_sampler
  };

  vk::WriteDescriptorSet write{
    .dstSet           = m_set,
    .dstBinding       = g_samplerBinding,
    .dstArrayElement  = slot,
    .descriptorCount  = 1,
    .descriptorType   = vk::DescriptorType::eSampler,
    .pImageInfo       = &info
  };

  m_device.updateDescriptorSets(1, &write, 0, nullptr);

  impl->m_index   = slot;
  impl->m_binding = g_samplerBinding;
}

uint32_t DescriptorHeap::acquireSlot(uint32_t binding) {
  auto& freeList = m_freeSlots[binding];
  if (!freeList.empty()) {
    uint32_t slot = freeList.back();
    freeList.pop_back();
    return slot;
  }

  if (m_nextIndex[binding] >= m_maxVal[binding])
    GRF_PANIC("Heap full at binding {}", binding);

  return m_nextIndex[binding]++;
}

void DescriptorHeap::releaseSlot(uint32_t binding, uint32_t slot) {
  if (slot == 0xFFFFFFFFu || binding == 0xFFFFFFFFu) return;
  m_freeSlots[binding].push_back(slot);
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