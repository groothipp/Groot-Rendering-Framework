#include "internal/gsl/assembler.hpp"

#include <format>

namespace grf::gsl {

namespace {

constexpr std::string_view kHeader = R"(#version 460
#extension GL_EXT_buffer_reference          : require
#extension GL_EXT_nonuniform_qualifier      : require
#extension GL_EXT_scalar_block_layout       : require
#extension GL_EXT_shader_image_load_formatted : require

layout(set = 0, binding = 0) uniform texture2D   grf_Tex2D[];
layout(set = 0, binding = 1) uniform texture3D   grf_Tex3D[];
layout(set = 0, binding = 2) uniform textureCube grf_TexCube[];
layout(set = 0, binding = 3) uniform image2D     grf_StorageImg2D[];
layout(set = 0, binding = 4) uniform sampler     grf_Sampler[];

)";

}

Assembler::Assembler(const ParsedSource& parsed) : m_parsed(parsed) {}

std::string Assembler::assemble() const {
  std::string out;
  out.reserve(kHeader.size() + m_parsed.body.size() + 1024);

  out += kHeader;

  for (const auto& b : m_parsed.buffers) {
    out += std::format("layout(buffer_reference, std430) {} buffer {} {{{}}};\n",
                       b.qualifier, b.typeName, b.body);
  }
  if (!m_parsed.buffers.empty()) out += '\n';

  const bool hasPush = m_parsed.push.has_value() || !m_parsed.buffers.empty();
  if (hasPush) {
    out += "layout(push_constant, std430) uniform GrfPushBlock {\n";
    if (m_parsed.push.has_value()) {
      out += m_parsed.push->body;
      if (!m_parsed.push->body.empty() && m_parsed.push->body.back() != '\n')
        out += '\n';
    }
    for (const auto& b : m_parsed.buffers) {
      out += std::format("  uint64_t grf_addr_{};\n", b.instanceName);
    }
    out += "};\n\n";
  }

  for (const auto& b : m_parsed.buffers) {
    out += std::format("#define {} {}(grf_addr_{})\n",
                       b.instanceName, b.typeName, b.instanceName);
  }
  if (!m_parsed.buffers.empty()) out += '\n';

  out += m_parsed.body;

  return out;
}

}
