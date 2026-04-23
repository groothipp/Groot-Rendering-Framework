#include "internal/resources/image.hpp"
#include "internal/log.hpp"

namespace grf {

Image::Image(std::weak_ptr<Image::Impl> impl) : m_impl(impl) {}

Format Image::format() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access format of invalid image");
  return static_cast<Format>(ptr->m_format);
}

std::tuple<uint32_t, uint32_t, uint32_t> Image::dims() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid image");
  return { ptr->m_width, ptr->m_height, ptr->m_depth };
}

std::size_t Image::size() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid image");
  return ptr->m_size;
}

uint32_t Image::heapIndex() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access heap index of invalid image");
  return ptr->m_index;
}

bool Image::valid() const {
  auto ptr = m_impl.lock();
  return ptr != nullptr;
}

Image::Impl::Impl(
  uint64_t id, VmaAllocation alloc, vk::Image image, vk::Format format,
  vk::DeviceSize size, uint32_t width, uint32_t height, uint32_t depth
) : m_id(id),
    m_allocation(alloc),
    m_image(image),
    m_format(format),
    m_size(size),
    m_width(width),
    m_height(height),
    m_depth(depth)
{}

Sampler::Sampler(std::weak_ptr<Sampler::Impl> impl) : m_impl(impl) {}

Filter Sampler::magFilter() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access mag filter on invalid sampler");
  return static_cast<Filter>(ptr->m_magFilter);
}

Filter Sampler::minFilter() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access min filter on invalid sampler");
  return static_cast<Filter>(ptr->m_minFilter);
}

SampleMode Sampler::uMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access u sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_uMode);
}

SampleMode Sampler::vMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access v sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_vMode);
}

SampleMode Sampler::wMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access w sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_wMode);
}

bool Sampler::anisotropicFiltering() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access anisotropic filtering on invalid sampler");
  return ptr->m_anisotropicFiltering;
}

uint32_t Sampler::heapIndex() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access heap index on invalid sampler");
  return ptr->m_index;
}

bool Sampler::valid() const {
  auto ptr = m_impl.lock();
  return ptr != nullptr;
}

Sampler::Impl::Impl(uint64_t id, vk::Sampler sampler, const SamplerSettings& settings)
: m_id(id), m_sampler(sampler) {
  m_magFilter = static_cast<vk::Filter>(settings.magFilter);
  m_minFilter = static_cast<vk::Filter>(settings.minFilter);
  m_uMode = static_cast<vk::SamplerAddressMode>(settings.uMode);
  m_vMode = static_cast<vk::SamplerAddressMode>(settings.vMode);
  m_wMode = static_cast<vk::SamplerAddressMode>(settings.wMode);
  m_anisotropicFiltering = settings.anisotropicFiltering;
}

}