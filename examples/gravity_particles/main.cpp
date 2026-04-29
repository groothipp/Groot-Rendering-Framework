#include "public/grf.hpp"

#include "external/imgui/imgui.h"

#include <format>
#include <numbers>
#include <random>

using namespace grf;

struct Constants {
  f32 G;
  f32 m;
  f32 r;
};

struct PhysicsPassData {
  u64 constBufAddr;
  u64 prevPosBufAddr;
  u64 posBufAddr;
  u64 prevVelBufAddr;
  u64 velBufAddr;
  u64 energyBufAddr;
  u32 particleCount;
  f32 dt;
};

struct GraphicsPassData {
  u64   constBufAddr;
  u64   posBufAddr;
  u64   velBufAddr;
  u64   energyBufAddr;
  uvec2 screenDims;
  u32   particleCount;
};

std::vector<vec2> spawnParticles(vec2, u32);

static const u32 g_flightFrames = 2;
static const u32 g_windowWidth = 1280;
static const u32 g_windowHeight = 720;
static const u32 g_maxParticles = 1500;
static const Constants g_constants = Constants{ .G = 0.00667, .m = 0.01, .r = 0.008 };

int main() {
  GRF grf(Settings{
    .windowTitle      = "Gravity Particles",
    .windowSize       = { g_windowWidth, g_windowHeight },
    .flightFrames     = g_flightFrames
  });

  // recommended to always compile shaders first thing so that any shader compilation errors panic before gpu resources are allocated
  Shader physicsShader = grf.compileShader(ShaderType::Compute, std::format("{}/physics.gsl", SHADERS));
  Shader vertShader = grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", SHADERS));
  Shader fragShader = grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", SHADERS));

  ComputePipeline physicsPass = grf.createComputePipeline(physicsShader);
  GraphicsPipeline graphicsPass = grf.createGraphicsPipeline(vertShader, fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { BlendState{ .srcColorFactor = BlendFactor::SrcAlpha, .dstColorFactor = BlendFactor::OneMinusSrcAlpha } }
  });

  Ring<CommandBuffer> compCmdRing = grf.createCmdRing(QueueType::Compute);
  Ring<CommandBuffer> graphCmdRing = grf.createCmdRing(QueueType::Graphics);

  Ring<Fence> flightFenceRing = grf.createFenceRing(true);
  Ring<Semaphore> imgSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> compSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> graphSemRing = grf.createSemaphoreRing();

  // Frequent update == "I plan to update this per frame via buffer.write()"
  // Single update == "I plan to initialize this with data and never touch it again"
  Ring<Buffer> posBufRing = grf.createBufferRing(BufferIntent::FrequentUpdate, sizeof(vec2) * g_maxParticles);
  Ring<Buffer> velBufRing = grf.createBufferRing(BufferIntent::SingleUpdate, sizeof(vec2) * g_maxParticles);
  Ring<Buffer> energyBufRing = grf.createBufferRing(BufferIntent::SingleUpdate, sizeof(f32) * g_maxParticles);
  Buffer constBuf = grf.createBuffer(BufferIntent::SingleUpdate, sizeof(Constants));

  { // init data inside of a new scope so that the vectors are destroyed
    std::vector<vec2> initData(g_maxParticles, vec2(0.0, 0.0));
    for (i32 i = 0; i < g_flightFrames; ++i) {
      posBufRing[i].write(initData);
      velBufRing[i].write(initData);
    }
  }

  {
    std::vector<f32> initData(g_maxParticles, 0.0);
    for (i32 i = 0; i < g_flightFrames; ++i)
      energyBufRing[i].write(initData);
  }

  constBuf.write(g_constants);

  Input& input = grf.input();
  u32 prevIndex = g_flightFrames - 1, particleCount = 0;
  while (grf.running([&](){ return !grf.gui().wantsKeyboard() && input.isJustPressed(Key::Escape); })) {
    auto [frameIndex, dt] = grf.beginFrame();

    auto& compCmd     = compCmdRing[frameIndex];
    auto& graphCmd    = graphCmdRing[frameIndex];

    auto& flightFence = flightFenceRing[frameIndex];
    auto& imgSem      = imgSemRing[frameIndex];
    auto& compSem     = compSemRing[frameIndex];
    auto& graphSem    = graphSemRing[frameIndex];

    auto& prevPosBuf  = posBufRing[prevIndex];
    auto& posBuf      = posBufRing[frameIndex];
    auto& prevVelBuf  = velBufRing[prevIndex];
    auto& velBuf      = velBufRing[frameIndex];
    auto& energyBuf   = energyBufRing[frameIndex];

    grf.waitFences({ flightFence });
    grf.resetFences({ flightFence });

    SwapchainImage renderTarget = grf.nextSwapchainImage(imgSem);

    compCmd.begin();
    compCmd.beginProfile("physics");

    compCmd.bindPipeline(physicsPass);
    compCmd.push(PhysicsPassData{
      .constBufAddr   = constBuf.address(),
      .prevPosBufAddr = prevPosBuf.address(),
      .posBufAddr     = posBuf.address(),
      .prevVelBufAddr = prevVelBuf.address(),
      .velBufAddr     = velBuf.address(),
      .energyBufAddr  = energyBuf.address(),
      .particleCount  = particleCount,
      .dt             = dt
    });
    if (particleCount > 0) {
      auto [x, y, z] = physicsShader.threadGroup();
      compCmd.dispatch((particleCount + x - 1) / x);
    }

    compCmd.endProfile();
    compCmd.end();

    graphCmd.begin();

    graphCmd.beginProfile("transition color attachment");
    graphCmd.transition(renderTarget, Layout::Undefined, Layout::ColorAttachmentOptimal);
    graphCmd.endProfile();

    graphCmd.beginRendering({ ColorAttachment{ .img = renderTarget, .loadOp = LoadOp::Clear } });

    graphCmd.beginProfile("graphics");

    graphCmd.bindPipeline(graphicsPass);
    graphCmd.push(GraphicsPassData{
      .constBufAddr   = constBuf.address(),
      .posBufAddr     = posBuf.address(),
      .velBufAddr     = velBuf.address(),
      .energyBufAddr  = energyBuf.address(),
      .screenDims     = uvec2(g_windowWidth, g_windowHeight),
      .particleCount  = particleCount
    });
    if (particleCount > 0)
      graphCmd.draw(6 * particleCount);

    graphCmd.endProfile();

    grf.gui().beginFrame();

    grf.profiler().render();

    ImGui::Begin("Particles");
      ImGui::Text("Count");
      ImGui::SameLine(0.0, 40.0);
      ImGui::Text("%d", particleCount);

      bool shouldReset = ImGui::Button("reset", ImVec2(100.0, 25.0));
    ImGui::End();

    graphCmd.beginProfile("gui");
    grf.gui().render(graphCmd);
    graphCmd.endProfile();

    graphCmd.endRendering();

    graphCmd.beginProfile("transition present");
    graphCmd.transition(renderTarget, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
    graphCmd.endProfile();

    graphCmd.end();

    grf.waitForResourceUpdates();

    grf.submit(compCmd, {}, { compSem });
    grf.submit(graphCmd, { imgSem, compSem }, { graphSem }, flightFence);
    grf.present(renderTarget, { graphSem });

    if (!shouldReset && particleCount != g_maxParticles && !grf.gui().wantsMouse() && input.isJustPressed(MouseButton::Left)) {
      auto [x, y] = input.cursorPos();
      f32 ar = static_cast<f32>(g_windowWidth) / static_cast<f32>(g_windowHeight);
      vec2 center = vec2(ar * (2.0 * x / g_windowWidth - 1.0), 2.0 * y / g_windowHeight - 1.0);

      std::vector<vec2> particles = spawnParticles(center, particleCount);
      posBuf.write(particles, sizeof(vec2) * particleCount);

      particleCount += particles.size();
    }

    if (shouldReset) {
      {
        std::vector<vec2> data(g_maxParticles, vec2(0.0));
        for (i32 i = 0; i < g_flightFrames; ++i) {
          posBufRing[i].write(data);
          velBufRing[i].write(data);
        }
      }
      {
        std::vector<f32> data(g_maxParticles, 0.0);
        for (i32 i = 0; i < g_flightFrames; ++i)
          energyBufRing[i].write(data);
      }
      particleCount = 0;
    }

    prevIndex = frameIndex;
    grf.endFrame();
  }
}

std::vector<vec2> spawnParticles(vec2 center, u32 particleCount) {
  static std::mt19937_64 gen(std::random_device{}());
  static const f32 density = 500.0;

  f32 radius = std::uniform_real_distribution<f32>(0.05, 0.15)(gen);
  f32 area = std::numbers::pi_v<f32> * radius * radius;
  u32 count = std::floor(density * area);
  count = std::min(count, g_maxParticles - particleCount);

  std::vector<vec2> particles;
  for (i32 i = 0; i < count; ++i) {
    float theta = std::uniform_real_distribution<f32>(0.0, 2.0 * std::numbers::pi_v<f32>)(gen);
    float r = radius * std::sqrt(std::uniform_real_distribution<f32>(0.0, 1.0)(gen));

    particles.emplace_back(center + vec2(r * cos(theta), r * sin(theta)));
  }

  return particles;
}