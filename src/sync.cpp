#include "internal/sync.hpp"

#include "internal/graveyard.hpp"
#include "internal/resources/resource_manager.hpp"

namespace grf {

Fence::Fence(std::shared_ptr<Impl> impl) : m_impl(impl) {}

Fence::Impl::Impl(std::weak_ptr<ResourceManager> resourceManager, vk::Fence fence)
: m_resourceManager(resourceManager), m_fence(fence)
{}

Fence::Impl::~Impl() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind         = ResourceKind::Fence,
    .retireValues = m_lastUseValues,
    .fence        = m_fence
  });
}

Semaphore::Semaphore(std::shared_ptr<Impl> impl) : m_impl(impl) {}

Semaphore::Impl::Impl(std::weak_ptr<ResourceManager> resourceManager, vk::Semaphore semaphore)
: m_resourceManager(resourceManager), m_semaphore(semaphore)
{}

Semaphore::Impl::~Impl() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind         = ResourceKind::Semaphore,
    .retireValues = m_lastUseValues,
    .semaphore    = m_semaphore
  });
}

}
