#include "nbody.hpp"
#include "public/types.hpp"

#include <format>

AABBDebug::AABBDebug(GRF& grf, const std::string& shadersFolderName) {
  std::string dir = std::format("{}/{}", SHADERS, shadersFolderName);

  m_vertShader = grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", dir));
  m_fragShader = grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", dir));

  BlendState alphaBlend{
    .enable         = true,
    .srcColorFactor = BlendFactor::SrcAlpha,
    .dstColorFactor = BlendFactor::OneMinusSrcAlpha,
  };

  m_pipeline = grf.createGraphicsPipeline(m_vertShader, m_fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .topology     = Topology::LineList,
    .cullMode     = CullMode::None,
    .blends       = { alphaBlend }
  });
}

void AABBDebug::render(CommandBuffer& cmd, const Data& data) {
  cmd.bindPipeline(m_pipeline);
  cmd.push(data);
  cmd.draw(8 * data.particleCount);
}