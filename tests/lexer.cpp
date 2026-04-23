#include "internal/gsl/lexer.hpp"
#include "internal/gsl/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

using grf::gsl::Lexer;
using grf::gsl::Token;
using grf::gsl::TokenData;

namespace {

std::vector<TokenData> lex(std::string_view s) {
  return Lexer{s}.tokenize();
}

std::vector<Token> kinds(const std::vector<TokenData>& toks, bool skipTrivia = false) {
  std::vector<Token> out;
  for (const auto& t : toks) {
    if (skipTrivia && (t.token == Token::Whitespace || t.token == Token::Newline))
      continue;
    out.push_back(t.token);
  }
  return out;
}

}

TEST_CASE("lexer: empty input produces only EndOfFile", "[gsl][lexer]") {
  auto t = lex("");
  REQUIRE(t.size() == 1);
  CHECK(t[0].token == Token::EndOfFile);
}

TEST_CASE("lexer: identifiers", "[gsl][lexer]") {
  SECTION("plain identifier") {
    auto t = lex("foo");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Identifier);
    CHECK(t[0].source == "foo");
    CHECK(t[1].token == Token::EndOfFile);
  }

  SECTION("underscore start with digits") {
    auto t = lex("_foo123");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Identifier);
    CHECK(t[0].source == "_foo123");
  }

  SECTION("GLSL keywords not in DSL stay as Identifier") {
    auto t = lex("void");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Identifier);
    CHECK(t[0].source == "void");
  }

  SECTION("near-miss DSL keywords stay as Identifier") {
    auto t = lex("buffers");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Identifier);
    CHECK(t[0].source == "buffers");
  }
}

TEST_CASE("lexer: DSL keywords classify", "[gsl][lexer]") {
  SECTION("buffer") {
    auto t = lex("buffer");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Buffer);
    CHECK(t[0].source == "buffer");
  }

  SECTION("readonly") {
    auto t = lex("readonly");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Readonly);
  }

  SECTION("writeonly") {
    auto t = lex("writeonly");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Writeonly);
  }

  SECTION("push") {
    auto t = lex("push");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Push);
  }
}

TEST_CASE("lexer: numbers", "[gsl][lexer]") {
  SECTION("decimal int") {
    auto t = lex("42");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == "42");
  }

  SECTION("float with dot") {
    auto t = lex("3.14");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == "3.14");
  }

  SECTION("leading-dot float") {
    auto t = lex(".5");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == ".5");
  }

  SECTION("trailing-dot float") {
    auto t = lex("2.");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == "2.");
  }

  SECTION("hex lowercase prefix") {
    auto t = lex("0xFF");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == "0xFF");
  }

  SECTION("hex uppercase prefix") {
    auto t = lex("0X1a2b");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Number);
    CHECK(t[0].source == "0X1a2b");
  }

  SECTION("scientific lowercase") {
    auto t = lex("1e5");
    REQUIRE(t.size() == 2);
    CHECK(t[0].source == "1e5");
  }

  SECTION("scientific with signed exponent") {
    auto t = lex("1.5E-10");
    REQUIRE(t.size() == 2);
    CHECK(t[0].source == "1.5E-10");
  }

  SECTION("float with single-char suffix") {
    auto t = lex("1.0f");
    REQUIRE(t.size() == 2);
    CHECK(t[0].source == "1.0f");
  }

  SECTION("int with single-char suffix") {
    auto t = lex("42u");
    REQUIRE(t.size() == 2);
    CHECK(t[0].source == "42u");
  }

  SECTION("int with multi-char suffix") {
    auto t = lex("42ul");
    REQUIRE(t.size() == 2);
    CHECK(t[0].source == "42ul");
  }

  SECTION("bare dot is Dot, not Number") {
    auto t = lex(".");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Dot);
  }
}

TEST_CASE("lexer: punctuation", "[gsl][lexer]") {
  auto t = lex("{}()[];,.");
  REQUIRE(t.size() == 10);
  CHECK(t[0].token == Token::LBrace);
  CHECK(t[1].token == Token::RBrace);
  CHECK(t[2].token == Token::LParen);
  CHECK(t[3].token == Token::RParen);
  CHECK(t[4].token == Token::LBrack);
  CHECK(t[5].token == Token::RBrack);
  CHECK(t[6].token == Token::Semicolon);
  CHECK(t[7].token == Token::Comma);
  CHECK(t[8].token == Token::Dot);
  CHECK(t[9].token == Token::EndOfFile);
}

TEST_CASE("lexer: whitespace", "[gsl][lexer]") {
  SECTION("single space between identifiers") {
    auto t = lex("a b");
    REQUIRE(t.size() == 4);
    CHECK(t[0].token == Token::Identifier);
    CHECK(t[1].token == Token::Whitespace);
    CHECK(t[1].source == " ");
    CHECK(t[2].token == Token::Identifier);
  }

  SECTION("run of mixed spaces and tabs coalesces") {
    auto t = lex("a \t  b");
    REQUIRE(t.size() == 4);
    CHECK(t[1].token == Token::Whitespace);
    CHECK(t[1].source == " \t  ");
  }

  SECTION("\\r is whitespace") {
    auto t = lex("a\rb");
    REQUIRE(t.size() == 4);
    CHECK(t[1].token == Token::Whitespace);
    CHECK(t[1].source == "\r");
  }
}

TEST_CASE("lexer: newlines", "[gsl][lexer]") {
  auto t = lex("a\nb");
  REQUIRE(t.size() == 4);
  CHECK(t[0].token == Token::Identifier);
  CHECK(t[1].token == Token::Newline);
  CHECK(t[1].source == "\n");
  CHECK(t[2].token == Token::Identifier);
  CHECK(t[2].source == "b");
}

TEST_CASE("lexer: line comments", "[gsl][lexer]") {
  SECTION("stops at newline, doesn't consume it") {
    auto t = lex("// hello\nfoo");
    REQUIRE(t.size() == 4);
    CHECK(t[0].token == Token::LineComment);
    CHECK(t[0].source == "// hello");
    CHECK(t[1].token == Token::Newline);
    CHECK(t[2].token == Token::Identifier);
    CHECK(t[2].source == "foo");
  }

  SECTION("at EOF without trailing newline") {
    auto t = lex("// just this");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::LineComment);
    CHECK(t[0].source == "// just this");
  }

  SECTION("'// buffer' inside a comment does not trigger the keyword") {
    auto t = lex("// buffer");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::LineComment);
  }
}

TEST_CASE("lexer: block comments", "[gsl][lexer]") {
  SECTION("single line") {
    auto t = lex("/* hello */");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::BlockComment);
    CHECK(t[0].source == "/* hello */");
  }

  SECTION("spans multiple lines") {
    auto t = lex("/*\n  multi\n  line\n*/");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::BlockComment);
  }

  SECTION("doesn't consume trailing tokens") {
    auto t = lex("/* x */ foo");
    REQUIRE(t.size() == 4);
    CHECK(t[0].token == Token::BlockComment);
    CHECK(t[2].token == Token::Identifier);
    CHECK(t[2].source == "foo");
  }
}

TEST_CASE("lexer: slash as operator when not a comment", "[gsl][lexer]") {
  auto t = lex("a/b");
  REQUIRE(t.size() == 4);
  CHECK(t[0].token == Token::Identifier);
  CHECK(t[1].token == Token::Operator);
  CHECK(t[1].source == "/");
  CHECK(t[2].token == Token::Identifier);
}

TEST_CASE("lexer: preprocessor", "[gsl][lexer]") {
  SECTION("basic directive") {
    auto t = lex("#version 460");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Preprocessor);
    CHECK(t[0].source == "#version 460");
  }

  SECTION("stops at newline") {
    auto t = lex("#version 460\nfoo");
    REQUIRE(t.size() == 4);
    CHECK(t[0].token == Token::Preprocessor);
    CHECK(t[0].source == "#version 460");
    CHECK(t[1].token == Token::Newline);
    CHECK(t[2].token == Token::Identifier);
  }

  SECTION("line continuation extends the directive") {
    auto t = lex("#define FOO \\\n  bar");
    REQUIRE(t.size() == 2);
    CHECK(t[0].token == Token::Preprocessor);
    CHECK(t[0].source == "#define FOO \\\n  bar");
  }
}

TEST_CASE("lexer: source locations track row/col/offset", "[gsl][lexer]") {
  auto t = lex("ab\ncd");
  REQUIRE(t.size() == 4);

  CHECK(t[0].token == Token::Identifier);
  CHECK(t[0].loc.row    == 1);
  CHECK(t[0].loc.col    == 1);
  CHECK(t[0].loc.offset == 0);

  CHECK(t[1].token == Token::Newline);
  CHECK(t[1].loc.row    == 1);
  CHECK(t[1].loc.col    == 3);
  CHECK(t[1].loc.offset == 2);

  CHECK(t[2].token == Token::Identifier);
  CHECK(t[2].loc.row    == 2);
  CHECK(t[2].loc.col    == 1);
  CHECK(t[2].loc.offset == 3);
}

TEST_CASE("lexer: realistic DSL snippet", "[gsl][lexer]") {
  std::string_view src =
    "readonly buffer Vertices {\n"
    "  vec3 pos[];\n"
    "} verts;\n";

  auto t = lex(src);

  std::vector<Token> expected = {
    Token::Readonly,   Token::Buffer,    Token::Identifier, Token::LBrace,
    Token::Identifier, Token::Identifier, Token::LBrack,    Token::RBrack,    Token::Semicolon,
    Token::RBrace,     Token::Identifier, Token::Semicolon,
    Token::EndOfFile,
  };

  REQUIRE(kinds(t, /*skipTrivia=*/true) == expected);
}
