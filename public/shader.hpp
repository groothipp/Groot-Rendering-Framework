#pragma once

#include "./types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace grf {

class Shader {
  friend class ShaderManager;

  ShaderType              m_type        = ShaderType::Vertex;
  std::string             m_path        = "";
  std::array<uint32_t, 3> m_threadGroup = { 1, 1, 1 };

public:
  Shader() = default;

  const ShaderType&              type() const;
  const std::string&             path() const;
  const std::array<uint32_t, 3>& threadGroup() const;
  bool                           valid() const;

private:
  Shader(ShaderType, const std::string&, std::array<uint32_t, 3>);
};

}