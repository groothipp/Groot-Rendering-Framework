#pragma once

#include "./parser.hpp"

#include "public/types.hpp"

#include <string>

namespace grf::gsl {

class Assembler {
  const ParsedSource& m_parsed;
  ShaderType          m_type;

public:
  Assembler(const ParsedSource&, ShaderType);

  std::string assemble() const;
};

}
