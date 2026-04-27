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

TEST_CASE("cmd: submit empty buffer signals fence", "[cmd][submit]") {
  grf::GRF grf;
  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Fence fence = grf.createFence(false);

  ring[0].begin();
  ring[0].end();
  grf.submit(ring[0], {}, {}, fence);

  grf.waitFences({ fence });
  SUCCEED();
}

TEST_CASE("cmd: dispatch a compute pipeline", "[cmd][compute]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_minimal.gsl", GRF_TEST_DIR)
  );
  grf::ComputePipeline pipe = grf.createComputePipeline(shader);

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Compute);
  grf::Fence fence = grf.createFence(false);

  ring[0].begin();
  ring[0].bindPipeline(pipe);
  ring[0].dispatch(1, 1, 1);
  ring[0].end();
  grf.submit(ring[0], {}, {}, fence);

  grf.waitFences({ fence });
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
  grf::Fence fence = grf.createFence(false);
  grf::Semaphore acquired = grf.createSemaphore();

  grf.beginFrame();
  grf::SwapchainImage swap = grf.nextSwapchainImage(acquired);

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
  grf.submit(ring[0], std::array{ acquired }, {}, fence);

  grf.waitFences({ fence });
  SUCCEED();
}

TEST_CASE("cmd: push constants accept span and template", "[cmd][push]") {
  grf::GRF grf;
  grf::Shader shader = grf.compileShader(grf::ShaderType::Compute,
    std::format("{}/shaders/compute_buffers.gsl", GRF_TEST_DIR)
  );
  grf::ComputePipeline pipe = grf.createComputePipeline(shader);

  grf::Buffer src = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
  grf::Buffer dst = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);

  struct Push {
    uint64_t srcAddr;
    uint64_t dstAddr;
    uint32_t count;
  };

  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Compute);
  grf::Fence fence = grf.createFence(false);

  ring[0].begin();
  ring[0].bindPipeline(pipe);
  ring[0].push(Push{ src.address(), dst.address(), 16 });
  ring[0].dispatch(1, 1, 1);
  ring[0].end();
  grf.submit(ring[0], {}, {}, fence);

  grf.waitFences({ fence });
  SUCCEED();
}

TEST_CASE("cmd: queue ownership transfer between graphics and compute", "[cmd][ownership]") {
  grf::GRF grf;

  grf::Img2D img = grf.createImg2D(grf::Format::rgba8_unorm, 64, 64);

  grf::Ring<grf::CommandBuffer> graphicsRing = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Ring<grf::CommandBuffer> computeRing  = grf.createCmdRing(grf::QueueType::Compute);

  grf::Semaphore handoff = grf.createSemaphore();
  grf::Fence done = grf.createFence(false);

  graphicsRing[0].begin();
  graphicsRing[0].transition(img, grf::Layout::Undefined, grf::Layout::General);
  graphicsRing[0].release(img, grf::Layout::General, grf::Layout::General, grf::QueueType::Compute);
  graphicsRing[0].end();
  grf.submit(graphicsRing[0], {}, std::array{ handoff });

  computeRing[0].begin();
  computeRing[0].acquire(img, grf::Layout::General, grf::Layout::General, grf::QueueType::Graphics);
  computeRing[0].end();
  grf.submit(computeRing[0], std::array{ handoff }, {}, done);

  grf.waitFences({ done });
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

  grf::Ring<grf::CommandBuffer> cmds      = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Ring<grf::Semaphore>     acquired  = grf.createSemaphoreRing();
  grf::Ring<grf::Semaphore>     rendered  = grf.createSemaphoreRing();
  grf::Ring<grf::Fence>         fences    = grf.createFenceRing(true);

  for (int frame = 0; frame < 2; ++frame) {
    auto [idx, _] = grf.beginFrame();

    grf.waitFences({ fences[idx] });
    grf.resetFences({ fences[idx] });

    grf::SwapchainImage swap = grf.nextSwapchainImage(acquired[idx]);

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

    grf.submit(cmds[idx], std::array{ acquired[idx] }, std::array{ rendered[idx] }, fences[idx]);
    grf.present(swap, std::array{ rendered[idx] });

    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("cmd: timeline value advances after submit", "[cmd][timeline]") {
  grf::GRF grf;
  grf::Ring<grf::CommandBuffer> ring = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Fence f0 = grf.createFence(false);
  grf::Fence f1 = grf.createFence(false);

  ring[0].begin();
  ring[0].end();
  grf.submit(ring[0], {}, {}, f0);
  grf.waitFences({ f0 });

  ring[0].begin();
  ring[0].end();
  grf.submit(ring[0], {}, {}, f1);
  grf.waitFences({ f1 });

  SUCCEED();
}
