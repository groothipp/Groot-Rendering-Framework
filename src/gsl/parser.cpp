#include "internal/gsl/parser.hpp"
#include "internal/log.hpp"

#include <charconv>
#include <format>
#include <system_error>

namespace grf::gsl {

namespace {

bool isTrivia(Token k) {
  return k == Token::Whitespace
      || k == Token::Newline
      || k == Token::LineComment
      || k == Token::BlockComment;
}

}

Parser::Parser(std::span<const TokenData> tokens, std::string_view source, std::string_view sourceName)
  : m_tokens(tokens), m_source(source), m_sourceName(sourceName) {}

bool Parser::atEnd() const {
  return m_cursor >= m_tokens.size() || m_tokens[m_cursor].token == Token::EndOfFile;
}

const TokenData& Parser::peek(std::size_t offset) const {
  std::size_t idx = m_cursor + offset;
  if (idx >= m_tokens.size()) return m_tokens.back();
  return m_tokens[idx];
}

const TokenData& Parser::advance() {
  if (atEnd()) return m_tokens.back();
  return m_tokens[m_cursor++];
}

const TokenData* Parser::peekNextNonTrivia() const {
  std::size_t i = m_cursor + 1;
  while (i < m_tokens.size()) {
    const auto& t = m_tokens[i];
    if (isTrivia(t.token)) { i++; continue; }
    if (t.token == Token::EndOfFile) return nullptr;
    return &t;
  }
  return nullptr;
}

void Parser::skipTrivia() {
  while (!atEnd() && isTrivia(peek().token)) advance();
}

const TokenData& Parser::expect(Token kind, std::string_view what) {
  const TokenData& t = peek();
  if (t.token != kind) {
    GRF_PANIC("{}:{}:{}: expected {}, got '{}'",
              m_sourceName, t.loc.row, t.loc.col, what, t.source);
  }
  return advance();
}

void Parser::emitLineDirective(std::string& body, uint32_t row) const {
  body += std::format("#line {} \"{}\"\n", row, m_sourceName);
}

ParsedSource Parser::parse() {
  ParsedSource result;
  m_cursor     = 0;
  m_parenDepth = 0;

  emitLineDirective(result.body, 1);

  while (!atEnd()) {
    const TokenData& tok = peek();

    if (tok.token == Token::Readonly || tok.token == Token::Writeonly) {
      const TokenData* nxt = peekNextNonTrivia();
      if (nxt && nxt->token == Token::Buffer) {
        BufferDecl bd = parseBufferDecl();

        for (const auto& existing : result.buffers) {
          if (existing.instanceName == bd.instanceName) {
            GRF_PANIC("{}:{}:{}: duplicate buffer instance name '{}'",
                      m_sourceName, bd.loc.row, bd.loc.col, bd.instanceName);
          }
        }
        result.buffers.push_back(bd);

        if (!atEnd()) emitLineDirective(result.body, peek().loc.row);
        continue;
      }
      result.body += tok.source;
      advance();
    }
    else if (tok.token == Token::Buffer) {
      GRF_PANIC("{}:{}:{}: 'buffer' must be preceded by 'readonly' or 'writeonly'",
                m_sourceName, tok.loc.row, tok.loc.col);
    }
    else if (tok.token == Token::Push) {
      const TokenData* nxt = peekNextNonTrivia();
      if (nxt && nxt->token == Token::LBrace) {
        if (result.push.has_value()) {
          GRF_PANIC("{}:{}:{}: duplicate 'push' block",
                    m_sourceName, tok.loc.row, tok.loc.col);
        }
        result.push = parsePushBlock();

        if (!atEnd()) emitLineDirective(result.body, peek().loc.row);
        continue;
      }
      GRF_PANIC("{}:{}:{}: 'push' is reserved; expected '{{' after",
                m_sourceName, tok.loc.row, tok.loc.col);
    }
    else if (tok.token == Token::ThreadGroup) {
      if (result.threadGroup.has_value()) {
        GRF_PANIC("{}:{}:{}: duplicate 'thread_group' declaration",
                  m_sourceName, tok.loc.row, tok.loc.col);
      }
      result.threadGroup = parseThreadGroup();

      if (!atEnd()) emitLineDirective(result.body, peek().loc.row);
      continue;
    }
    else if (m_parenDepth == 0 && (tok.token == Token::In || tok.token == Token::Out)) {
      StageVar sv = parseStageVar({}, tok.loc);
      if (sv.direction == StageDirection::In)  result.ins.push_back(sv);
      else                                     result.outs.push_back(sv);

      if (!atEnd()) emitLineDirective(result.body, peek().loc.row);
      continue;
    }
    else if (m_parenDepth == 0 && (tok.token == Token::Flat
                                || tok.token == Token::Smooth
                                || tok.token == Token::Noperspective)) {
      const TokenData* nxt = peekNextNonTrivia();
      if (nxt && (nxt->token == Token::In || nxt->token == Token::Out)) {
        std::string_view interp = tok.source;
        SourceLoc        declLoc = tok.loc;
        advance();           // consume interpolation qualifier
        skipTrivia();
        StageVar sv = parseStageVar(interp, declLoc);
        if (sv.direction == StageDirection::In)  result.ins.push_back(sv);
        else                                     result.outs.push_back(sv);

        if (!atEnd()) emitLineDirective(result.body, peek().loc.row);
        continue;
      }
      result.body += tok.source;
      advance();
    }
    else {
      if      (tok.token == Token::LParen) m_parenDepth++;
      else if (tok.token == Token::RParen) m_parenDepth--;
      result.body += tok.source;
      advance();
    }
  }

  return result;
}

BufferDecl Parser::parseBufferDecl() {
  const TokenData& qualTok  = advance();
  std::string_view qualifier = qualTok.source;
  SourceLoc        declLoc   = qualTok.loc;

  skipTrivia();
  expect(Token::Buffer, "'buffer' after qualifier");

  skipTrivia();
  const TokenData& typeTok = expect(Token::Identifier, "buffer type name");
  std::string_view typeName = typeTok.source;

  skipTrivia();
  const TokenData& lbrace = expect(Token::LBrace, "'{' to open buffer body");

  std::size_t depth     = 1;
  std::size_t bodyStart = lbrace.loc.offset + 1;
  std::size_t bodyEnd   = 0;
  bool        closed    = false;
  while (!atEnd()) {
    const TokenData& t = peek();
    if (t.token == Token::LBrace) {
      depth++;
      advance();
    } else if (t.token == Token::RBrace) {
      depth--;
      if (depth == 0) {
        bodyEnd = t.loc.offset;
        advance();
        closed = true;
        break;
      }
      advance();
    } else {
      advance();
    }
  }
  if (!closed) {
    GRF_PANIC("{}:{}:{}: unterminated buffer body (no matching '}}')",
              m_sourceName, lbrace.loc.row, lbrace.loc.col);
  }
  std::string_view body = m_source.substr(bodyStart, bodyEnd - bodyStart);

  skipTrivia();
  const TokenData& instTok = expect(Token::Identifier, "buffer instance name");
  std::string_view instanceName = instTok.source;

  skipTrivia();
  expect(Token::Semicolon, "';' after buffer instance");

  return BufferDecl{
    .qualifier    = qualifier,
    .typeName     = typeName,
    .body         = body,
    .instanceName = instanceName,
    .loc          = declLoc,
  };
}

ThreadGroup Parser::parseThreadGroup() {
  const TokenData& kwTok = advance();
  SourceLoc        loc   = kwTok.loc;

  skipTrivia();
  expect(Token::LBrack, "'[' after 'thread_group'");

  std::array<uint32_t, 3> dims{};
  for (std::size_t i = 0; i < 3; ++i) {
    skipTrivia();
    const TokenData& numTok = expect(Token::Number, "thread_group dimension");

    auto [_, ec] = std::from_chars(
      numTok.source.data(),
      numTok.source.data() + numTok.source.size(),
      dims[i]
    );
    if (ec != std::errc{}) {
      GRF_PANIC("{}:{}:{}: invalid thread_group dimension '{}'",
                m_sourceName, numTok.loc.row, numTok.loc.col, numTok.source);
    }

    if (i < 2) {
      skipTrivia();
      expect(Token::Comma, "',' between thread_group dimensions");
    }
  }

  skipTrivia();
  expect(Token::RBrack, "']' to close thread_group");

  skipTrivia();
  expect(Token::Semicolon, "';' after thread_group");

  return ThreadGroup{ .dims = dims, .loc = loc };
}

PushBlock Parser::parsePushBlock() {
  const TokenData& pushTok = advance();
  SourceLoc        loc     = pushTok.loc;

  skipTrivia();
  const TokenData& lbrace = expect(Token::LBrace, "'{' to open push block");

  std::size_t depth     = 1;
  std::size_t bodyStart = lbrace.loc.offset + 1;
  std::size_t bodyEnd   = 0;
  bool        closed    = false;
  while (!atEnd()) {
    const TokenData& t = peek();
    if (t.token == Token::LBrace) {
      depth++;
      advance();
    } else if (t.token == Token::RBrace) {
      depth--;
      if (depth == 0) {
        bodyEnd = t.loc.offset;
        advance();
        closed = true;
        break;
      }
      advance();
    } else {
      advance();
    }
  }
  if (!closed) {
    GRF_PANIC("{}:{}:{}: unterminated push block (no matching '}}')",
              m_sourceName, lbrace.loc.row, lbrace.loc.col);
  }
  std::string_view body = m_source.substr(bodyStart, bodyEnd - bodyStart);

  skipTrivia();
  expect(Token::Semicolon, "';' after push block");

  return PushBlock{
    .body = body,
    .loc  = loc,
  };
}

StageVar Parser::parseStageVar(std::string_view interpolation, SourceLoc declLoc) {
  const TokenData& dirTok = peek();
  StageDirection direction = dirTok.token == Token::In ? StageDirection::In : StageDirection::Out;
  advance();

  skipTrivia();
  const TokenData& typeTok = expect(Token::Identifier, "stage variable type");
  std::string_view type = typeTok.source;

  skipTrivia();
  const TokenData& nameTok = expect(Token::Identifier, "stage variable name");
  std::string_view name = nameTok.source;

  skipTrivia();
  expect(Token::Semicolon, "';' after stage variable");

  return StageVar{
    .direction     = direction,
    .interpolation = interpolation,
    .type          = type,
    .name          = name,
    .loc           = declLoc,
  };
}

}
