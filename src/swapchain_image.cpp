#include "internal/swapchain_image.hpp"
#include "internal/log.hpp"

namespace grf {

SwapchainImage::SwapchainImage(std::shared_ptr<Impl> impl) : m_impl(impl) {}

bool SwapchainImage::valid() const {
  return m_impl != nullptr;
}

uint32_t SwapchainImage::heapIndex() const {
  if (m_impl == nullptr)
    GRF_PANIC("Tried to access heap index of invalid SwapchainImage");
  return m_impl->m_heapIndexStorage;
}

SwapchainImage::Impl::Impl(vk::Image image, vk::ImageView view, uint32_t heapIndexStorage)
: m_image(image), m_view(view), m_heapIndexStorage(heapIndexStorage)
{}

}