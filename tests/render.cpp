#include "public/grf.hpp"

#include "imgui.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cinttypes>
#include <format>

TEST_CASE("render: full triangle render loop with present", "[render]") {
  grf::GRF grf(grf::Settings{
    .windowTitle = "GRF triangle test",
    .windowSize  = { 1280u, 720u },
    .flightFrames = 2
  });

  grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
    std::format("{}/shaders/triangle.vert.gsl", GRF_TEST_DIR)
  );
  grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
    std::format("{}/shaders/triangle.frag.gsl", GRF_TEST_DIR)
  );

  grf::GraphicsPipeline pipeline = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .topology     = grf::Topology::TriangleList,
    .cullMode     = grf::CullMode::None
  });

  grf::Ring<grf::CommandBuffer> cmds     = grf.createCmdRing(grf::QueueType::Graphics);
  grf::Ring<grf::Semaphore>     acquired = grf.createSemaphoreRing();
  grf::Ring<grf::Semaphore>     rendered = grf.createSemaphoreRing();
  grf::Ring<grf::Fence>         fences   = grf.createFenceRing(true);

  const uint64_t frameLimit = 1200;
  uint64_t       framesRun  = 0;

  while (grf.running([&]{ return framesRun >= frameLimit; })) {
    auto [idx, dt] = grf.beginFrame();
    grf.gui().beginFrame();

    ImGui::Begin("GRF render test");
    ImGui::Text("frame: %" PRIu64 " / %" PRIu64, framesRun, frameLimit);
    ImGui::Text("dt: %.3f ms", dt * 1000.0);
    ImGui::Text("gui wants mouse: %s", grf.gui().wantsMouse() ? "yes" : "no");
    ImGui::End();

    grf.profiler().render();

    grf.waitFences({ fences[idx] });
    grf.resetFences({ fences[idx] });

    grf::SwapchainImage swap = grf.nextSwapchainImage(acquired[idx]);

    cmds[idx].begin();
    cmds[idx].beginProfile("frame");

    cmds[idx].beginProfile("acquire transition");
    cmds[idx].transition(swap, grf::Layout::Undefined, grf::Layout::ColorAttachmentOptimal);
    cmds[idx].endProfile();

    cmds[idx].beginRendering(
      std::array{ grf::ColorAttachment{
        .img        = swap,
        .loadOp     = grf::LoadOp::Clear,
        .storeOp    = grf::StoreOp::Store,
        .clearValue = { 0.05f, 0.05f, 0.08f, 1.0f }
      } }
    );
    cmds[idx].beginProfile("triangle");
    cmds[idx].bindPipeline(pipeline);
    cmds[idx].draw(3);
    cmds[idx].endProfile();
    cmds[idx].beginProfile("imgui");
    grf.gui().render(cmds[idx]);
    cmds[idx].endProfile();
    cmds[idx].endRendering();

    cmds[idx].beginProfile("present transition");
    cmds[idx].transition(swap, grf::Layout::ColorAttachmentOptimal, grf::Layout::PresentSrc);
    cmds[idx].endProfile();

    cmds[idx].endProfile();
    cmds[idx].end();

    grf.submit(
      cmds[idx],
      std::array{ acquired[idx] },
      std::array{ rendered[idx] },
      fences[idx]
    );
    grf.present(swap, std::array{ rendered[idx] });

    grf.endFrame();
    ++framesRun;
  }

  CHECK(framesRun == frameLimit);
}
