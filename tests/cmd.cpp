#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("cmd: createCmdRing returns flightFrames buffers", "[cmd][ring]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);

  CHECK(&ring[0] != &ring[1]);
}

TEST_CASE("cmd: begin/end on empty cmd buffer succeeds", "[cmd]") {
  grf::GRF grf;
  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);

  ring[0].begin();
  ring[0].end();
  SUCCEED();
}

TEST_CASE("cmd: submit empty buffer returns valid sync", "[cmd][submit]") {
  grf::GRF grf;
  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);

  ring[0].begin();
  ring[0].end();
  grf::Sync done = grf.submit(ring[0]);
  CHECK(done.valid());

  grf.wait(done);
  SUCCEED();
}

TEST_CASE("cmd: dispatch a compute pipeline", "[cmd][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_minimal.gsl", GRF_TEST_DIR)
  );
  grf::ComputePipeline pipe = grf.createComputePipeline(shader);

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Compute);

  ring[0].begin();
  ring[0].bindPipeline(pipe);
  ring[0].dispatch(1, 1, 1);
  ring[0].end();
  grf::Sync done = grf.submit(ring[0]);

  grf.wait(done);
  SUCCEED();
}

TEST_CASE("cmd: render into swapchain with clear", "[cmd][graphics]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  grf::GraphicsPipeline pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb }
  });

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);

  grf.beginFrame();
  grf::SwapchainImage swap = grf.nextSwapchainImage();

  ring[0].begin();
  ring[0].transition(swap, grf::Layout::Undefined, grf::Layout::ColorAttachmentOptimal);
  ring[0].beginRendering(
    std::array{ grf::ColorAttachment{
      .img        = swap,
      .loadOp     = grf::LoadOp::Clear,
      .storeOp    = grf::StoreOp::Store,
      .clearValue = { 0.1f, 0.2f, 0.3f, 1.0f }
    } }
  );
  ring[0].bindPipeline(pipe);
  ring[0].draw(3);
  ring[0].endRendering();
  ring[0].end();
  grf::Sync done = grf.submit(ring[0], std::array{ swap.sync() });

  grf.wait(done);
  SUCCEED();
}

TEST_CASE("cmd: push constants drive a compute copy", "[cmd][push]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_buffers.gsl", GRF_TEST_DIR)
  );
  grf::ComputePipeline pipe = grf.createComputePipeline(shader);

  constexpr uint32_t kCount = 16;

  grf::Buffer src = grf.createBuffer(grf::BufferIntent::FrequentUpdate,
                                      sizeof(uint32_t) * kCount);
  grf::Buffer dst = grf.createBuffer(grf::BufferIntent::Readable,
                                      sizeof(uint32_t) * kCount);

  std::array<uint32_t, kCount> expected{};
  for (uint32_t i = 0; i < kCount; ++i) expected[i] = i * 7 + 3;
  src.write(expected);

  // Flush any queued resource uploads to the GPU.
  grf.beginFrame();
  grf.waitForResourceUpdates();

  // The assembler emits GrfPushBlock with buffer references first (in
  // declaration order), then user `push { }` fields. compute_buffers.gsl
  // declares `buffer Src;`, `buffer Dst;`, then `push { uint count; }`, so
  // the push struct is { srcAddr, dstAddr, count } — uint64s naturally
  // 8-aligned, no manual padding required.
  struct Push {
    uint64_t srcAddr;
    uint64_t dstAddr;
    uint32_t count;
  };

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Compute);

  ring[0].begin();
  ring[0].bindPipeline(pipe);
  ring[0].push(Push{ src.address(), dst.address(), kCount });
  ring[0].dispatch(1, 1, 1);
  ring[0].end();
  grf::Sync done = grf.submit(ring[0]);

  grf.wait(done);

  std::array<uint32_t, kCount> readBack{};
  dst.read(readBack);
  for (uint32_t i = 0; i < kCount; ++i)
    CHECK(readBack[i] == expected[i]);

  grf.endFrame();
}

TEST_CASE("cmd: cross-queue sync via Sync", "[cmd][cross-queue]") {
  grf::GRF grf;

  grf::Img2D img = grf.createImg2D(grf::Format::rgba8_unorm, 64, 64);

  grf::Ring<grf::CommandBuffer> graphicsRing = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Ring<grf::CommandBuffer> computeRing  = grf.createCmdRing(grf::QueueType::Compute);

  graphicsRing[0].begin();
  graphicsRing[0].transition(img, grf::Layout::Undefined, grf::Layout::General);
  graphicsRing[0].end();
  grf::Sync graphicsDone = grf.submit(graphicsRing[0]);

  computeRing[0].begin();
  computeRing[0].end();
  grf::Sync computeDone = grf.submit(computeRing[0], std::array{ graphicsDone });

  grf.wait(computeDone);
  SUCCEED();
}

TEST_CASE("cmd: full acquire-render-present round trip", "[cmd][present]") {
  grf::GRF grf;
  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
  );
  grf::GraphicsPipeline pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb }
  });

  grf::Ring<grf::CommandBuffer> cmds       = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Ring<grf::Sync>          flightRing = grf.createSyncRing();

  for (int frame = 0; frame < 2; ++frame) {
    auto [idx, _] = grf.beginFrame();

    grf.wait(flightRing[idx]);

    grf::SwapchainImage swap = grf.nextSwapchainImage();

    cmds[idx].begin();
    cmds[idx].transition(swap, grf::Layout::Undefined, grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(
      std::array{ grf::ColorAttachment{
        .img        = swap,
        .loadOp     = grf::LoadOp::Clear,
        .clearValue = { 0.2f, 0.4f, 0.6f, 1.0f }
      } }
    );
    cmds[idx].bindPipeline(pipe);
    cmds[idx].draw(3);
    cmds[idx].endRendering();
    cmds[idx].transition(swap, grf::Layout::ColorAttachmentOptimal, grf::Layout::PresentSrc);
    cmds[idx].end();

    grf::Sync done = grf.submit(cmds[idx], std::array{ swap.sync() });
    flightRing[idx] = done;
    grf.present(swap, std::array{ done });

    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("cmd: timeline value advances after submit", "[cmd][timeline]") {
  grf::GRF grf;
  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);

  ring[0].begin();
  ring[0].end();
  grf::Sync s0 = grf.submit(ring[0]);
  grf.wait(s0);

  ring[0].begin();
  ring[0].end();
  grf::Sync s1 = grf.submit(ring[0]);
  grf.wait(s1);

  SUCCEED();
}
