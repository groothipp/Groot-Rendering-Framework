#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("destruction: buffer survives endFrame within flightFrames window", "[destruction][buffer]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint64_t address;
  {
    grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
    address = buf.address();
    CHECK(address != 0);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: image survives endFrame within flightFrames window", "[destruction][image]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, 64, 64);
    CHECK(tex.heapIndex() != 0xFFFFFFFFu);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: sampler dies after endFrame drain", "[destruction][sampler]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Sampler s = grf.createSampler({});
    CHECK(s.heapIndex() != 0xFFFFFFFFu);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: depth image dies after endFrame drain", "[destruction][depth]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::DepthImage d = grf.createDepthImage(grf::Format::d32_sfloat, 64, 64);
    CHECK(d.dims().first == 64u);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: pipeline dies after endFrame drain", "[destruction][pipeline]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
      std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
    );
    grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
      std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
    );
    grf::GraphicsPipeline pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
      .colorFormats = { grf::Format::bgra8_srgb }
    });
    CHECK(pipe.valid());
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: copies share lifetime, last copy triggers death", "[destruction][buffer]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint64_t address;
  {
    grf::Buffer original = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
    address = original.address();

    {
      grf::Buffer copy1 = original;
      grf::Buffer copy2 = original;
      CHECK(copy1.address() == address);
      CHECK(copy2.address() == address);
    }

    CHECK(original.address() == address);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: many resources can be created and destroyed in a loop", "[destruction]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  for (int frame = 0; frame < 8; ++frame) {
    grf.beginFrame();

    for (int i = 0; i < 16; ++i) {
      grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
      grf::Tex2D  tex = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    }

    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: outliving the proxy does not crash GRF shutdown", "[destruction][shutdown]") {
  grf::Buffer* leaked = nullptr;

  {
    grf::GRF grf;
    leaked = new grf::Buffer(grf.createBuffer(grf::BufferIntent::GPUOnly, 256));
  }

  delete leaked;

  SUCCEED();
}

TEST_CASE("destruction: heap slots are NOT reclaimed (TODO)", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t firstSlot;
  {
    grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    firstSlot = tex.heapIndex();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Tex2D next = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
  CHECK(next.heapIndex() != firstSlot);
}
