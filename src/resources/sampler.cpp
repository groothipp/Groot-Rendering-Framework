#include "internal/resources/sampler.hpp"
#include "internal/log.hpp"

namespace grf {

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