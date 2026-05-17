#include "internal/swapchain_image.hpp"

#include <cstring>

namespace grf {

SwapchainImage::SwapchainImage(std::shared_ptr<Impl> impl) : m_impl(impl) {}

uint32_t SwapchainImage::heapIndex() const {
  return m_impl->m_heapIndexStorage;
}

Sync SwapchainImage::sync() const {
  Sync s;
  if (m_impl == nullptr || m_impl->m_acquireSem == nullptr) return s;

  VkSemaphore raw = m_impl->m_acquireSem;
  uint64_t handle = 0;
  std::memcpy(&handle, &raw, sizeof(VkSemaphore));
  s.m_handle = handle;
  s.m_kind   = 2;
  return s;
}

SwapchainImage::Impl::Impl(vk::Image image, vk::ImageView view, uint32_t index, uint32_t heapIndexStorage)
: m_image(image), m_view(view), m_index(index), m_heapIndexStorage(heapIndexStorage)
{}

}
