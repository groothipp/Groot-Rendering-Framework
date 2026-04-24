#include "internal/swapchain_image.hpp"

namespace grf {

SwapchainImage::SwapchainImage(std::shared_ptr<Impl> impl) : m_impl(impl) {}

SwapchainImage::Impl::Impl(vk::Image image, vk::ImageView view)
: m_image(image), m_view(view)
{}

}