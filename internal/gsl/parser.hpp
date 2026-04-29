#pragma once

#include "./tokens.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace grf::gsl {

struct BufferDecl {
  std::string_view              qualifier;
  std::string                   typeName;       // auto-generated
  std::string_view              body;
  std::string                   instanceName;   // user-given (named form) or generated (anonymous form)
  std::vector<std::string_view> fieldNames;     // populated only for anonymous form
  bool                          anonymous;      // true → emit #define for each field
  SourceLoc                     loc;
};

struct PushBlock {
  std::string_view body;
  SourceLoc        loc;
};

struct ThreadGroup {
  std::array<uint32_t, 3> dims;
  SourceLoc               loc;
};

enum class StageDirection { In, Out };

struct StageVar {
  StageDirection   direction;
  std::string_view interpolation;  // "flat" | "smooth" | "noperspective" | empty
  std::string_view type;
  std::string_view name;
  SourceLoc        loc;
};

struct ParsedSource {
  std::vector<BufferDecl>       buffers;
  std::optional<PushBlock>      push;
  std::optional<ThreadGroup>    threadGroup;
  std::vector<StageVar>         ins;
  std::vector<StageVar>         outs;
  std::vector<std::string_view> extensions;
  std::string                   body;
};

class Parser {
  std::span<const TokenData> m_tokens;
  std::string_view           m_source;
  std::string_view           m_sourceName;
  std::size_t                m_cursor       = 0;
  int                        m_parenDepth   = 0;
  std::size_t                m_bufferCounter = 0;

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

  BufferDecl  parseBufferDecl();
  PushBlock   parsePushBlock();
  ThreadGroup parseThreadGroup();
  StageVar    parseStageVar(std::string_view interpolation, SourceLoc declLoc);

  void emitLineDirective(std::string& body, uint32_t row) const;
};

}
