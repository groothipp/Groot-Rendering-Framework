#pragma once

#include "./tokens.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace grf::gsl {

class Lexer {
  std::string_view  m_source;
  std::size_t       m_cursor = 0;
  uint32_t          m_row = 1;
  uint32_t          m_col = 1;

public:
  explicit Lexer(std::string_view);

  std::vector<TokenData> tokenize();

private:
  TokenData nextToken();

  TokenData lexIdentifierOrKeyword();
  TokenData lexNumber();
  TokenData lexLineComment();
  TokenData lexBlockComment();
  TokenData lexWhitespace();
  TokenData lexPreprocessor();
  TokenData lexOperator();

  char peek(std::size_t offset = 0) const;
  char advance();
  bool atEnd() const;
  TokenData makeToken(Token, size_t startOffset, SourceLoc startLoc) const;
};

}