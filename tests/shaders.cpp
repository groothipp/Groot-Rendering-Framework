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
  CHECK( shader.threadGroup() == std::array<uint32_t, 3>{ 1, 1, 1 } );
}

TEST_CASE("shaders: compute thread_group is exposed on shader", "[shaders]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_workgroup.gsl", GRF_TEST_DIR)
  );

  CHECK( shader.valid() );
  CHECK( shader.type() == grf::ShaderType::Compute );
  CHECK( shader.threadGroup() == std::array<uint32_t, 3>{ 16, 16, 1 } );
}