#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("shaders: compile", "[shaders]" ) {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/vertex.gsl", GRF_TEST_DIR)
  );

  CHECK( shader.valid() );
  CHECK( shader.path() == std::format("{}/shaders/vertex.gsl", GRF_TEST_DIR));
  CHECK( shader.type() == grf::ShaderType::Vertex );
}