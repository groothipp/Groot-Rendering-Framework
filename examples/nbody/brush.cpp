#include "nbody.hpp"

#include <format>

Brush::Brush(GRF& grf, const std::string& shadersFolderName) {
  std::string dir = std::format("{}/{}", SHADERS, shadersFolderName);

  m_vertShader = grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", dir));
  m_fragShader = grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", dir));

  BlendState alphaBlend{
    .enable = true,
    .srcColorFactor = BlendFactor::SrcAlpha,
    .dstColorFactor = BlendFactor::DstAlpha
  };

  m_pipeline = grf.createGraphicsPipeline(m_vertShader, m_fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { alphaBlend }
  });
}

void Brush::render(CommandBuffer& cmd, const Data& data) {
  cmd.bindPipeline(m_pipeline);
  cmd.push(data);
  cmd.draw(6);
}