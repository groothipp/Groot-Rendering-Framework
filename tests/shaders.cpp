#include "public/enums.hpp"
#include "public/gpu.hpp"
#include "public/shader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("shaders: compile", "[shaders]" ) {
  grf::GPU gpu;
  grf::Shader shader = gpu.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/vertex.gsl", GRF_TEST_DIR)
  );

  CHECK( shader.valid() );
  CHECK( shader.path() == std::format("{}/shaders/vertex.gsl", GRF_TEST_DIR));
  CHECK( shader.type() == grf::ShaderType::Vertex );
}