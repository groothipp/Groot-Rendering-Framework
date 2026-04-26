#include "public/pipelines.hpp"

#include "internal/graveyard.hpp"
#include "internal/pipelines.hpp"
#include "internal/resources/resource_manager.hpp"


namespace grf {

ComputePipeline::ComputePipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

GraphicsPipeline::GraphicsPipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

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
    .kind         = ResourceKind::Pipeline,
    .retireValues = m_lastUseValues,
    .pipeline     = m_pipeline
  });
}

}
