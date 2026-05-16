#include "internal/gsl/assembler.hpp"
#include "internal/log.hpp"

#include <format>

namespace grf::gsl {

namespace {

constexpr std::string_view kPreludeExtensions = R"(#version 460
#extension GL_EXT_buffer_reference          : require
#extension GL_EXT_nonuniform_qualifier      : require
#extension GL_EXT_scalar_block_layout       : require
#extension GL_EXT_shader_image_load_formatted : require
)";

constexpr std::string_view kPreludeHeapBindings = R"(
layout(set = 0, binding = 0) uniform texture2D    grf_Tex2D[];
layout(set = 0, binding = 1) uniform texture3D    grf_Tex3D[];
layout(set = 0, binding = 2) uniform textureCube  grf_Cubemap[];
layout(set = 0, binding = 3) uniform image2D      grf_Img2D[];
layout(set = 0, binding = 4) uniform image3D      grf_Img3D[];
layout(set = 0, binding = 5) uniform sampler      grf_Sampler[];
layout(set = 0, binding = 6) uniform image2DArray grf_CubemapStorage[];

)";

uint32_t locationCount(std::string_view type) {
  if (type == "mat2" || type == "mat2x2" || type == "mat2x3" || type == "mat2x4") return 2;
  if (type == "mat3" || type == "mat3x2" || type == "mat3x3" || type == "mat3x4") return 3;
  if (type == "mat4" || type == "mat4x2" || type == "mat4x3" || type == "mat4x4") return 4;

  if (type == "dmat2"   || type == "dmat2x2") return 2;
  if (type == "dmat2x3" || type == "dmat2x4") return 4;
  if (type == "dmat3x2") return 3;
  if (type == "dmat3"   || type == "dmat3x3" || type == "dmat3x4") return 6;
  if (type == "dmat4x2") return 4;
  if (type == "dmat4"   || type == "dmat4x3" || type == "dmat4x4") return 8;

  if (type == "dvec3" || type == "dvec4") return 2;

  return 1;
}

}

Assembler::Assembler(const ParsedSource& parsed, ShaderType type)
: m_parsed(parsed), m_type(type) {}

std::string Assembler::assemble() const {
  if (m_parsed.threadGroup.has_value() && m_type != ShaderType::Compute) {
    const auto& loc = m_parsed.threadGroup->loc;
    GRF_PANIC("{}:{}: 'thread_group' is only allowed in compute shaders",
              loc.row, loc.col);
  }

  std::string out;
  out.reserve(kPreludeExtensions.size() + kPreludeHeapBindings.size()
              + m_parsed.body.size() + 1024);

  out += kPreludeExtensions;
  for (const auto& ext : m_parsed.extensions) {
    out += ext;
    out += '\n';
  }
  out += kPreludeHeapBindings;

  if (m_parsed.threadGroup.has_value()) {
    const auto& [x, y, z] = m_parsed.threadGroup->dims;
    out += std::format(
      "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\n\n",
      x, y, z
    );
  }

  for (const auto& b : m_parsed.buffers) {
    if (b.qualifier.empty()) {
      out += std::format("layout(buffer_reference, std430) buffer {} {{{}}};\n",
                         b.typeName, b.body);
    } else {
      out += std::format("layout(buffer_reference, std430) {} buffer {} {{{}}};\n",
                         b.qualifier, b.typeName, b.body);
    }
  }
  if (!m_parsed.buffers.empty()) out += '\n';

  const bool hasPush = m_parsed.push.has_value() || !m_parsed.buffers.empty();
  if (hasPush) {
    out += "layout(push_constant, std430) uniform GrfPushBlock {\n";
    for (const auto& b : m_parsed.buffers) {
      out += std::format("  {} {};\n", b.typeName, b.instanceName);
    }
    if (m_parsed.push.has_value()) {
      out += m_parsed.push->body;
      if (!m_parsed.push->body.empty() && m_parsed.push->body.back() != '\n')
        out += '\n';
    }
    out += "};\n\n";

    for (const auto& b : m_parsed.buffers) {
      if (!b.anonymous) continue;
      for (const auto& field : b.fieldNames) {
        out += std::format("#define {} {}.{}\n", field, b.instanceName, field);
      }
    }
  }

  uint32_t inLoc = 0;
  for (const auto& v : m_parsed.ins) {
    if (v.interpolation.empty())
      out += std::format("layout(location = {}) in {} {};\n",
                         inLoc, v.type, v.name);
    else
      out += std::format("layout(location = {}) {} in {} {};\n",
                         inLoc, v.interpolation, v.type, v.name);
    inLoc += locationCount(v.type);
  }
  if (!m_parsed.ins.empty()) out += '\n';

  uint32_t outLoc = 0;
  for (const auto& v : m_parsed.outs) {
    if (v.interpolation.empty())
      out += std::format("layout(location = {}) out {} {};\n",
                         outLoc, v.type, v.name);
    else
      out += std::format("layout(location = {}) {} out {} {};\n",
                         outLoc, v.interpolation, v.type, v.name);
    outLoc += locationCount(v.type);
  }
  if (!m_parsed.outs.empty()) out += '\n';

  out += m_parsed.body;

  return out;
}

}
