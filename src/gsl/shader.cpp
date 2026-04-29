#include "public/shader.hpp"

namespace grf {

Shader::Shader(ShaderType type, const std::string& path, std::array<uint32_t, 3> threadGroup)
: m_type(type), m_path(path), m_threadGroup(threadGroup)
{}

const ShaderType& Shader::type() const {
  return m_type;
}

const std::string& Shader::path() const {
  return m_path;
}

const std::array<uint32_t, 3>& Shader::threadGroup() const {
  return m_threadGroup;
}

bool Shader::valid() const {
  return m_path != "";
}

}