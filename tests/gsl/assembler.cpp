#include "internal/gsl/assembler.hpp"
#include "internal/gsl/lexer.hpp"
#include "internal/gsl/parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using grf::gsl::Assembler;
using grf::gsl::Lexer;
using grf::gsl::Parser;

namespace {

std::string assembleFrom(std::string_view source, std::string_view name = "test.gsl") {
  auto tokens = Lexer{source}.tokenize();
  auto parsed = Parser{tokens, source, name}.parse();
  return Assembler{parsed}.assemble();
}

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

std::size_t indexOf(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle);
}

}

TEST_CASE("assembler: header present in every output", "[gsl][assembler]") {
  auto out = assembleFrom("");
  CHECK(out.starts_with("#version 460\n"));
  CHECK(contains(out, "GL_EXT_buffer_reference"));
  CHECK(contains(out, "GL_EXT_nonuniform_qualifier"));
  CHECK(contains(out, "GL_EXT_scalar_block_layout"));
  CHECK(contains(out, "GL_EXT_shader_image_load_formatted"));
}

TEST_CASE("assembler: descriptor heap bindings emitted", "[gsl][assembler]") {
  auto out = assembleFrom("");
  CHECK(contains(out, "grf_Tex2D[]"));
  CHECK(contains(out, "grf_Tex3D[]"));
  CHECK(contains(out, "grf_Cubemap[]"));
  CHECK(contains(out, "grf_Img2D[]"));
  CHECK(contains(out, "grf_Img3D[]"));
  CHECK(contains(out, "grf_Sampler[]"));
  CHECK(contains(out, "set = 0"));
}

TEST_CASE("assembler: no buffer decls, no push block emitted", "[gsl][assembler]") {
  auto out = assembleFrom("void main() {}");
  CHECK_FALSE(contains(out, "push_constant"));
  CHECK_FALSE(contains(out, "layout(buffer_reference"));
  CHECK(contains(out, "void main()"));
}

TEST_CASE("assembler: readonly buffer rewritten as buffer_reference", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer Vertices { vec3 pos[]; } verts;\n"
    "void main() {}"
  );
  CHECK(contains(out, "layout(buffer_reference, std430) readonly buffer Vertices"));
  CHECK(contains(out, "vec3 pos[];"));
}

TEST_CASE("assembler: writeonly buffer rewritten as buffer_reference", "[gsl][assembler]") {
  auto out = assembleFrom(
    "writeonly buffer Output { vec4 colors[]; } results;\n"
    "void main() {}"
  );
  CHECK(contains(out, "layout(buffer_reference, std430) writeonly buffer Output"));
}

TEST_CASE("assembler: buffer presence adds buffer-reference field to push block", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer Vertices { vec3 pos[]; } verts;\n"
    "void main() {}"
  );
  CHECK(contains(out, "layout(push_constant, std430) uniform GrfPushBlock"));
  CHECK(contains(out, "Vertices verts;"));
}

TEST_CASE("assembler: push block uses anonymous instance for global field access", "[gsl][assembler]") {
  auto out = assembleFrom(
    "push { uint x; uint y; };\n"
    "void main() {}"
  );
  CHECK(contains(out, "uniform GrfPushBlock {"));
  CHECK(contains(out, "uint x;"));
  CHECK(contains(out, "uint y;"));
  CHECK(contains(out, "};"));
  CHECK_FALSE(contains(out, "} grf_push"));
  CHECK_FALSE(contains(out, "} GrfPushBlock"));
}

TEST_CASE("assembler: user push fields come before auto buffer-reference fields", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer V { int x; } v;\n"
    "push { uint idx; };\n"
    "void main() {}"
  );
  auto userFieldPos = indexOf(out, "uint idx;");
  auto bufFieldPos  = indexOf(out, "V v;");
  REQUIRE(userFieldPos != std::string::npos);
  REQUIRE(bufFieldPos  != std::string::npos);
  CHECK(userFieldPos < bufFieldPos);
}

TEST_CASE("assembler: buffer instance becomes a push-block field directly", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer Vertices { vec3 pos[]; } verts;\n"
    "void main() {}"
  );
  CHECK(contains(out, "Vertices verts;"));
  CHECK_FALSE(contains(out, "#define"));
  CHECK_FALSE(contains(out, "uint64_t"));
}

TEST_CASE("assembler: multiple buffers each get their own push-block field", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer A { int x; } a;\n"
    "writeonly buffer B { int y; } b;\n"
    "void main() {}"
  );
  CHECK(contains(out, "A a;"));
  CHECK(contains(out, "B b;"));
}

TEST_CASE("assembler: user body appears after framework prelude", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer V { int x; } v;\n"
    "void main() { int y = 0; }"
  );
  auto pushBlockPos = indexOf(out, "uniform GrfPushBlock");
  auto bodyStart    = indexOf(out, "void main()");
  REQUIRE(pushBlockPos != std::string::npos);
  REQUIRE(bodyStart    != std::string::npos);
  CHECK(pushBlockPos < bodyStart);
}

TEST_CASE("assembler: parser's #line directives preserved in body", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer V {\n"
    "  int x;\n"
    "} v;\n"
    "void main() {}",
    "shaders/mesh.gsl"
  );
  CHECK(contains(out, "#line 1 \"shaders/mesh.gsl\""));
  CHECK(contains(out, "#line 3 \"shaders/mesh.gsl\""));
}

TEST_CASE("assembler: single 'in' gets location 0", "[gsl][assembler]") {
  auto out = assembleFrom("in vec3 position;\nvoid main() {}");
  CHECK(contains(out, "layout(location = 0) in vec3 position;"));
}

TEST_CASE("assembler: in and out counters are independent", "[gsl][assembler]") {
  auto out = assembleFrom(
    "in vec3 position;\n"
    "out vec4 color;\n"
    "void main() {}"
  );
  CHECK(contains(out, "layout(location = 0) in vec3 position;"));
  CHECK(contains(out, "layout(location = 0) out vec4 color;"));
}

TEST_CASE("assembler: multiple ins get increasing locations", "[gsl][assembler]") {
  auto out = assembleFrom(
    "in vec3 position;\n"
    "in vec3 normal;\n"
    "in vec2 uv;\n"
    "void main() {}"
  );
  CHECK(contains(out, "layout(location = 0) in vec3 position;"));
  CHECK(contains(out, "layout(location = 1) in vec3 normal;"));
  CHECK(contains(out, "layout(location = 2) in vec2 uv;"));
}

TEST_CASE("assembler: interpolation qualifier emitted with layout", "[gsl][assembler]") {
  auto out = assembleFrom("flat in int materialId;\nvoid main() {}");
  CHECK(contains(out, "layout(location = 0) flat in int materialId;"));
}

TEST_CASE("assembler: full integration - buffer + push + body", "[gsl][assembler]") {
  auto out = assembleFrom(
    "readonly buffer Vertices { vec3 pos[]; } verts;\n"
    "push { uint idx; };\n"
    "void main() { gl_Position = vec4(verts.pos[idx], 1.0); }"
  );

  CHECK(contains(out, "#version 460"));
  CHECK(contains(out, "grf_Tex2D[]"));
  CHECK(contains(out, "layout(buffer_reference, std430) readonly buffer Vertices"));
  CHECK(contains(out, "uniform GrfPushBlock"));
  CHECK(contains(out, "uint idx;"));
  CHECK(contains(out, "Vertices verts;"));
  CHECK(contains(out, "gl_Position = vec4(verts.pos[idx], 1.0);"));
}
