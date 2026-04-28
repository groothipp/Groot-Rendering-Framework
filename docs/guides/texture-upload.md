# Loading and sampling a texture

Decode a PNG/JPG, upload it to a `Tex2D`, sample it in a fragment shader.
Builds on [first-triangle.md](first-triangle.md).

## Project additions

Add an image file under `assets/`:

```
my_textured_quad/
  CMakeLists.txt
  src/main.cpp
  shaders/
    quad.vert.gsl
    quad.frag.gsl
  assets/
    crate.png
```

Add a `target_compile_definitions` for `ASSET_DIR` alongside `SHADER_DIR`.

## shaders/quad.vert.gsl

A full-screen quad pulled from a const array, with UVs:

```glsl
const vec2 positions[6] = vec2[6](
  vec2(-1, -1), vec2( 1, -1), vec2( 1,  1),
  vec2(-1, -1), vec2( 1,  1), vec2(-1,  1)
);

const vec2 uvs[6] = vec2[6](
  vec2(0, 0), vec2(1, 0), vec2(1, 1),
  vec2(0, 0), vec2(1, 1), vec2(0, 1)
);

out vec2 vUV;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
  vUV         = uvs[gl_VertexIndex];
}
```

## shaders/quad.frag.gsl

```glsl
push {
  uint albedo;
  uint linearSampler;
};

in  vec2 vUV;
out vec4 color;

void main() {
  color = texture(sampler2D(grf_Tex2D[albedo],
                             grf_Sampler[linearSampler]), vUV);
}
```

The push constants are heap indices. The shader looks them up in the
bindless `grf_Tex2D[]` and `grf_Sampler[]` arrays and combines them at the
sample call site.

## src/main.cpp

```cpp
#include <grf/grf.hpp>
#include <array>
#include <format>

int main() {
  grf::GRF grf(grf::Settings{
    .windowTitle = "Textured Quad",
    .windowSize  = { 1024, 1024 },
  });

  // Shaders + pipeline
  auto vs = grf.compileShader(grf::ShaderType::Vertex,
                               std::format("{}/quad.vert.gsl", SHADER_DIR));
  auto fs = grf.compileShader(grf::ShaderType::Fragment,
                               std::format("{}/quad.frag.gsl", SHADER_DIR));
  auto pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .cullMode     = grf::CullMode::None,
  });

  // Load image from disk
  grf::ImageData img = grf::readImage(std::format("{}/crate.png", ASSET_DIR));

  // Allocate texture, upload pixels, leave in ShaderReadOptimal
  auto tex = grf.createTex2D(grf::Format::rgba8_srgb, img.width, img.height);
  tex.write(img.bytes, grf::Layout::ShaderReadOptimal);

  // Sampler — one is enough for any number of textures
  auto sampler = grf.createSampler(grf::SamplerSettings{
    .magFilter            = grf::Filter::Linear,
    .minFilter            = grf::Filter::Linear,
    .uMode                = grf::SampleMode::Repeat,
    .vMode                = grf::SampleMode::Repeat,
    .anisotropicFiltering = true,
  });

  // Push struct must match the GSL `push { }` block exactly under std430.
  struct PushData { uint32_t albedo; uint32_t sampler; };

  auto cmds     = grf.createCmdRing(grf::QueueType::Graphics);
  auto acquired = grf.createSemaphoreRing();
  auto rendered = grf.createSemaphoreRing();
  auto fences   = grf.createFenceRing(true);

  while (grf.running()) {
    auto [idx, dt] = grf.beginFrame();

    grf.waitFences({ fences[idx] });
    grf.resetFences({ fences[idx] });

    auto swap = grf.nextSwapchainImage(acquired[idx]);

    cmds[idx].begin();
    cmds[idx].transition(swap, grf::Layout::Undefined,
                                grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(std::array{
      grf::ColorAttachment{ .img = swap, .loadOp = grf::LoadOp::Clear }
    });

    cmds[idx].bindPipeline(pipe);
    cmds[idx].push(PushData{ tex.heapIndex(), sampler.heapIndex() });
    cmds[idx].draw(6);

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

## What changed from the triangle

The shader now declares `push { uint albedo; uint linearSampler; }` — two
uint heap indices. On the C++ side, a matching `PushData` struct is pushed
once per frame.

`grf::readImage(path)` decodes via stb_image. The result has byte data,
width, and height. `Tex2D::write(bytes, finalLayout)` queues a staging
copy + transition; the upload happens on a worker thread before the next
`waitForResourceUpdates()` (which `beginFrame` triggers).

The first frame's render submission may fence-wait on the upload completing
internally (the resource manager tracks last-use values); you do not need
to add explicit fencing.

## Common issues

**Sampled colors look washed out / wrong gamma.** sRGB vs linear mismatch.
PNG content is usually sRGB-encoded; loading into an `rgba8_srgb` texture
is correct (the GPU does sRGB → linear on sample for you). If your
results look too bright, you might be loading sRGB data into a `_unorm`
format and not getting the sRGB conversion.

**Texture appears as solid color.** Likely the heap index in the push
struct is wrong. Print `tex.heapIndex()` and `sampler.heapIndex()` and
verify they are both small positive integers. A default-constructed
texture would give a garbage index.

**Triangle (or quad) is upside down.** Vulkan's Y is down. UV maps that
expect OpenGL's Y-up direction will appear flipped. Flip your UVs in the
vertex shader (`vUV.y = 1 - vUV.y`) or in the texture data.

## Where to next

- [guides/compute-pass.md](compute-pass.md) — write to a storage image
  from compute and sample it in a graphics pass.
- [api/resources.md](../api/resources.md) — full reference for `Tex2D`,
  `Img2D`, `Cubemap`.
