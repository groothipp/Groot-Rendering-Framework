#include "public/pipelines.hpp"

#include "internal/pipelines.hpp"


namespace grf {

ComputePipeline::ComputePipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

bool ComputePipeline::valid() const {
  return m_impl != nullptr && m_impl->m_pipeline != nullptr;
}

GraphicsPipeline::GraphicsPipeline(std::shared_ptr<Pipeline> impl) : m_impl(impl) {}

bool GraphicsPipeline::valid() const {
  return m_impl != nullptr && m_impl->m_pipeline != nullptr;
}

Pipeline::Pipeline(uint64_t id, vk::PipelineLayout layout, vk::Pipeline pipeline)
: m_id(id), m_layout(layout), m_pipeline(pipeline)
{}

}