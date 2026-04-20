#include "public/shader.hpp"

namespace grf {

Shader::Shader(ShaderType type, const std::string& path)
: m_type(type), m_path(path)
{}

const ShaderType& Shader::type() const {
  return m_type;
}

const std::string& Shader::path() const {
  return m_path;
}

bool Shader::valid() const {
  return m_path != "";
}

}