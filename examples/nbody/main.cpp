#include "nbody.hpp"

#include "external/imgui/imgui.h"

const u32 g_windowWidth = 1280;
const u32 g_windowHeight = 720;
const u32 g_flightFrames = 2;
const f32 g_spawnTimerTimeout = 0.3;

int main() {
  GRF grf(Settings{
    .windowTitle  = "GRF Example: N-Body Simulation",
    .windowSize   = { g_windowWidth, g_windowHeight },
    .flightFrames = g_flightFrames
  });

  Particles particles(grf, "particles", g_flightFrames);

  Ring<CommandBuffer> graphCmdRing = grf.createCmdRing(QueueType::Graphics);
  Ring<Fence> flightFenceRing = grf.createFenceRing(true);
  Ring<Semaphore> imgSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> graphSemRing = grf.createSemaphoreRing();

  u32 particleCount = 0;
  u32 prevFrameIndex = g_flightFrames - 1;
  Input& input = grf.input();
  bool aabbToggle = false;
  f32 spawnTimer = g_spawnTimerTimeout;

  while (grf.running([&]() { return input.isJustPressed(Key::Escape); })) {
    auto [frameIndex, dt] = grf.beginFrame();

    spawnTimer = std::min(spawnTimer + dt, g_spawnTimerTimeout);

    auto& graphCmd = graphCmdRing[frameIndex];

    auto& flightFence = flightFenceRing[frameIndex];
    auto& imgSem = imgSemRing[frameIndex];
    auto& graphSem = graphSemRing[frameIndex];

    grf.waitFences({ flightFence });
    grf.resetFences({ flightFence });

    SwapchainImage swapchainImage = grf.nextSwapchainImage({ imgSem });
    ColorAttachment swapchainAttachment{
      .img      = swapchainImage,
      .loadOp   = LoadOp::Clear,
      .storeOp  = StoreOp::Store
    };

    graphCmd.begin();
    graphCmd.transition(swapchainImage, Layout::Undefined, Layout::ColorAttachmentOptimal);
    graphCmd.beginRendering({ swapchainAttachment });

    if (particleCount > 0) {
      graphCmd.beginProfile("particles");
      particles.render(graphCmd, frameIndex, Particles::Data{
        .screenDims     = { g_windowWidth, g_windowHeight },
        .particleCount  = particleCount
      });
      graphCmd.endProfile();
    }

    graphCmd.beginProfile("gui");
    grf.gui().beginFrame();
    grf.profiler().render();

    ImGui::Begin("Particles");
      ImGui::Text("Count");
      ImGui::SameLine(0.0, 40.0);
      ImGui::Text("%d", particleCount);
      ImGui::Checkbox("AABB Visuals", &aabbToggle);
      bool shouldReset = ImGui::Button("reset", ImVec2(100.0, 25.0));
    ImGui::End();

    grf.gui().render(graphCmd);
    graphCmd.endProfile();

    graphCmd.endRendering();
    graphCmd.transition(swapchainImage, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
    graphCmd.end();

    grf.waitForResourceUpdates();

    grf.submit(graphCmd, { imgSem }, { graphSem }, flightFence);
    grf.present(swapchainImage, { graphSem });

    if (particleCount > 0 && shouldReset) {
      particles.reset(g_flightFrames);
      particleCount = 0;
    }
    else if (
      !grf.gui().wantsMouse()             &&
      input.isPressed(MouseButton::Left)  &&
      spawnTimer == g_spawnTimerTimeout
    ) {
      auto [x, y] = input.cursorPos();
      vec2 center = vec2(2.0 * x / g_windowWidth - 1.0, 2.0 * y / g_windowHeight - 1.0);

      particleCount += particles.spawn(center, particleCount, g_flightFrames);

      spawnTimer = 0.0;
    }

    prevFrameIndex = frameIndex;
    grf.endFrame();
  }
}