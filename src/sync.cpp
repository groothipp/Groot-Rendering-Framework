#include "internal/sync.hpp"

namespace grf {

Fence::Fence(std::shared_ptr<Impl> impl) : m_impl(impl) {}

bool Fence::valid() const {
  return m_impl != nullptr;
}

Fence::Impl::Impl(uint64_t id, vk::Fence fence)
: m_id(id), m_fence(fence)
{}

Semaphore::Semaphore(std::shared_ptr<Impl> impl) : m_impl(impl) {}

bool Semaphore::valid() const {
  return m_impl != nullptr;
}

Semaphore::Impl::Impl(uint64_t id, vk::Semaphore semaphore)
: m_id(id), m_semaphore(semaphore)
{}

}