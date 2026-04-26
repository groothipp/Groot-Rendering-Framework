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
  SUCCEED();
}

TEST_CASE("pipelines: createComputePipeline accepts shader with buffer and push blocks",
         "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_buffers.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline pipeline = grf.createComputePipeline(shader);
  SUCCEED();
}

TEST_CASE("pipelines: createComputePipeline accepts shader with 2D workgroup",
         "[pipelines][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_workgroup.gsl", GRF_TEST_DIR)
  );
  REQUIRE(shader.valid());

  grf::ComputePipeline pipeline = grf.createComputePipeline(shader);
  SUCCEED();
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
  SUCCEED();
}

TEST_CASE("pipelines: createGraphicsPipeline produces valid pipeline", "[pipelines][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  REQUIRE(vs.valid());
  REQUIRE(fs.valid());

  grf::GraphicsPipeline pipeline = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb }
  });
  SUCCEED();
}

TEST_CASE("pipelines: createGraphicsPipeline accepts BDA-pulled vertex shader",
         "[pipelines][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_pulled_vertex.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_pulled_vertex.frag.gsl", GRF_TEST_DIR)
  );
  REQUIRE(vs.valid());
  REQUIRE(fs.valid());

  grf::GraphicsPipeline pipeline = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb }
  });
  SUCCEED();
}

TEST_CASE("pipelines: createGraphicsPipeline with depth attachment + test enabled",
         "[pipelines][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  REQUIRE(vs.valid());
  REQUIRE(fs.valid());

  grf::GraphicsPipeline pipeline = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats   = { grf::Format::bgra8_srgb },
    .depthFormat    = grf::Format::d32_sfloat,
    .depthTest      = true,
    .depthWrite     = true,
    .depthCompareOp = grf::CompareOp::Less
  });
  SUCCEED();
}

TEST_CASE("pipelines: createGraphicsPipeline with alpha blending", "[pipelines][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  REQUIRE(vs.valid());
  REQUIRE(fs.valid());

  grf::GraphicsPipeline pipeline = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .blends = { grf::BlendState{
      .enable         = true,
      .srcColorFactor = grf::BlendFactor::SrcAlpha,
      .dstColorFactor = grf::BlendFactor::OneMinusSrcAlpha
    } }
  });
  SUCCEED();
}

TEST_CASE("pipelines: createGraphicsPipeline can be called multiple times",
         "[pipelines][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  REQUIRE(vs.valid());
  REQUIRE(fs.valid());

  grf::GraphicsPipelineSettings settings{ .colorFormats = { grf::Format::bgra8_srgb } };
  grf::GraphicsPipeline a = grf.createGraphicsPipeline(vs, fs, settings);
  grf::GraphicsPipeline b = grf.createGraphicsPipeline(vs, fs, settings);
  SUCCEED();
}

TEST_CASE("resources: createDepthImage produces valid depth image", "[resources][depth]") {
  grf::GRF grf;

  SECTION("attachment-only") {
    grf::DepthImage depth = grf.createDepthImage(grf::Format::d32_sfloat, 256, 256);
    CHECK(depth.dims() == std::pair<uint32_t, uint32_t>{ 256, 256 });
    CHECK(depth.format() == grf::Format::d32_sfloat);
    CHECK(depth.heapIndex() == 0xFFFFFFFF);
  }

  SECTION("sampled depth gets a heap slot") {
    grf::DepthImage depth = grf.createDepthImage(grf::Format::d32_sfloat, 256, 256, true);
    CHECK(depth.heapIndex() != 0xFFFFFFFF);
  }
}
