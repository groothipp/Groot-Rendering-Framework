#include "internal/swapchain_image.hpp"

namespace grf {

SwapchainImage::SwapchainImage(std::shared_ptr<Impl> impl) : m_impl(impl) {}

uint32_t SwapchainImage::heapIndex() const {
  return m_impl->m_heapIndexStorage;
}

SwapchainImage::Impl::Impl(vk::Image image, vk::ImageView view, uint32_t index, uint32_t heapIndexStorage)
: m_image(image), m_view(view), m_index(index), m_heapIndexStorage(heapIndexStorage)
{}

}
