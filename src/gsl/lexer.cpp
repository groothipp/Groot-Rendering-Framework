#include "internal/gsl/lexer.hpp"
#include "internal/log.hpp"

namespace grf::gsl {

namespace {

bool isDigit(char c)         { return c >= '0' && c <= '9'; }
bool isHexDigit(char c)      { return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool isAlpha(char c)         { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isIdentStart(char c)    { return isAlpha(c) || c == '_'; }
bool isIdentContinue(char c) { return isAlpha(c) || isDigit(c) || c == '_'; }

}

Lexer::Lexer(std::string_view s) : m_source(s) {}

std::vector<TokenData> Lexer::tokenize() {
  std::vector<TokenData> tokens;
  while (true) {
    TokenData tok = nextToken();
    tokens.emplace_back(tok);
    if (tok.token == Token::EndOfFile) break;
  }
  return tokens;
}

char Lexer::peek(std::size_t offset) const {
  std::size_t idx = m_cursor + offset;
  return idx < m_source.size() ? m_source[idx] : '\0';
}

char Lexer::advance() {
  if (atEnd()) return '\0';
  char c = m_source[m_cursor++];
  if (c == '\n') { m_row++; m_col = 1; }
  else           { m_col++; }
  return c;
}

bool Lexer::atEnd() const {
  return m_cursor >= m_source.size();
}

TokenData Lexer::makeToken(Token kind, std::size_t startOffset, SourceLoc startLoc) const {
  return TokenData{
    .token  = kind,
    .source = m_source.substr(startOffset, m_cursor - startOffset),
    .loc    = startLoc
  };
}

TokenData Lexer::nextToken() {
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };
  std::size_t startOffset = m_cursor;

  if (atEnd()) return makeToken(Token::EndOfFile, startOffset, startLoc);

  char c = peek();

  if (c == '\n') {
    advance();
    return makeToken(Token::Newline, startOffset, startLoc);
  }

  if (c == ' ' || c == '\t' || c == '\r') return lexWhitespace();

  if (c == '#') return lexPreprocessor();

  if (c == '/' && peek(1) == '/') return lexLineComment();
  if (c == '/' && peek(1) == '*') return lexBlockComment();

  switch (c) {
    case '{': advance(); return makeToken(Token::LBrace,    startOffset, startLoc);
    case '}': advance(); return makeToken(Token::RBrace,    startOffset, startLoc);
    case '(': advance(); return makeToken(Token::LParen,    startOffset, startLoc);
    case ')': advance(); return makeToken(Token::RParen,    startOffset, startLoc);
    case '[': advance(); return makeToken(Token::LBrack,    startOffset, startLoc);
    case ']': advance(); return makeToken(Token::RBrack,    startOffset, startLoc);
    case ';': advance(); return makeToken(Token::Semicolon, startOffset, startLoc);
    case ',': advance(); return makeToken(Token::Comma,     startOffset, startLoc);
  }

  if (isDigit(c))                   return lexNumber();
  if (c == '.' && isDigit(peek(1))) return lexNumber();

  if (c == '.') {
    advance();
    return makeToken(Token::Dot, startOffset, startLoc);
  }

  if (isIdentStart(c)) return lexIdentifierOrKeyword();

  return lexOperator();
}

TokenData Lexer::lexWhitespace() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r'))
    advance();

  return makeToken(Token::Whitespace, startOffset, startLoc);
}

TokenData Lexer::lexLineComment() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  while (!atEnd() && peek() != '\n') advance();

  return makeToken(Token::LineComment, startOffset, startLoc);
}

TokenData Lexer::lexBlockComment() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  advance(); advance();

  while (!atEnd()) {
    if (peek() == '*' && peek(1) == '/') {
      advance(); advance();
      return makeToken(Token::BlockComment, startOffset, startLoc);
    }
    advance();
  }

  GRF_PANIC("Unterminated block comment starting at {}:{}", startLoc.row, startLoc.col);
}

TokenData Lexer::lexPreprocessor() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  while (!atEnd() && peek() != '\n') {
    if (peek() == '\\' && peek(1) == '\n') {
      advance();
      advance();
      continue;
    }
    advance();
  }

  return makeToken(Token::Preprocessor, startOffset, startLoc);
}

TokenData Lexer::lexIdentifierOrKeyword() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  while (!atEnd() && isIdentContinue(peek())) advance();

  std::string_view text = m_source.substr(startOffset, m_cursor - startOffset);

  Token kind = Token::Identifier;
  if      (text == "buffer")        kind = Token::Buffer;
  else if (text == "readonly")      kind = Token::Readonly;
  else if (text == "writeonly")     kind = Token::Writeonly;
  else if (text == "push")          kind = Token::Push;
  else if (text == "thread_group")  kind = Token::ThreadGroup;
  else if (text == "in")            kind = Token::In;
  else if (text == "out")           kind = Token::Out;
  else if (text == "flat")          kind = Token::Flat;
  else if (text == "smooth")        kind = Token::Smooth;
  else if (text == "noperspective") kind = Token::Noperspective;

  return makeToken(kind, startOffset, startLoc);
}

TokenData Lexer::lexNumber() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
    advance(); advance();
    while (!atEnd() && isHexDigit(peek())) advance();
  } else {
    while (!atEnd() && isDigit(peek())) advance();

    if (peek() == '.') {
      advance();
      while (!atEnd() && isDigit(peek())) advance();
    }

    if (peek() == 'e' || peek() == 'E') {
      advance();
      if (peek() == '+' || peek() == '-') advance();
      while (!atEnd() && isDigit(peek())) advance();
    }
  }

  while (!atEnd() && isAlpha(peek())) advance();

  return makeToken(Token::Number, startOffset, startLoc);
}

TokenData Lexer::lexOperator() {
  std::size_t startOffset = m_cursor;
  SourceLoc   startLoc    = { m_row, m_col, static_cast<uint32_t>(m_cursor) };

  advance();

  return makeToken(Token::Operator, startOffset, startLoc);
}

}
