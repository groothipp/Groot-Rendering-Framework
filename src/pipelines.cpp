#include "public/pipelines.hpp"

#include "internal/graveyard.hpp"
#include "internal/pipelines.hpp"
#include "internal/resources/resource_manager.hpp"


namespace grf {

ComputePipeline::ComputePipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

bool ComputePipeline::valid() const {
  return m_impl != nullptr && m_impl->m_pipeline != nullptr;
}

GraphicsPipeline::GraphicsPipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

bool GraphicsPipeline::valid() const {
  return m_impl != nullptr && m_impl->m_pipeline != nullptr;
}

Pipeline::Pipeline(
  std::weak_ptr<ResourceManager> resourceManager,
  vk::PipelineLayout layout, vk::Pipeline pipeline
)
: m_resourceManager(resourceManager), m_layout(layout), m_pipeline(pipeline)
{}

Pipeline::~Pipeline() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind     = ResourceKind::Pipeline,
    .retireAt = m_lastUseFrame + rm->flightFrames(),
    .pipeline = m_pipeline
  });
}

}
