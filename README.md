# GRF — Groot Rendering Framework

A C++26 Vulkan rendering framework. Bindless descriptors, buffer device
addresses, custom shader DSL.

GRF is not an engine. It does not know what a mesh is. It handles descriptor
management, resource lifetimes, swapchain plumbing, queue synchronization,
shader compilation, and ImGui wiring. It does not handle anything else.

```cpp
#include <grf/grf.hpp>
#include <array>

int main() {
  grf::GRF grf;

  auto vs   = grf.compileShader(grf::ShaderType::Vertex,   "triangle.vert.gsl");
  auto fs   = grf.compileShader(grf::ShaderType::Fragment, "triangle.frag.gsl");
  auto pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
    .colorFormats = { grf::Format::bgra8_srgb },
    .cullMode     = grf::CullMode::None,
  });

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
    cmds[idx].transition(swap, grf::Layout::Undefined, grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(std::array{
      grf::ColorAttachment{ .img = swap, .loadOp = grf::LoadOp::Clear }
    });
    cmds[idx].bindPipeline(pipe);
    cmds[idx].draw(3);
    cmds[idx].endRendering();
    cmds[idx].transition(swap, grf::Layout::ColorAttachmentOptimal, grf::Layout::PresentSrc);
    cmds[idx].end();

    grf.submit(cmds[idx], std::array{ acquired[idx] }, std::array{ rendered[idx] }, fences[idx]);
    grf.present(swap, std::array{ rendered[idx] });
    grf.endFrame();
  }
}
```

## Features

- Single global descriptor heap. All textures, samplers, storage images live
  in one set; shaders look them up by integer index.
- Buffer device address everywhere. Vertex data is pulled from buffer
  references in shaders, not bound through vertex input.
- GSL shader DSL. Bindless heap and push constants are syntax, not boilerplate.
- On-disk SPIR-V cache. Compiled shaders are content-addressed; subsequent
  loads of unchanged source skip glslang/shaderc entirely.
- Vulkan 1.3 only. Dynamic rendering, sync2, timeline semaphores. No legacy paths.
- Handles are PIMPL value types backed by `shared_ptr<Impl>`. Default-constructible
  to a null state.
- Tracked resource lifetimes. Drop a handle while the GPU is still using it; the
  framework destroys it once the GPU is done.
- Multithreaded resource updates. Buffer and image uploads run on dedicated
  worker threads with their own command pools, in parallel with frame
  recording. Synchronized via timeline semaphores; the main thread never
  blocks on transfers.
- Built-in GPU-timestamp profiler with ImGui readout.
- Optional ImGui integration.

## Requirements

- C++26 compiler (Clang 18+, GCC 14+, MSVC 19.40+)
- CMake 4.3+
- Vulkan SDK 1.4+ with `shaderc_combined`, `glslang`, `SPIRV-Tools`
- glfw3
- glm
- Catch2 v3 (only for tests)

Hardware ray-tracing extensions (`VK_KHR_acceleration_structure`,
`VK_KHR_ray_query`) are detected at startup. They are not currently exposed
on macOS / MoltenVK.

## Install

```bash
git clone https://github.com/groothipp/Groot-Rendering-Framework.git grf
cd grf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

Options:

| Option           | Default | Effect                                     |
|------------------|---------|--------------------------------------------|
| `GRF_DEBUG`      | `OFF`   | `-O0 -g3`, defines `GRF_DEBUG=1`           |
| `GRF_MAKE_TESTS` | `OFF`   | Build the Catch2 test suite                |

## Consume

```cmake
cmake_minimum_required(VERSION 4.3)
project(my_renderer LANGUAGES CXX)

find_package(grf REQUIRED)

add_executable(my_renderer main.cpp)
target_link_libraries(my_renderer PRIVATE grf::grf)
```

The install ships GRF's public headers under `include/grf/`, bundled glm
headers under `include/glm/`, and ImGui headers as `include/grf/imgui.h` and
`include/grf/imconfig.h`.

## Documentation

- [Getting Started](docs/getting-started.md)
- Concepts:
  [Architecture](docs/concepts/architecture.md),
  [Resources](docs/concepts/resources.md),
  [Shaders](docs/concepts/shaders.md),
  [Synchronization](docs/concepts/synchronization.md)
- [API Reference](docs/api/)
- Guides:
  [First triangle](docs/guides/first-triangle.md),
  [Texture upload](docs/guides/texture-upload.md),
  [Compute pass](docs/guides/compute-pass.md)

## Credits

GRF is by Matthew Hipp. If you build something with it and want to acknowledge
it, the wording I appreciate is:

> Built with **GRF — Groot Rendering Framework**
> &lt;https://github.com/groothipp/Groot-Rendering-Framework&gt;

Apache 2.0 only requires you keep the copyright notice and `LICENSE` text in
redistributions. Visible attribution is a request.

## Acknowledgements

| Library                       | Purpose                              | License      |
|-------------------------------|--------------------------------------|--------------|
| Vulkan-Hpp                    | C++ Vulkan bindings                  | Apache 2.0   |
| MoltenVK                      | Vulkan-on-Metal                      | Apache 2.0   |
| glfw                          | Window and input                     | zlib/libpng  |
| glm                           | Vector / matrix math                 | MIT          |
| Dear ImGui                    | Immediate-mode GUI                   | MIT          |
| VMA                           | GPU memory allocation                | MIT          |
| stb\_image                    | Image decoding                       | MIT / Public Domain |
| nlohmann/json                 | JSON (shader cache)                  | MIT          |
| shaderc / glslang / SPIRV-Tools | GLSL → SPIR-V                      | Apache 2.0 / BSD |
| Catch2                        | Test framework                       | BSL 1.0      |

Full third-party license info in [LICENSE](LICENSE).

## License

Apache License 2.0. See [LICENSE](LICENSE).
