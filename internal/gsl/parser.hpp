#pragma once

#include "./tokens.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace grf::gsl {

struct BufferDecl {
  std::string_view qualifier;
  std::string_view typeName;
  std::string_view body;
  std::string_view instanceName;
  SourceLoc        loc;
};

struct PushBlock {
  std::string_view body;
  SourceLoc        loc;
};

struct ParsedSource {
  std::vector<BufferDecl>  buffers;
  std::optional<PushBlock> push;
  std::string              body;
};

class Parser {
  std::span<const TokenData> m_tokens;
  std::string_view           m_source;
  std::string_view           m_sourceName;
  std::size_t                m_cursor = 0;

public:
  Parser(std::span<const TokenData> tokens, std::string_view source, std::string_view sourceName = "<shader>");

  ParsedSource parse();

private:
  bool              atEnd() const;
  const TokenData&  peek(std::size_t offset = 0) const;
  const TokenData&  advance();
  const TokenData*  peekNextNonTrivia() const;
  void              skipTrivia();
  const TokenData&  expect(Token kind, std::string_view what);

  BufferDecl parseBufferDecl();
  PushBlock  parsePushBlock();

  void emitLineDirective(std::string& body, uint32_t row) const;
};

}
