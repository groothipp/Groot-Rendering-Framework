# GRF Documentation

## New here

Read in order:

1. [Getting Started](getting-started.md) — install, set up CMake, render a frame.
2. [Architecture](concepts/architecture.md) — five design decisions everything
   else follows from.
3. [Resources](concepts/resources.md) — buffers, images, samplers.
4. [Shaders (GSL)](concepts/shaders.md) — the shader DSL.
5. [Synchronization](concepts/synchronization.md) — fences, semaphores,
   barriers, lifetime tracking.

## Tasks

- [First triangle](guides/first-triangle.md)
- [Loading a texture](guides/texture-upload.md)
- [Compute pass feeding graphics](guides/compute-pass.md)

## Reference

| Page                                 | Covers                                  |
|--------------------------------------|-----------------------------------------|
| [api/grf.md](api/grf.md)             | `GRF` — top-level class, factories, frame loop |
| [api/cmd.md](api/cmd.md)             | `CommandBuffer` — every recording method |
| [api/resources.md](api/resources.md) | Buffer, Tex, Img, Cubemap, DepthImage, Sampler, SwapchainImage |
| [api/pipelines.md](api/pipelines.md) | Graphics + compute pipeline construction |
| [api/sync.md](api/sync.md)           | Fence, Semaphore                        |
| [api/shader.md](api/shader.md)       | Shader handle and compileShader          |
| [api/profiler.md](api/profiler.md)   | GPU-timestamp zone profiling             |
| [api/gui.md](api/gui.md)             | ImGui lifecycle and integration          |
| [api/input.md](api/input.md)         | Keyboard / mouse input                  |
| [api/math.md](api/math.md)           | Numeric, vector, matrix aliases         |
| [api/ring.md](api/ring.md)           | `Ring<T>` helper                         |
| [api/types.md](api/types.md)         | Enums (Format, Layout, ...)             |

## Conventions

- Code is real and compileable unless explicitly marked.
- `grf::` prefix shown in code, omitted in prose.
- "Handle" means a public type wrapping `shared_ptr<Impl>` (Buffer, Tex2D,
  GraphicsPipeline, Fence, ...).
- "Bindless" means resource lookup by integer index from a single global
  descriptor set.
- `.gsl` files are GSL source, compiled at runtime by `grf.compileShader`.

## Versioning

Semantic versioning. Pre-1.0 has no API stability guarantee. Post-1.0:

- Major bump → public API may break.
- Minor bump → new API, no breaks.
- Patch bump → bug fixes only.

Current version is in `CMakeLists.txt` (`project(... VERSION x.y.z)`).
