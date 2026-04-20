#pragma once

#include "./parser.hpp"

#include <string>

namespace grf::gsl {

class Assembler {
  const ParsedSource& m_parsed;

public:
  explicit Assembler(const ParsedSource&);

  std::string assemble() const;
};

}
