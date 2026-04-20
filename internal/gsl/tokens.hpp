#pragma once

#include <cstdint>
#include <string_view>

namespace grf::gsl {

enum class Token : int8_t {
  LBrace, RBrace,
  LParen, RParen,
  LBrack, RBrack,
  Semicolon,
  Comma,
  Dot,

  Identifier,
  Number,
  Operator,

  Buffer,
  Readonly,
  Writeonly,
  Push,
  In,
  Out,
  Flat,
  Smooth,
  Noperspective,

  Whitespace,
  Newline,
  LineComment,
  BlockComment,

  Preprocessor,

  EndOfFile
};

struct SourceLoc {
  uint32_t row;
  uint32_t col;
  uint32_t offset;
};

struct TokenData {
  Token token;
  std::string_view source;
  SourceLoc loc;
};

}