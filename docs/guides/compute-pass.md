# Compute writes, graphics reads

A two-pass setup: a compute shader writes per-pixel values to a storage
image; a graphics pass samples that image to render to the swapchain.
Demonstrates layout transitions and barriers between passes.

Builds on [first-triangle.md](first-triangle.md) and
[texture-upload.md](texture-upload.md).

## What we're building

A scrolling animated checkerboard. Compute shader generates the pattern
into an `Img2D` each frame; the fragment shader samples it as a texture.

## Project layout

```
my_compute/
  CMakeLists.txt
  src/main.cpp
  shaders/
    pattern.gsl       # compute
    fullscreen.vert.gsl
    fullscreen.frag.gsl
```

## shaders/pattern.gsl

Compute shader. Writes a checkerboard pattern shifted by `time`:

```glsl
thread_group [8, 8, 1];

push {
  uint   target;     // grf_Img2D[] slot
  float  time;
  uvec2  size;       // image dims
};

void main() {
  uvec2 px = gl_GlobalInvocationID.xy;
  if (px.x >= size.x || px.y >= size.y) return;

  uvec2 cell = (px + uvec2(time * 32, time * 16)) / 32u;
  bool  on   = ((cell.x ^ cell.y) & 1u) == 0u;

  vec3 a = vec3(0.85, 0.65, 0.40);
  vec3 b = vec3(0.10, 0.15, 0.20);
  vec3 c = on ? a : b;

  imageStore(grf_Img2D[target], ivec2(px), vec4(c, 1));
}
```

## shaders/fullscreen.vert.gsl

Three-vertex fullscreen triangle (no quad needed):

```glsl
out vec2 vUV;

void main() {
  // standard "big triangle" trick — covers the whole screen
  vUV         = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(vUV * 2 - 1, 0, 1);
}
```

## shaders/fullscreen.frag.gsl

```glsl
push {
  uint source;
  uint linearSampler;
};

in  vec2 vUV;
out vec4 color;

void main() {
  color = texture(sampler2D(grf_Tex2D[source],
                             grf_Sampler[linearSampler]), vUV);
}
```

## src/main.cpp

```cpp
#include <grf/grf.hpp>
#include <array>
#include <format>

int main() {
  grf::GRF grf(grf::Settings{
    .windowTitle = "Compute -> Graphics",
    .windowSize  = { 1280, 720 },
  });

  // Pipelines
  auto computeShader  = grf.compileShader(grf::ShaderType::Compute,
                          std::format("{}/pattern.gsl", SHADER_DIR));
  auto vs             = grf.compileShader(grf::ShaderType::Vertex,
                          std::format("{}/fullscreen.vert.gsl", SHADER_DIR));
  auto fs             = grf.compileShader(grf::ShaderType::Fragment,
                          std::format("{}/fullscreen.frag.gsl", SHADER_DIR));

  auto computePipe = grf.createComputePipeline(computeShader);
  auto graphicsPipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .cullMode     = grf::CullMode::None,
  });

  // The image compute writes to. Same dimensions as the swapchain.
  const uint32_t W = 1280;
  const uint32_t H = 720;
  auto target = grf.createImg2D(grf::Format::rgba8_unorm, W, H);

  // Sampler for the graphics pass to read it
  auto sampler = grf.createSampler(grf::SamplerSettings{
    .magFilter = grf::Filter::Linear,
    .minFilter = grf::Filter::Linear,
    .uMode     = grf::SampleMode::ClampToEdge,
    .vMode     = grf::SampleMode::ClampToEdge,
  });

  struct ComputePush {
    uint32_t target;
    float    time;
    uint32_t sizeX;
    uint32_t sizeY;
  };
  struct GraphicsPush {
    uint32_t source;
    uint32_t sampler;
  };

  auto cmds     = grf.createCmdRing(grf::QueueType::Graphics);
  auto acquired = grf.createSemaphoreRing();
  auto rendered = grf.createSemaphoreRing();
  auto fences   = grf.createFenceRing(true);

  float t = 0;

  while (grf.running()) {
    auto [idx, dt] = grf.beginFrame();
    t += dt;

    grf.waitFences({ fences[idx] });
    grf.resetFences({ fences[idx] });

    auto swap = grf.nextSwapchainImage(acquired[idx]);

    cmds[idx].begin();

    // ── Compute pass ──
    // First time only: target starts as Undefined → General.
    // After that: ShaderReadOptimal → General.
    cmds[idx].transition(target, grf::Layout::Undefined,
                                  grf::Layout::General);

    cmds[idx].bindPipeline(computePipe);
    cmds[idx].push(ComputePush{
      .target = target.storageHeapIndex(),  // grf_Img2D[] slot, image in General
      .time   = t,
      .sizeX  = W,
      .sizeY  = H,
    });
    cmds[idx].dispatch((W + 7) / 8, (H + 7) / 8, 1);

    // ── Barrier: compute writes -> graphics reads ──
    // Transition target General -> ShaderReadOptimal.
    cmds[idx].transition(target, grf::Layout::General,
                                  grf::Layout::ShaderReadOptimal);

    // ── Graphics pass ──
    cmds[idx].transition(swap, grf::Layout::Undefined,
                                grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(std::array{
      grf::ColorAttachment{ .img = swap, .loadOp = grf::LoadOp::DontCare }
    });

    cmds[idx].bindPipeline(graphicsPipe);
    cmds[idx].push(GraphicsPush{
      .source  = target.sampledHeapIndex(),  // grf_Tex2D[] slot, image in ShaderReadOptimal
      .sampler = sampler.heapIndex(),
    });
    cmds[idx].draw(3);

    cmds[idx].endRendering();
    cmds[idx].transition(swap, grf::Layout::ColorAttachmentOptimal,
                                grf::Layout::PresentSrc);

    cmds[idx].end();

    grf.submit(cmds[idx],
               std::array{ acquired[idx] },
               std::array{ rendered[idx] },
               fences[idx]);
    grf.present(swap, std::array{ rendered[idx] });
    grf.endFrame();
  }
}
```

## What's happening

**Compute pass.** `target` is an `Img2D` — a storage image. The compute
pipeline binds, push constants are written (heap slot of `target`, current
time, image dims), `dispatch` launches `ceil(W/8) * ceil(H/8) * 1`
workgroups of 8×8 threads each. Each thread writes one pixel.

**Layout transition.** Storage image writes happen in `Layout::General`;
sampling happens in `Layout::ShaderReadOptimal`. The transition between
the two passes also acts as a memory barrier — the GPU finishes all
compute writes before any subsequent shader reads.

For just a buffer, `cmd.barrier(buf, ShaderWrite, ShaderRead)` would be
the equivalent. For images, a layout transition handles both jobs (memory
ordering + access-type signaling).

**Graphics pass.** `Img2D` has two heap slots: `storageHeapIndex()` for
storage access (used in the compute pass) and `sampledHeapIndex()` for
sampled access (used in the fragment shader). Push the right one for the
binding the shader expects — they are different integers, and using the
wrong one is a validation error.

The fragment shader uses the standard "fullscreen triangle" trick: a single
triangle that covers the entire screen, with UVs derived from
`gl_VertexIndex` to give 0..1 across the framebuffer.

## Common issues

**Validation layer complains about layout.** Likely `Undefined → General`
on the first frame works, but subsequent frames need
`ShaderReadOptimal → General` — the framework tracks the layout per image,
so you must specify the correct `from` argument every time. The example
above transitions `Undefined → General` on every frame, which works
because `Undefined` is special (it discards old contents). For more careful
tracking, use the actual previous layout.

**Black or garbage output.** Most likely a slot type mismatch. The compute
shader needs `storageHeapIndex()` (writing to `grf_Img2D[]` in `General`
layout); the fragment shader needs `sampledHeapIndex()` (reading from
`grf_Tex2D[]` in `ShaderReadOptimal`). Pushing the wrong one shows up as
either nothing rendered, undefined contents, or — with validation enabled
— `VUID-VkWriteDescriptorSet-descriptorType-00319`.

**Compute slower than expected.** `8 × 8` is a typical workgroup size for
2D image processing. Try `16 × 16` for memory-bound kernels, or smaller
for branchier work. Profile with `cmd.beginProfile("compute") /
endProfile()` to measure.

## Where to next

- [api/cmd.md](../api/cmd.md) — full reference for `dispatch`,
  `transition`, `barrier`.
- [concepts/synchronization.md](../concepts/synchronization.md) — when
  you need explicit `cmd.barrier(buf, ...)` instead of relying on layout
  transitions.
- [api/profiler.md](../api/profiler.md) — measuring per-pass GPU time.
