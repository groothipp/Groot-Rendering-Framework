# Getting Started

## Prerequisites

- C++26 compiler (Clang 18+, GCC 14+, MSVC 19.40+)
- CMake 4.3+
- Vulkan SDK 1.4+ with `shaderc_combined`, `glslang`, `SPIRV-Tools`,
  `SPIRV-Tools-opt`
- glfw3
- glm
- Catch2 v3 (only if `-DGRF_MAKE_TESTS=ON`)

## Install

```bash
git clone https://github.com/groothipp/Groot-Rendering-Framework.git grf
cd grf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

Custom prefix:

```bash
cmake -S . -B build \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local \
  -DCMAKE_BUILD_TYPE=Release
```

The install ships:

- `libgrf.a` to `lib/`
- Public headers under `include/grf/`
- Bundled glm headers under `include/glm/`
- ImGui public headers as `include/grf/imgui.h` and `include/grf/imconfig.h`
- CMake package config under `lib/cmake/grf/`

## Options

| Option           | Default | Effect                                    |
|------------------|---------|-------------------------------------------|
| `GRF_DEBUG`      | `OFF`   | `-O0 -g3`, defines `GRF_DEBUG=1`          |
| `GRF_MAKE_TESTS` | `OFF`   | Build the Catch2 test suite               |

## Consume

```cmake
cmake_minimum_required(VERSION 4.3)
project(my_renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(grf REQUIRED)

add_executable(my_renderer src/main.cpp)
target_link_libraries(my_renderer PRIVATE grf::grf)
```

`find_package(grf)` brings in include paths, the C++26 requirement, the
public compile definition `GLM_FORCE_DEPTH_ZERO_TO_ONE` (Vulkan-correct
depth in `glm::perspective`), and the link to `libgrf.a`.

## Minimal program

Open a window, clear to a color every frame, present.

```cpp
#include <grf/grf.hpp>
#include <array>

int main() {
  grf::GRF grf(grf::Settings{
    .windowTitle = "Hello GRF",
    .windowSize  = { 1280, 720 },
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
    cmds[idx].transition(swap, grf::Layout::Undefined,
                                grf::Layout::ColorAttachmentOptimal);
    cmds[idx].beginRendering(std::array{
      grf::ColorAttachment{
        .img        = swap,
        .loadOp     = grf::LoadOp::Clear,
        .clearValue = { 0.05f, 0.07f, 0.10f, 1.0f },
      }
    });
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

Build and run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./my_renderer
```

To draw geometry, see [guides/first-triangle.md](guides/first-triangle.md).

## Common installation issues

**`find_package(grf)` fails.** Pass the install prefix:

```bash
cmake -DCMAKE_PREFIX_PATH=$HOME/.local ..
```

**`vulkan/vulkan.hpp` not found.** Vulkan SDK not installed or `VULKAN_SDK`
unset. On macOS install both the Khronos loader (`libvulkan`) and MoltenVK.

**Linker errors mentioning `glslang_*` or `SPIRV_*`.** SDK install is
incomplete. Need `glslang`, `SPIRV-Tools`, `SPIRV-Tools-opt`. The LunarG SDK
includes them; some package managers split them out
(`glslang-dev`, `spirv-tools` on Debian/Ubuntu).

**Black window or immediate crash.** Check Vulkan 1.3+ support (dynamic
rendering, sync2). MoltenVK 1.2.6+ on macOS.
