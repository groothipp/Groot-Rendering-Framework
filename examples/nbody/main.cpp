#include "nbody.hpp"

#include "external/imgui/imgui.h"

const u32     g_windowWidth = 1280;
const u32     g_windowHeight = 720;
const u32     g_flightFrames = 2;
const f32     g_spawnTimerTimeout = 0.01;
constexpr f32 g_ar = static_cast<f32>(g_windowWidth) / static_cast<f32>(g_windowHeight);
const f32     g_G = 2e-4;
const f32     g_theta = 0.5;

int main() {
  GRF grf(Settings{
    .windowTitle  = "GRF Example: N-Body Simulation",
    .windowSize   = { g_windowWidth, g_windowHeight },
    .flightFrames = g_flightFrames
  });

  Particles particles(grf, "particles", g_flightFrames);
  AABBDebug aabbDebug(grf, "aabb_debug");
  Brush brush(grf, "brush");
  LBVHTree lbvhTree(grf, "tree");
  Physics physics(grf, "physics");

  Ring<CommandBuffer> graphCmdRing = grf.createCmdRing(QueueType::Graphics);
  Ring<CommandBuffer> compCmdRing = grf.createCmdRing(QueueType::Compute);
  Ring<Fence> flightFenceRing = grf.createFenceRing(true);
  Ring<Semaphore> imgSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> graphSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> compSemRing = grf.createSemaphoreRing();

  u32 particleCount = 0;
  u32 prevFrameIndex = g_flightFrames - 1;
  Input& input = grf.input();
  bool aabbToggle = false;
  f32 spawnTimer = g_spawnTimerTimeout;
  i32 spawnRadius = (g_minSpawnRadius + g_maxSpawnRadius) / 2;
  bool brushToggle = false;
  f32 gamma = 0.5;
  f32 K = 75.0;
  f32 kickC = 1.0;
  f32 r0 = 2.0 * g_particleRadius;
  f32 maxV = 2.0;
  f32 maxA = 60.0;

  while (grf.running([&]() { return input.isJustPressed(Key::Escape); })) {
    auto [frameIndex, dt] = grf.beginFrame();

    if (!grf.gui().wantsKeyboard() && input.isJustPressed(Key::S))
      brushToggle = !brushToggle;

    spawnTimer = std::min(spawnTimer + dt, g_spawnTimerTimeout);

    auto& graphCmd = graphCmdRing[frameIndex];
    auto& compCmd = compCmdRing[frameIndex];

    auto& flightFence = flightFenceRing[frameIndex];
    auto& imgSem = imgSemRing[frameIndex];
    auto& graphSem = graphSemRing[frameIndex];
    auto& compSem = compSemRing[frameIndex];

    auto [prevPosBuf, prevVelBuf] = particles.buffers(prevFrameIndex);

    grf.waitFences({ flightFence });
    grf.resetFences({ flightFence });

    SwapchainImage swapchainImage = grf.nextSwapchainImage({ imgSem });
    ColorAttachment swapchainAttachment{
      .img      = swapchainImage,
      .loadOp   = LoadOp::Clear,
      .storeOp  = StoreOp::Store
    };

    compCmd.begin();
    if (particleCount > 0) {
      Buffer& comBuf = physics.comBuffer(frameIndex);

      compCmd.beginProfile("tree construction");
      lbvhTree.construct(compCmd, LBVHTree::Data{
        .frameIndex     = frameIndex,
        .particleCount  = particleCount,
        .posBufAddr     = prevPosBuf.address(),
        .comBufAddr     = comBuf.address()
      });
      compCmd.endProfile();

      Buffer& aabbBuf = lbvhTree.aabbBuffer(frameIndex);

      compCmd.barrier(aabbBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
      compCmd.barrier(comBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

      auto [posBuf, velBuf] = particles.buffers(frameIndex);
      auto [indexBuf, childBuf] = lbvhTree.treeBuffers(frameIndex);

      physics.dispatch(compCmd, frameIndex, Physics::Data{
        .indexBufAddr   = indexBuf.address(),
        .childBufAddr   = childBuf.address(),
        .aabbBufAddr    = aabbBuf.address(),
        .prevPosBufAddr = prevPosBuf.address(),
        .prevVelBufAddr = prevVelBuf.address(),
        .posBufAddr     = posBuf.address(),
        .velBufAddr     = velBuf.address(),
        .particleCount  = particleCount,
        .dt             = dt,
        .G              = g_G,
        .theta          = g_theta,
        .softening      = r0,
        .gamma          = gamma * std::sqrt(2.0f * K),
        .K              = K,
        .kickC          = kickC,
        .r0             = r0,
        .maxV           = maxV,
        .maxA           = maxA
      });
    }
    compCmd.end();

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

      if (aabbToggle) {
        graphCmd.beginProfile("aabb debug");
        aabbDebug.render(graphCmd, AABBDebug::Data{
          .aabbBufAddr    = lbvhTree.aabbBuffer(frameIndex).address(),
          .screenDims     = uvec2(g_windowWidth, g_windowHeight),
          .particleCount  = particleCount
        });
        graphCmd.endProfile();
      }
    }

    if (brushToggle && particleCount < g_maxParticleCount) {
      auto [x, y] = input.cursorPos();
      vec2 cursor = vec2(g_ar * (2.0 * x / g_windowWidth - 1.0), 2.0 * y / g_windowHeight - 1.0);

      graphCmd.beginProfile("brush");
      brush.render(graphCmd, Brush::Data{
        .screenDims = uvec2(g_windowWidth, g_windowHeight),
        .pos        = cursor,
        .radius     = static_cast<f32>(spawnRadius) / 1000.0f
      });
      graphCmd.endProfile();
    }

    grf.gui().beginFrame();
    grf.profiler().render();

    ImGui::Begin("Particles");
      ImGui::Text("Count");
      ImGui::SameLine(0.0, 40.0);
      ImGui::Text("%d", particleCount);
      ImGui::Checkbox("AABB Visuals", &aabbToggle);
      ImGui::SliderFloat("Gamma", &gamma, 0.1, 1.0);
      ImGui::SliderFloat("K", &K, 1.0, 150.0);
      ImGui::SliderFloat("KickC", &kickC, 0.0, 5.0);
      ImGui::SliderFloat("r0", &r0, 0.5 * g_particleRadius, 5.0 * g_particleRadius);
      ImGui::SliderFloat("MaxV", &maxV, 0.05, 5.0);
      ImGui::SliderFloat("MaxA", &maxA, 20.0, 200.0);
      bool shouldReset = ImGui::Button("reset", ImVec2(100.0, 25.0));
      if (brushToggle)
        ImGui::SliderInt("radius", &spawnRadius, g_minSpawnRadius, g_maxSpawnRadius);
    ImGui::End();

    graphCmd.beginProfile("gui");
    grf.gui().render(graphCmd);
    graphCmd.endProfile();

    graphCmd.endRendering();
    graphCmd.transition(swapchainImage, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
    graphCmd.end();

    grf.waitForResourceUpdates();

    grf.submit(compCmd, {}, { compSem });
    grf.submit(graphCmd, { imgSem, compSem }, { graphSem }, flightFence);
    grf.present(swapchainImage, { graphSem });

    if (particleCount > 0 && shouldReset) {
      particles.reset(g_flightFrames);
      particleCount = 0;
    }
    else if (
      !grf.gui().wantsMouse()             &&
      particleCount < g_maxParticleCount  &&
      brushToggle                         &&
      input.isPressed(MouseButton::Left)  &&
      spawnTimer == g_spawnTimerTimeout
    ) {
      auto [x, y] = input.cursorPos();
      vec2 cursor = vec2(g_ar * (2.0 * x / g_windowWidth - 1.0), 2.0 * y / g_windowHeight - 1.0);

      particleCount += particles.spawn(
        cursor, particleCount, g_flightFrames, static_cast<f32>(spawnRadius) / 1000.f
      );

      spawnTimer = 0.0;
    }

    prevFrameIndex = frameIndex;
    grf.endFrame();
  }
}