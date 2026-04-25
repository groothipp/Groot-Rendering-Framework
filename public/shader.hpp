#pragma once

#include "./types.hpp"

#include <string>

namespace grf {

class Shader {
  friend class ShaderManager;

  ShaderType  m_type = ShaderType::Vertex;
  std::string m_path = "";

public:
  const ShaderType& type() const;
  const std::string& path() const;
  bool valid() const;

private:
  Shader(ShaderType, const std::string&);
};

}