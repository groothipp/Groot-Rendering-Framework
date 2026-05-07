#include "public/grf.hpp"

#include "external/imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <format>
#include <numbers>
#include <random>
#include <string>
#include <vector>

using namespace grf;

struct InternalNode {
  u32 left;
  u32 right;
  u32 parent;
  u32 _pad;
};

struct Bounds1Data {
  u64 posBufAddr;
  u64 scratchBufAddr;
  u32 N;
};

struct Bounds2Data {
  u64 scratchBufAddr;
  u64 boundsBufAddr;
  u32 tiles;
};

struct EncodeData {
  u64 posBufAddr;
  u64 boundsBufAddr;
  u64 mortonBufAddr;
  u64 indexBufAddr;
  u32 N;
};

struct Sort1Data {
  u64 keyBufAddr;
  u64 histBufAddr;
  u32 N;
  u32 tiles;
  u32 shift;
};

struct Sort2Data {
  u64 histBufAddr;
  u64 offsetBufAddr;
  u32 tiles;
};

struct Sort3Data {
  u64 inKeysBufAddr;
  u64 outKeysBufAddr;
  u64 inValsBufAddr;
  u64 outValsBufAddr;
  u64 offsetBufAddr;
  u32 N;
  u32 tiles;
  u32 shift;
};

struct BuildData {
  u64 mortonBufAddr;
  u64 nodeBufAddr;
  u64 nodeParentBufAddr;
  u64 leafBufAddr;
  u32 N;
};

struct ClearData {
  u64 aggBufAddr;
  u64 nodeParentBufAddr;
  u32 N;
};

struct AggregateData {
  u64 aggBufAddr;
  u64 scratchComBufAddr;
  u64 scratchAabbBufAddr;
  u64 comBufAddr;
  u64 aabbBufAddr;
  u64 indexBufAddr;
  u64 posBufAddr;
  u64 leafBufAddr;
  u64 nodeParentBufAddr;
  u64 nodeBufAddr;
  u32 N;
  f32 M;
};

struct DensityData {
  u64 indexBufAddr;
  u64 nodeBufAddr;
  u64 aabbBufAddr;
  u64 posBufAddr;
  u64 densityBufAddr;
  u32 rootIndex;
  u32 N;
  f32 M;
  f32 H;
};

struct ForceData {
  u64 indexBufAddr;
  u64 nodeBufAddr;
  u64 comBufAddr;
  u64 aabbBufAddr;
  u64 densityBufAddr;
  u64 prevPosBufAddr;
  u64 posBufAddr;
  u64 prevVelBufAddr;
  u64 velBufAddr;
  u64 energyBufAddr;
  u64 constBufAddr;
  u32 rootIndex;
  f32 theta;
  u32 N;
  f32 dt;
};

struct GraphicsData {
  u64   posBufAddr;
  u64   velBufAddr;
  u64   energyBufAddr;
  uvec2 screenDims;
  u32   N;
  f32   R;
  f32   M;
  f32   K;
};

struct AABBData {
  u64   nodeAABBBufAddr;
  uvec2 screenDims;
  u32   N;
};

struct Constants {
  float G;
  float M;
  float H;
  float cs;
  float smoothing;
};

static const u32 g_flightFrames    = 2;
static const u32 g_windowWidth     = 1280;
static const u32 g_windowHeight    = 720;
static const u32 g_maxParticles    = 1000;
static const u32 g_keysPerTile     = 1024;
static const u32 g_buckets         = 16;
static const u32 g_maxSortTiles    = (g_maxParticles + g_keysPerTile - 1) / g_keysPerTile;
static const u32 g_maxBoundsTiles  = (g_maxParticles + 255) / 256;
static const f32 g_particleMass    = 0.001;
static const f32 g_particleRadius  = 0.01;
static const f32 g_particleSmoothing = 2.0 * g_particleRadius;
static const f32 g_specificHeat    = 0.1;
static const f32 g_theta           = 0.5;
static const f32 g_gravConst       = 1.0;
static const f32 g_boltzmann       = 1.0;

std::vector<vec2> spawnParticles(vec2 center, f32 radius, u32 currentCount);

int main() {
  GRF grf(Settings{
    .windowTitle  = "N-Body Simulation",
    .windowSize   = { g_windowWidth, g_windowHeight },
    .flightFrames = g_flightFrames
  });

  Shader boundsShader1 = grf.compileShader(
    ShaderType::Compute, std::format("{}/bounds_1.gsl", SHADERS)
  );

  Shader boundsShader2 = grf.compileShader(
    ShaderType::Compute, std::format("{}/bounds_2.gsl", SHADERS)
  );

  Shader encodeShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/encode.gsl", SHADERS)
  );

  Shader sortShader1 = grf.compileShader(
    ShaderType::Compute, std::format("{}/sort_1.gsl", SHADERS)
  );

  Shader sortShader2 = grf.compileShader(
    ShaderType::Compute, std::format("{}/sort_2.gsl", SHADERS)
  );

  Shader sortShader3 = grf.compileShader(
    ShaderType::Compute, std::format("{}/sort_3.gsl", SHADERS)
  );

  Shader buildShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/build.gsl", SHADERS)
  );

  Shader clearShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/clear.gsl", SHADERS)
  );

  Shader aggregateShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/aggregate.gsl", SHADERS)
  );

  Shader densityShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/density.gsl", SHADERS)
  );

  Shader forceShader = grf.compileShader(
    ShaderType::Compute, std::format("{}/force.gsl", SHADERS)
  );

  Shader vertShader = grf.compileShader(
    ShaderType::Vertex, std::format("{}/vert.gsl", SHADERS)
  );

  Shader fragShader = grf.compileShader(
    ShaderType::Fragment, std::format("{}/frag.gsl", SHADERS)
  );

  Shader aabbVertShader = grf.compileShader(
    ShaderType::Vertex, std::format("{}/aabb_vert.gsl", SHADERS)
  );

  Shader aabbFragShader = grf.compileShader(
    ShaderType::Fragment, std::format("{}/aabb_frag.gsl", SHADERS)
  );

  ComputePipeline boundsPass1   = grf.createComputePipeline(boundsShader1);
  ComputePipeline boundsPass2   = grf.createComputePipeline(boundsShader2);
  ComputePipeline encodePass    = grf.createComputePipeline(encodeShader);
  ComputePipeline sortPass1     = grf.createComputePipeline(sortShader1);
  ComputePipeline sortPass2     = grf.createComputePipeline(sortShader2);
  ComputePipeline sortPass3     = grf.createComputePipeline(sortShader3);
  ComputePipeline buildPass     = grf.createComputePipeline(buildShader);
  ComputePipeline clearPass     = grf.createComputePipeline(clearShader);
  ComputePipeline aggregatePass = grf.createComputePipeline(aggregateShader);
  ComputePipeline densityPass   = grf.createComputePipeline(densityShader);
  ComputePipeline forcePass     = grf.createComputePipeline(forceShader);

  GraphicsPipeline graphicsPass = grf.createGraphicsPipeline(vertShader, fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { BlendState{ .srcColorFactor = BlendFactor::SrcAlpha, .dstColorFactor = BlendFactor::OneMinusSrcAlpha } }
  });

  GraphicsPipeline aabbPass = grf.createGraphicsPipeline(aabbVertShader, aabbFragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .topology     = Topology::LineList,
    .cullMode     = CullMode::None,
    .blends       = { BlendState{ .srcColorFactor = BlendFactor::SrcAlpha, .dstColorFactor = BlendFactor::OneMinusSrcAlpha } }
  });

  Ring<CommandBuffer> compCmdRing  = grf.createCmdRing(QueueType::Compute);
  Ring<CommandBuffer> graphCmdRing = grf.createCmdRing(QueueType::Graphics);

  Ring<Fence>     flightFenceRing = grf.createFenceRing(true);
  Ring<Semaphore> imgSemRing      = grf.createSemaphoreRing();
  Ring<Semaphore> compSemRing     = grf.createSemaphoreRing();
  Ring<Semaphore> graphSemRing    = grf.createSemaphoreRing();

  Ring<Buffer> posBufRing            = grf.createBufferRing(BufferIntent::FrequentUpdate, g_maxParticles * sizeof(vec2));
  Ring<Buffer> velBufRing            = grf.createBufferRing(BufferIntent::FrequentUpdate, g_maxParticles * sizeof(vec2));
  Ring<Buffer> boundsBufRing         = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4));
  Ring<Buffer> mortonBufRing         = grf.createBufferRing(BufferIntent::Readable, sizeof(u32) * g_maxParticles);
  Ring<Buffer> indexBufRing          = grf.createBufferRing(BufferIntent::Readable, sizeof(u32) * g_maxParticles);
  Ring<Buffer> mortonScratchBufRing  = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticles);
  Ring<Buffer> indexScratchBufRing   = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticles);
  Ring<Buffer> boundsScratchBufRing  = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4) * g_maxBoundsTiles);
  Ring<Buffer> histBufRing           = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_buckets * g_maxSortTiles);
  Ring<Buffer> offsetBufRing         = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_buckets * g_maxSortTiles);
  Ring<Buffer> nodeBufRing           = grf.createBufferRing(BufferIntent::Readable, sizeof(InternalNode) * g_maxParticles);
  Ring<Buffer> nodeParentBufRing     = grf.createBufferRing(BufferIntent::Readable, sizeof(u32) * g_maxParticles);
  Ring<Buffer> leafBufRing           = grf.createBufferRing(BufferIntent::Readable, sizeof(u32) * g_maxParticles);
  Ring<Buffer> aggBufRing            = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(u32) * g_maxParticles);
  Ring<Buffer> comBufRing            = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4) * g_maxParticles);
  Ring<Buffer> aabbBufRing           = grf.createBufferRing(BufferIntent::Readable, sizeof(vec4) * g_maxParticles);
  Ring<Buffer> scratchComBufRing     = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4) * 2 * g_maxParticles);
  Ring<Buffer> scratchAabbBufRing    = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(vec4) * 2 * g_maxParticles);
  Ring<Buffer> densityBufRing        = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(f32)  * g_maxParticles);
  Ring<Buffer> energyBufRing         = grf.createBufferRing(BufferIntent::GPUOnly, sizeof(f32)  * g_maxParticles);
  Buffer constBuf                    = grf.createBuffer(BufferIntent::SingleUpdate, sizeof(Constants));

  {
    std::vector<vec2> initData(g_maxParticles, vec2(0.0));
    for (i32 i = 0; i < g_flightFrames; ++i) {
      posBufRing[i].write(initData);
      velBufRing[i].write(initData);
    }
  }

  constBuf.write(Constants{
    .G          = g_gravConst,
    .M          = g_particleMass,
    .H          = g_particleRadius,
    .cs         = g_specificHeat,
    .smoothing  = g_particleSmoothing
  });

  Input& input = grf.input();

  u32 prevIndex = g_flightFrames - 1;
  u32 N = 0;
  f32 spawnRadius = 0.1f;
  f32 spawnTimer = 0.0f;
  const f32 spawnInterval = 0.1f;
  bool showAABBs = false;
  while (grf.running([&](){ return !grf.gui().wantsKeyboard() && input.isJustPressed(Key::Escape); })) {
    auto [frameIndex, dt] = grf.beginFrame();

    spawnTimer = std::min(spawnTimer + dt, spawnInterval);

    auto& compCmd  = compCmdRing[frameIndex];
    auto& graphCmd = graphCmdRing[frameIndex];

    auto& flightFence = flightFenceRing[frameIndex];
    auto& imgSem      = imgSemRing[frameIndex];
    auto& compSem     = compSemRing[frameIndex];
    auto& graphSem    = graphSemRing[frameIndex];

    auto& prevPosBuf       = posBufRing[prevIndex];
    auto& posBuf           = posBufRing[frameIndex];
    auto& prevVelBuf       = velBufRing[prevIndex];
    auto& velBuf           = velBufRing[frameIndex];
    auto& boundsBuf        = boundsBufRing[frameIndex];
    auto& mortonBuf        = mortonBufRing[frameIndex];
    auto& indexBuf         = indexBufRing[frameIndex];
    auto& mortonScratchBuf = mortonScratchBufRing[frameIndex];
    auto& indexScratchBuf  = indexScratchBufRing[frameIndex];
    auto& boundsScratchBuf = boundsScratchBufRing[frameIndex];
    auto& histBuf          = histBufRing[frameIndex];
    auto& offsetBuf        = offsetBufRing[frameIndex];
    auto& nodeBuf          = nodeBufRing[frameIndex];
    auto& nodeParentBuf    = nodeParentBufRing[frameIndex];
    auto& leafBuf          = leafBufRing[frameIndex];
    auto& aggBuf           = aggBufRing[frameIndex];
    auto& comBuf           = comBufRing[frameIndex];
    auto& aabbBuf          = aabbBufRing[frameIndex];
    auto& scratchComBuf    = scratchComBufRing[frameIndex];
    auto& scratchAabbBuf   = scratchAabbBufRing[frameIndex];
    auto& densityBuf       = densityBufRing[frameIndex];
    auto& energyBuf        = energyBufRing[frameIndex];

    const u32 boundsTiles = (N + 255) / 256;
    const u32 sortTiles   = (N + g_keysPerTile - 1) / g_keysPerTile;

    grf.waitFences({ flightFence });
    grf.resetFences({ flightFence });

    SwapchainImage renderTarget = grf.nextSwapchainImage(imgSem);

    compCmd.begin();
    compCmd.beginProfile("compute");

    compCmd.beginProfile("bounds 1");
    compCmd.bindPipeline(boundsPass1);
    compCmd.push(Bounds1Data{
      .posBufAddr     = prevPosBuf.address(),
      .scratchBufAddr = boundsScratchBuf.address(),
      .N              = N
    });
    if (N > 0)
      compCmd.dispatch(boundsTiles);
    compCmd.endProfile();

    compCmd.barrier(boundsScratchBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("bounds 2");
    compCmd.bindPipeline(boundsPass2);
    compCmd.push(Bounds2Data{
      .scratchBufAddr = boundsScratchBuf.address(),
      .boundsBufAddr  = boundsBuf.address(),
      .tiles          = boundsTiles
    });
    if (N > 0)
      compCmd.dispatch(1);
    compCmd.endProfile();

    compCmd.barrier(boundsBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("morton encode");
    compCmd.bindPipeline(encodePass);
    compCmd.push(EncodeData{
      .posBufAddr     = prevPosBuf.address(),
      .boundsBufAddr  = boundsBuf.address(),
      .mortonBufAddr  = mortonBuf.address(),
      .indexBufAddr   = indexBuf.address(),
      .N              = N
    });
    if (N > 0) {
      auto [x, y, z] = encodeShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.barrier(mortonBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
    compCmd.barrier(indexBuf,  BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    u64 inKeysAddr  = mortonBuf.address();
    u64 outKeysAddr = mortonScratchBuf.address();
    u64 inValsAddr  = indexBuf.address();
    u64 outValsAddr = indexScratchBuf.address();

    compCmd.beginProfile("sort");
    for (u32 pass = 0; pass < 8; ++pass) {
      const u32 shift = pass * 4;

      compCmd.bindPipeline(sortPass1);
      compCmd.push(Sort1Data{
        .keyBufAddr   = inKeysAddr,
        .histBufAddr  = histBuf.address(),
        .N            = N,
        .tiles        = sortTiles,
        .shift        = shift
      });
      if (N > 0)
        compCmd.dispatch(sortTiles);

      compCmd.barrier(histBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

      compCmd.bindPipeline(sortPass2);
      compCmd.push(Sort2Data{
        .histBufAddr    = histBuf.address(),
        .offsetBufAddr  = offsetBuf.address(),
        .tiles          = sortTiles
      });
      if (N > 0)
        compCmd.dispatch(1);

      compCmd.barrier(offsetBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

      compCmd.bindPipeline(sortPass3);
      compCmd.push(Sort3Data{
        .inKeysBufAddr  = inKeysAddr,
        .outKeysBufAddr = outKeysAddr,
        .inValsBufAddr  = inValsAddr,
        .outValsBufAddr = outValsAddr,
        .offsetBufAddr  = offsetBuf.address(),
        .N              = N,
        .tiles          = sortTiles,
        .shift          = shift
      });
      if (N > 0)
        compCmd.dispatch(sortTiles);

      compCmd.barrier();

      std::swap(inKeysAddr, outKeysAddr);
      std::swap(inValsAddr, outValsAddr);
    }
    compCmd.endProfile();

    compCmd.beginProfile("clear");
    compCmd.bindPipeline(clearPass);
    compCmd.push(ClearData{
      .aggBufAddr        = aggBuf.address(),
      .nodeParentBufAddr = nodeParentBuf.address(),
      .N                 = N
    });
    if (N > 0) {
      auto [x, y, z] = clearShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.barrier(aggBuf,        BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
    compCmd.barrier(nodeParentBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("build");
    compCmd.bindPipeline(buildPass);
    compCmd.push(BuildData{
      .mortonBufAddr     = mortonBuf.address(),
      .nodeBufAddr       = nodeBuf.address(),
      .nodeParentBufAddr = nodeParentBuf.address(),
      .leafBufAddr       = leafBuf.address(),
      .N                 = N
    });
    if (N > 0) {
      auto [x, y, z] = buildShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.barrier(nodeBuf,       BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
    compCmd.barrier(nodeParentBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
    compCmd.barrier(leafBuf,       BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("aggregate");
    compCmd.bindPipeline(aggregatePass);
    compCmd.push(AggregateData{
      .aggBufAddr         = aggBuf.address(),
      .scratchComBufAddr  = scratchComBuf.address(),
      .scratchAabbBufAddr = scratchAabbBuf.address(),
      .comBufAddr         = comBuf.address(),
      .aabbBufAddr        = aabbBuf.address(),
      .indexBufAddr       = indexBuf.address(),
      .posBufAddr         = prevPosBuf.address(),
      .leafBufAddr        = leafBuf.address(),
      .nodeParentBufAddr  = nodeParentBuf.address(),
      .nodeBufAddr        = nodeBuf.address(),
      .N                  = N,
      .M                  = g_particleMass
    });
    if (N >= 2) {
      auto [x, y, z] = aggregateShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.barrier(aabbBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
    compCmd.barrier(comBuf,  BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("density");
    compCmd.bindPipeline(densityPass);
    compCmd.push(DensityData{
      .indexBufAddr   = indexBuf.address(),
      .nodeBufAddr    = nodeBuf.address(),
      .aabbBufAddr    = aabbBuf.address(),
      .posBufAddr     = prevPosBuf.address(),
      .densityBufAddr = densityBuf.address(),
      .rootIndex      = 0,
      .N              = N,
      .M              = g_particleMass,
      .H              = g_particleRadius
    });
    if (N >= 2) {
      auto [x, y, z] = densityShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.barrier(densityBuf, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);

    compCmd.beginProfile("force");
    compCmd.bindPipeline(forcePass);
    compCmd.push(ForceData{
      .indexBufAddr   = indexBuf.address(),
      .nodeBufAddr    = nodeBuf.address(),
      .comBufAddr     = comBuf.address(),
      .aabbBufAddr    = aabbBuf.address(),
      .densityBufAddr = densityBuf.address(),
      .prevPosBufAddr = prevPosBuf.address(),
      .posBufAddr     = posBuf.address(),
      .prevVelBufAddr = prevVelBuf.address(),
      .velBufAddr     = velBuf.address(),
      .energyBufAddr  = energyBuf.address(),
      .constBufAddr   = constBuf.address(),
      .rootIndex      = 0,
      .theta          = g_theta,
      .N              = N,
      .dt             = dt
    });
    if (N > 0) {
      auto [x, y, z] = forceShader.threadGroup();
      compCmd.dispatch((N + x - 1) / x);
    }
    compCmd.endProfile();

    compCmd.endProfile();
    compCmd.end();

    graphCmd.begin();
    graphCmd.beginProfile("graphics");

    graphCmd.transition(renderTarget, Layout::Undefined, Layout::ColorAttachmentOptimal);
    graphCmd.beginRendering({ ColorAttachment{ .img = renderTarget, .loadOp = LoadOp::Clear } });

    graphCmd.beginProfile("particles");
    graphCmd.bindPipeline(graphicsPass);
    graphCmd.push(GraphicsData{
      .posBufAddr     = posBuf.address(),
      .velBufAddr     = velBuf.address(),
      .energyBufAddr  = energyBuf.address(),
      .screenDims     = uvec2(g_windowWidth, g_windowHeight),
      .N              = N,
      .R              = g_particleRadius,
      .M              = g_particleMass,
      .K              = g_boltzmann
    });
    if (N > 0)
      graphCmd.draw(6 * N);
    graphCmd.endProfile();

    if (showAABBs && N >= 2) {
      graphCmd.beginProfile("debug aabbs");
      graphCmd.bindPipeline(aabbPass);
      graphCmd.push(AABBData{
        .nodeAABBBufAddr = aabbBuf.address(),
        .screenDims      = uvec2(g_windowWidth, g_windowHeight),
        .N               = N
      });
      graphCmd.draw(8 * (N - 1));
      graphCmd.endProfile();
    }

    grf.gui().beginFrame();
    grf.profiler().render();

    bool shouldReset = false;
    bool dumpTree = false;
    ImGui::Begin("Particles");
      ImGui::Text("Count");
      ImGui::SameLine(0.0, 40.0);
      ImGui::Text("%d", N);

      ImGui::SliderFloat("spawn radius", &spawnRadius, 0.01f, 0.30f, "%.3f");
      ImGui::Checkbox("show tree AABBs", &showAABBs);

      shouldReset = ImGui::Button("reset", ImVec2(100, 25));
      ImGui::SameLine();
      dumpTree = ImGui::Button("dump tree", ImVec2(100, 25));
    ImGui::End();

    graphCmd.beginProfile("gui");
    grf.gui().render(graphCmd);
    graphCmd.endProfile();

    graphCmd.endRendering();
    graphCmd.transition(renderTarget, Layout::ColorAttachmentOptimal, Layout::PresentSrc);

    graphCmd.endProfile();
    graphCmd.end();

    grf.waitForResourceUpdates();

    grf.submit(compCmd, {}, { compSem });
    grf.submit(graphCmd, { imgSem, compSem }, { graphSem }, flightFence);
    grf.present(renderTarget, { graphSem });

    if (dumpTree && N >= 2) {
      grf.waitFences({ flightFence });

      std::vector<InternalNode> nodes(N);
      std::vector<u32>          nodeParents(N);
      std::vector<u32>          leafParents(N);
      std::vector<vec4>         aabbs(N);
      std::vector<vec2>         positions(N);
      std::vector<u32>          mortons(N);
      std::vector<u32>          indices(N);
      nodeBuf.read(nodes);
      nodeParentBuf.read(nodeParents);
      leafBuf.read(leafParents);
      aabbBuf.read(aabbs);
      prevPosBuf.read(positions);
      mortonBuf.read(mortons);
      indexBuf.read(indices);

      std::printf("\n=== TREE DUMP (N=%u) ===\n", N);
      std::printf("Sorted index : morton    -> particle@pos\n");
      for (u32 k = 0; k < N; ++k) {
        std::printf("  [%2u] %08x -> P%-2u @(%.3f, %.3f)\n",
                    k, mortons[k], indices[k],
                    positions[indices[k]].x, positions[indices[k]].y);
      }
      std::printf("\nLeaf parents:\n");
      for (u32 k = 0; k < N; ++k) {
        std::printf("  leaf[%2u] -> internal[%u]\n", k, leafParents[k]);
      }
      std::printf("\nInternal nodes (left, right, parent):\n");
      const u32 LEAF_BIT = 0x80000000u;
      for (u32 k = 0; k + 1 < N; ++k) {
        auto fmtChild = [](u32 c) {
          return (c & LEAF_BIT)
              ? std::format("leaf[{}]",   c & 0x7FFFFFFFu)
              : std::format("intnl[{}]", c);
        };
        std::string parentStr = (nodeParents[k] == 0xFFFFFFFFu)
                              ? std::string("ROOT")
                              : std::format("intnl[{}]", nodeParents[k]);
        std::printf("  internal[%2u]: L=%s, R=%s, parent=%s, AABB=(%.3f,%.3f)-(%.3f,%.3f)\n",
                    k, fmtChild(nodes[k].left).c_str(), fmtChild(nodes[k].right).c_str(),
                    parentStr.c_str(),
                    aabbs[k].x, aabbs[k].y, aabbs[k].z, aabbs[k].w);
      }
      std::printf("======================\n\n");
      std::fflush(stdout);
    }

    if (!shouldReset && N < g_maxParticles && !grf.gui().wantsMouse()
        && input.isPressed(MouseButton::Left) && spawnTimer >= spawnInterval) {
      auto [x, y] = input.cursorPos();
      f32 ar = static_cast<f32>(g_windowWidth) / static_cast<f32>(g_windowHeight);
      vec2 center = vec2(ar * (2.0 * x / g_windowWidth - 1.0), 2.0 * y / g_windowHeight - 1.0);

      std::vector<vec2> particles = spawnParticles(center, spawnRadius, N);
      if (!particles.empty()) {
        posBuf.write(particles, sizeof(vec2) * N);
        N += static_cast<u32>(particles.size());
        spawnTimer = 0.0f;
      }
    }

    if (shouldReset) {
      std::vector<vec2> zeros(g_maxParticles, vec2(0.0));
      for (i32 i = 0; i < g_flightFrames; ++i) {
        posBufRing[i].write(zeros);
        velBufRing[i].write(zeros);
      }
      N = 0;
    }

    prevIndex = frameIndex;
    grf.endFrame();
  }
}

std::vector<vec2> spawnParticles(vec2 center, f32 radius, u32 currentCount) {
  static std::mt19937_64 gen(std::random_device{}());

  if (currentCount >= g_maxParticles) return {};

  f32 theta = std::uniform_real_distribution<f32>(0.0, 2.0 * std::numbers::pi_v<f32>)(gen);
  f32 r     = radius * std::sqrt(std::uniform_real_distribution<f32>(0.0, 1.0)(gen));

  return { center + vec2(r * std::cos(theta), r * std::sin(theta)) };
}
