#include "internal/gsl/lexer.hpp"
#include "internal/gsl/parser.hpp"
#include "internal/gsl/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using grf::gsl::Lexer;
using grf::gsl::Parser;
using grf::gsl::ParsedSource;

namespace {

ParsedSource parse(std::string_view source, std::string_view name = "test.gsl") {
  auto tokens = Lexer{source}.tokenize();
  return Parser{tokens, source, name}.parse();
}

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

}

TEST_CASE("parser: empty input emits only leading #line", "[gsl][parser]") {
  auto r = parse("");
  CHECK(r.buffers.empty());
  CHECK_FALSE(r.push.has_value());
  CHECK(r.body == "#line 1 \"test.gsl\"\n");
}

TEST_CASE("parser: pass-through source without triggers", "[gsl][parser]") {
  auto r = parse("void main() { gl_Position = vec4(0.0); }");
  CHECK(r.buffers.empty());
  CHECK_FALSE(r.push.has_value());
  CHECK(r.body == "#line 1 \"test.gsl\"\nvoid main() { gl_Position = vec4(0.0); }");
}

TEST_CASE("parser: readonly buffer decl extracted", "[gsl][parser]") {
  auto r = parse("readonly buffer Vertices { vec3 pos[]; } verts;");
  REQUIRE(r.buffers.size() == 1);

  const auto& b = r.buffers[0];
  CHECK(b.qualifier    == "readonly");
  CHECK(b.typeName     == "Vertices");
  CHECK(b.body         == " vec3 pos[]; ");
  CHECK(b.instanceName == "verts");
  CHECK(b.loc.row      == 1);
  CHECK(b.loc.col      == 1);
}

TEST_CASE("parser: writeonly buffer decl extracted", "[gsl][parser]") {
  auto r = parse("writeonly buffer Output { vec4 colors[]; } results;");
  REQUIRE(r.buffers.size() == 1);
  CHECK(r.buffers[0].qualifier    == "writeonly");
  CHECK(r.buffers[0].typeName     == "Output");
  CHECK(r.buffers[0].instanceName == "results");
}

TEST_CASE("parser: push block extracted", "[gsl][parser]") {
  auto r = parse("push { uint frameIndex; float time; };");
  REQUIRE(r.push.has_value());
  CHECK(r.push->body    == " uint frameIndex; float time; ");
  CHECK(r.push->loc.row == 1);
}

TEST_CASE("parser: multiple buffer decls preserved in order", "[gsl][parser]") {
  auto r = parse(
    "readonly buffer A { int x; } a;\n"
    "writeonly buffer B { int y; } b;\n"
    "void main() {}"
  );
  REQUIRE(r.buffers.size() == 2);
  CHECK(r.buffers[0].instanceName == "a");
  CHECK(r.buffers[0].qualifier    == "readonly");
  CHECK(r.buffers[1].instanceName == "b");
  CHECK(r.buffers[1].qualifier    == "writeonly");
  CHECK(contains(r.body, "void main()"));
}

TEST_CASE("parser: buffer body preserves inner whitespace and comments", "[gsl][parser]") {
  auto r = parse(
    "readonly buffer Vertices {\n"
    "  // coord data\n"
    "  vec3 pos[];\n"
    "} verts;"
  );
  REQUIRE(r.buffers.size() == 1);
  CHECK(r.buffers[0].body == "\n  // coord data\n  vec3 pos[];\n");
}

TEST_CASE("parser: nested braces in buffer body balance correctly", "[gsl][parser]") {
  auto r = parse("readonly buffer X { struct S { int y; } s; } x;");
  REQUIRE(r.buffers.size() == 1);
  CHECK(r.buffers[0].body         == " struct S { int y; } s; ");
  CHECK(r.buffers[0].instanceName == "x");
}

TEST_CASE("parser: readonly as a non-buffer GLSL qualifier passes through", "[gsl][parser]") {
  auto r = parse("layout(rgba8) readonly image2D img;");
  CHECK(r.buffers.empty());
  CHECK(contains(r.body, "readonly"));
  CHECK(contains(r.body, "image2D"));
}

TEST_CASE("parser: writeonly as non-buffer qualifier passes through", "[gsl][parser]") {
  auto r = parse("layout(rgba8) writeonly image2D img;");
  CHECK(r.buffers.empty());
  CHECK(contains(r.body, "writeonly"));
}

TEST_CASE("parser: leading #line directive always emitted", "[gsl][parser]") {
  auto r = parse("int x;");
  CHECK(r.body.starts_with("#line 1 \"test.gsl\"\n"));
}

TEST_CASE("parser: #line directive emitted after a removed trigger", "[gsl][parser]") {
  auto r = parse(
    "readonly buffer V { int x; } v;\n"
    "void main() {}"
  );
  CHECK(contains(r.body, "#line 1 \"test.gsl\""));
  CHECK(contains(r.body, "void main()"));
}

TEST_CASE("parser: multi-line trigger removal updates #line correctly", "[gsl][parser]") {
  auto r = parse(
    "readonly buffer V {\n"
    "  int x;\n"
    "} v;\n"
    "void main() {}"
  );
  REQUIRE(r.buffers.size() == 1);
  CHECK(contains(r.body, "#line 3 \"test.gsl\""));
  CHECK(contains(r.body, "void main()"));
}

TEST_CASE("parser: trigger at EOF without trailing content", "[gsl][parser]") {
  auto r = parse("readonly buffer V { int x; } v;");
  REQUIRE(r.buffers.size() == 1);
  CHECK(r.body == "#line 1 \"test.gsl\"\n");
}

TEST_CASE("parser: buffer + push + body together", "[gsl][parser]") {
  auto r = parse(
    "readonly buffer Vertices { vec3 pos[]; } verts;\n"
    "push { uint idx; };\n"
    "void main() { gl_Position = vec4(verts.pos[idx], 1.0); }"
  );

  REQUIRE(r.buffers.size() == 1);
  REQUIRE(r.push.has_value());
  CHECK(r.buffers[0].instanceName == "verts");
  CHECK(r.push->body              == " uint idx; ");
  CHECK(contains(r.body, "void main()"));
  CHECK(contains(r.body, "gl_Position"));
}

TEST_CASE("parser: source name appears in #line directives", "[gsl][parser]") {
  auto r = parse("int x;", "shaders/mesh.vert.gsl");
  CHECK(contains(r.body, "#line 1 \"shaders/mesh.vert.gsl\""));
}

TEST_CASE("parser: single 'in' stage var extracted", "[gsl][parser]") {
  auto r = parse("in vec3 position;");
  REQUIRE(r.ins.size() == 1);
  CHECK(r.outs.empty());
  CHECK(r.ins[0].direction     == grf::gsl::StageDirection::In);
  CHECK(r.ins[0].type          == "vec3");
  CHECK(r.ins[0].name          == "position");
  CHECK(r.ins[0].interpolation == "");
}

TEST_CASE("parser: single 'out' stage var extracted", "[gsl][parser]") {
  auto r = parse("out vec4 color;");
  REQUIRE(r.outs.size() == 1);
  CHECK(r.ins.empty());
  CHECK(r.outs[0].direction == grf::gsl::StageDirection::Out);
  CHECK(r.outs[0].type      == "vec4");
  CHECK(r.outs[0].name      == "color");
}

TEST_CASE("parser: interpolation qualifier captured with stage var", "[gsl][parser]") {
  auto r = parse("flat in int materialId;");
  REQUIRE(r.ins.size() == 1);
  CHECK(r.ins[0].interpolation == "flat");
  CHECK(r.ins[0].type          == "int");
  CHECK(r.ins[0].name          == "materialId");
}

TEST_CASE("parser: in/out inside function parameters passes through", "[gsl][parser]") {
  auto r = parse("vec3 blend(in vec3 a, out vec3 b) { b = a; return a; }");
  CHECK(r.ins.empty());
  CHECK(r.outs.empty());
  CHECK(contains(r.body, "vec3 blend(in vec3 a, out vec3 b)"));
}

TEST_CASE("parser: multiple in/out stage vars preserve declaration order", "[gsl][parser]") {
  auto r = parse(
    "in vec3 position;\n"
    "in vec3 normal;\n"
    "out vec4 color;\n"
  );
  REQUIRE(r.ins.size()  == 2);
  REQUIRE(r.outs.size() == 1);
  CHECK(r.ins[0].name  == "position");
  CHECK(r.ins[1].name  == "normal");
  CHECK(r.outs[0].name == "color");
}

TEST_CASE("parser: thread_group extracted", "[gsl][parser]") {
  auto r = parse("thread_group [8, 4, 2];");
  REQUIRE(r.threadGroup.has_value());
  CHECK(r.threadGroup->dims[0] == 8u);
  CHECK(r.threadGroup->dims[1] == 4u);
  CHECK(r.threadGroup->dims[2] == 2u);
  CHECK(r.threadGroup->loc.row == 1u);
  CHECK(r.threadGroup->loc.col == 1u);
}

TEST_CASE("parser: thread_group not present when omitted", "[gsl][parser]") {
  auto r = parse("void main() {}");
  CHECK_FALSE(r.threadGroup.has_value());
}

TEST_CASE("parser: thread_group followed by body emits #line", "[gsl][parser]") {
  auto r = parse(
    "thread_group [16, 16, 1];\n"
    "void main() {}"
  );
  REQUIRE(r.threadGroup.has_value());
  CHECK(contains(r.body, "#line 1 \"test.gsl\""));
  CHECK(contains(r.body, "void main()"));
}
