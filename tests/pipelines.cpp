#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("pipelines: createComputePipeline produces valid pipeline", "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_minimal.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline pipeline = grf.createComputePipeline(shader);
  CHECK(pipeline.valid());
}

TEST_CASE("pipelines: createComputePipeline accepts shader with buffer and push blocks",
         "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_buffers.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline pipeline = grf.createComputePipeline(shader);
  CHECK(pipeline.valid());
}

TEST_CASE("pipelines: createComputePipeline accepts shader with 2D workgroup",
         "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_workgroup.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline pipeline = grf.createComputePipeline(shader);
  CHECK(pipeline.valid());
}

TEST_CASE("pipelines: createComputePipeline can be called multiple times with same shader",
         "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_minimal.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline a = grf.createComputePipeline(shader);
  grf::ComputePipeline b = grf.createComputePipeline(shader);
  CHECK(a.valid());
  CHECK(b.valid());
}
