# Your first triangle

Render an RGB triangle to a window. Full project from scratch.

Prerequisite: GRF installed (see [getting-started.md](../getting-started.md)).

## Project layout

```
my_triangle/
  CMakeLists.txt
  src/
    main.cpp
  shaders/
    triangle.vert.gsl
    triangle.frag.gsl
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 4.3)
project(my_triangle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(grf REQUIRED)

add_executable(my_triangle src/main.cpp)
target_link_libraries(my_triangle PRIVATE grf::grf)

# Pass the shader directory path through to the binary so it can find
# .gsl files at runtime.
target_compile_definitions(my_triangle PRIVATE
  "SHADER_DIR=\"${CMAKE_SOURCE_DIR}/shaders\""
)
```

## shaders/triangle.vert.gsl

```glsl
const vec2 positions[3] = vec2[3](
  vec2( 0.0, -0.5),
  vec2( 0.5,  0.5),
  vec2(-0.5,  0.5)
);

const vec3 colors[3] = vec3[3](
  vec3(1, 0, 0),
  vec3(0, 1, 0),
  vec3(0, 0, 1)
);

out vec3 vColor;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
  vColor      = colors[gl_VertexIndex];
}
```

The positions and colors are baked into the shader as constant arrays.
`gl_VertexIndex` runs 0..2 across the three vertices of `cmd.draw(3)`.

## shaders/triangle.frag.gsl

```glsl
in vec3 vColor;
out vec4 color;

void main() {
  color = vec4(vColor, 1);
}
```

## src/main.cpp

```cpp
#include <grf/grf.hpp>
#include <array>
#include <format>

int main() {
  grf::GRF grf(grf::Settings{
    .windowTitle = "First Triangle",
    .windowSize  = { 1280, 720 },
  });

  auto vs = grf.compileShader(
    grf::ShaderType::Vertex,
    std::format("{}/triangle.vert.gsl", SHADER_DIR));
  auto fs = grf.compileShader(
    grf::ShaderType::Fragment,
    std::format("{}/triangle.frag.gsl", SHADER_DIR));

  auto pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .cullMode     = grf::CullMode::None,
  });

  auto cmds     = grf.createCmdRing(grf::QueueType::Graphics);
  auto acquired = grf.createSemaphoreRing();
  auto rendered = grf.createSemaphoreRing();
  auto fences   = grf.createFenceRing(/*signaled=*/true);

  while (grf.running()) {
    auto [idx, dt] = grf.beginFrame();

    grf.waitFences({ fences[idx] });
    grf.resetFences({ fences[idx] });

    auto swap = grf.nextSwapchainImage(acquired[idx]);

    cmds[idx].begin();
    cmds[idx].transition(swap, grf::Layout::Undefined,
                                grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(std::array{
      grf::ColorAttachment{
        .img        = swap,
        .loadOp     = grf::LoadOp::Clear,
        .clearValue = { 0.05f, 0.07f, 0.10f, 1.0f },
      }
    });
    cmds[idx].bindPipeline(pipe);
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

## Build and run

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./my_triangle
```

A 1280×720 window opens with a colored triangle on a dark blue background.
Close it to exit.

## What each piece is doing

`grf::GRF grf(...)`. Initializes the framework. Window opens, Vulkan
instance + device come up, swapchain + descriptor heap built, ImGui
initialized. After the constructor returns you are ready to record commands.

`compileShader(...)`. Reads `.gsl` from disk, runs the GSL preprocessor,
hands GLSL to shaderc, caches the SPIR-V. Subsequent runs of the program
skip recompilation because the cache hits.

`createGraphicsPipeline(...)`. Builds a `VkPipeline` with the given vertex
and fragment shaders. `colorFormats` must match the format of whatever you
will render to (the swapchain in this case — `bgra8_srgb` matches the
default `Settings::swapchainFormat`).

`createCmdRing` / `createSemaphoreRing` / `createFenceRing`. Allocate
per-frame state, sized for the number of frames in flight (default 2).

The frame loop: wait for the GPU to finish the previous use of this
slot's command buffer, acquire the next swapchain image, record (transition
to color attachment layout, begin rendering, bind pipeline, draw, end
rendering, transition to present layout), submit, present, end frame.

## Common issues

**Black window.** Likely the swapchain format does not match the
pipeline's `colorFormats`. Both are set to `bgra8_srgb` in this example;
keep them in sync.

**Triangle culled / not visible.** The example's vertices are wound
clockwise (in NDC with Vulkan's Y-down coordinate system, the screen-space
winding flips). Setting `cullMode = CullMode::None` is the safe default
when you are not sure. To use back-face culling, you may need to swap two
vertex positions or set `frontFace = FrontFace::Clockwise`.

**Crash on first frame.** Check that `fences` were created with
`signaled = true`. Otherwise the first `waitFences` waits forever on a
fence that no one has ever signaled.

## Where to next

- [guides/texture-upload.md](texture-upload.md) — load an image, sample it
  in a fragment shader.
- [guides/compute-pass.md](compute-pass.md) — run a compute shader, feed
  its output into a graphics pass.
- [concepts/shaders.md](../concepts/shaders.md) — full GSL syntax, including
  push constants and buffer references for vertex pulling.
