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
      out += std::format("  {} {};\n", b.typeName, b.instanceName);
    }
    out += "};\n\n";
  }

  uint32_t inLoc = 0;
  for (const auto& v : m_parsed.ins) {
    if (v.interpolation.empty())
      out += std::format("layout(location = {}) in {} {};\n",
                         inLoc++, v.type, v.name);
    else
      out += std::format("layout(location = {}) {} in {} {};\n",
                         inLoc++, v.interpolation, v.type, v.name);
  }
  if (!m_parsed.ins.empty()) out += '\n';

  uint32_t outLoc = 0;
  for (const auto& v : m_parsed.outs) {
    if (v.interpolation.empty())
      out += std::format("layout(location = {}) out {} {};\n",
                         outLoc++, v.type, v.name);
    else
      out += std::format("layout(location = {}) {} out {} {};\n",
                         outLoc++, v.interpolation, v.type, v.name);
  }
  if (!m_parsed.outs.empty()) out += '\n';

  out += m_parsed.body;

  return out;
}

}
