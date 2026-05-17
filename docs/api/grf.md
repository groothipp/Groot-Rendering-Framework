# `GRF`

Top-level class. Owns the Vulkan instance, device, swapchain, descriptor heap,
allocator, resource manager, profiler, GUI, input subsystem.

## Construction

```cpp
explicit GRF(const Settings& settings = Settings{});
```

Constructs the framework. By the time the constructor returns: window is
open, instance + device created, swapchain built, descriptor heap allocated,
pipeline layout created, ImGui initialized.

`Settings`:

```cpp
struct Settings {
  std::string                   windowTitle         = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize          = { 1280u, 720u };
  std::string                   applicationVersion  = "1.0.0";
  uint32_t                      flightFrames        = 2;
  Format                        swapchainFormat     = Format::bgra8_srgb;
  PresentMode                   presentMode         = PresentMode::VSync;
  std::size_t                   uploadBytesPerFrame = 64ull * 1024ull * 1024ull;
};
```

`flightFrames` controls how many frames the GPU can have in flight at once.
Most ring sizes (`createCmdRing`, `createSyncRing`) match this. Setting
higher than 3 is unusual.

`uploadBytesPerFrame` caps how much data the resource manager pushes to the
GPU per `beginFrame` (default 64 MB). Larger upload queues are time-sliced
across multiple frames. Items larger than this budget fall back to one-shot
staging allocations. See [concepts/resources.md](../concepts/resources.md#uploads).

────────────────────────────────────────────────────────────

## Frame loop

```cpp
bool running(std::function<bool()> endCond = []{ return false; }) const;
```

Returns `false` when the window is closing or `endCond()` returns `true`.

```cpp
std::pair<uint32_t, float> beginFrame();
```

- Polls window/input events.
- Kicks pending resource uploads.
- Resets profiler slot for this frame.
- Returns `{ frameIndex % flightFrames, previousFrameDurationSeconds }`.

```cpp
void endFrame();
```

Rotates `frameIndex`. Drains retired resource graves.

```cpp
void waitForResourceUpdates();
```

Block until all queued buffer/image uploads have completed on the GPU. In a
typical render loop you don't need this — every `submit` auto-injects a GPU
wait on pending uploads. Call directly for synchronous-init paths or before
CPU readback (`buf.read<T>()`).

────────────────────────────────────────────────────────────

## Window dimensions and resize

```cpp
std::pair<uint32_t, uint32_t> screenDims() const;
```

Returns the window's **framebuffer pixel** dimensions — what the swapchain
is sized at, what depth attachments need to match, what shaders should
treat as the rendering resolution. On Retina / high-DPI displays this is
*not* the same as the logical window size: a 1280×720 logical window on a
2x display reports `{ 2560, 1440 }` here. Use this for any GPU resource
sizing; use `glfwGetWindowSize` directly (via `<GLFW/glfw3.h>`) if you need
logical window units for non-GPU purposes.

```cpp
void resizeCallback(std::function<void(uint32_t, uint32_t)> callback);
```

Register a callback fired whenever the swapchain is recreated (window resize,
DPI change, etc.). The arguments are the new framebuffer dimensions, matching
`screenDims()`. The callback does **not** fire on initial construction —
query `screenDims()` once at startup to handle initial-state sizing yourself.

────────────────────────────────────────────────────────────

## Subsystem accessors

```cpp
Input&    input();
GUI&      gui();
Profiler& profiler();
```

References to internal subsystem objects. Lifetime is tied to the `GRF`
instance.

────────────────────────────────────────────────────────────

## Swapchain

```cpp
SwapchainImage nextSwapchainImage();
```

Acquires the next swapchain image. The framework cycles through an internal
pool of binary acquire semaphores; the returned `SwapchainImage` exposes
its current acquire `Sync` via `image.sync()`. Pass that to the first
submit that touches the image.

```cpp
void present(const SwapchainImage&, std::span<const Sync> waits = {});
```

Queue the image for the compositor. `waits` should include the `Sync`
returned by your render submission so present doesn't happen until
rendering completes. Internally the framework translates the timeline-based
`Sync`s into the binary semaphore that `vkQueuePresentKHR` requires.

────────────────────────────────────────────────────────────

## Submission and waits

```cpp
Sync submit(
  const CommandBuffer&,
  std::span<const Sync> waits = {}
);
```

Submits the command buffer to its target queue (determined by the queue
type the cmd ring was created with). Waits on `waits` (plus any pending
resource uploads, auto-injected) before starting. Returns a `Sync`
representing the moment this work completes.

```cpp
void wait(const Sync&);
void wait(std::span<const Sync>);
```

CPU host wait on one or more `Sync`s. Blocks the calling thread until the
GPU work each refers to has finished. Default-invalid `Sync`s are skipped
(important for first-frame flight-ring slots).

```cpp
Ring<Sync> createSyncRing();
```

Returns a `Ring<Sync>` with `flightFrames` default-constructed (invalid)
slots. Standard flight pattern: assign each frame's terminal `Sync` into
the slot; `wait` on the slot at the top of the next iteration.

See [api/sync.md](sync.md) and [concepts/synchronization.md](../concepts/synchronization.md).

────────────────────────────────────────────────────────────

## Shaders

```cpp
Shader compileShader(ShaderType, const std::string& path);
```

Reads the GSL file at `path`, compiles to SPIR-V via shaderc, caches the
result on disk. Subsequent calls with unchanged source skip compilation.

────────────────────────────────────────────────────────────

## Resource factories

```cpp
Buffer    createBuffer(BufferIntent, std::size_t bytes);
Tex2D     createTex2D(Format, uint32_t w, uint32_t h);
Tex3D     createTex3D(Format, uint32_t w, uint32_t h, uint32_t d);
Cubemap   createCubemap(Format, uint32_t w, uint32_t h);
Img2D     createImg2D(Format, uint32_t w, uint32_t h);
Img3D     createImg3D(Format, uint32_t w, uint32_t h, uint32_t d);
DepthImage createDepthImage(Format, uint32_t w, uint32_t h, bool sampled = false);
Sampler   createSampler(const SamplerSettings&);
```

All return owning handles. Drop the handle to destroy (deferred until the
GPU is done — see [synchronization](../concepts/synchronization.md)).

`createDepthImage` with `sampled=true` adds the depth image to
`grf_Tex2D[]` so it can be sampled from shaders.

`Format` covers BC1–BC7, R/RG/RGBA at 8/16/32 bits, depth/stencil. Full
list in [api/types.md](types.md).

────────────────────────────────────────────────────────────

## Resource rings

```cpp
Ring<Buffer> createBufferRing(BufferIntent, std::size_t bytes);
Ring<Img2D>  createImg2DRing(Format, uint32_t w, uint32_t h);
Ring<Img3D>  createImg3DRing(Format, uint32_t w, uint32_t h, uint32_t d);
```

Allocate `flightFrames` copies of the resource. Index by frame index for
per-frame uniforms or per-frame compute outputs.

────────────────────────────────────────────────────────────

## Pipelines

```cpp
ComputePipeline  createComputePipeline(Shader);
GraphicsPipeline createGraphicsPipeline(Shader vertex, Shader fragment,
                                         const GraphicsPipelineSettings&);
```

See [api/pipelines.md](pipelines.md) for `GraphicsPipelineSettings`.

────────────────────────────────────────────────────────────

## Command buffers

```cpp
Ring<CommandBuffer> createCmdRing(QueueType);
```

Allocate `flightFrames` command buffers backed by a queue-type-specific
pool. Index by frame index to record per-frame.

────────────────────────────────────────────────────────────

## Free functions

```cpp
std::future<ImageData> readImage(const std::string& path);
```

Decodes an image file via stb_image on a worker thread. Returns a
`std::future<ImageData>` — call `.get()` when you need the pixel data.

The returned `format` field is chosen by the file's bit depth *and* channel
count:

| Source bit depth | Source channels | Returned `format`   | Bytes / pixel |
|------------------|-----------------|---------------------|---------------|
| 8-bit (PNG/JPG)  | 1               | `r8_unorm`          | 1             |
| 8-bit            | 2, 3, 4         | `rgba8_unorm`       | 4             |
| 16-bit PNG       | 1               | `r16_unorm`         | 2             |
| 16-bit           | 2, 3, 4         | `rgba16_unorm`      | 8             |
| HDR (`.hdr`)     | 1               | `r32_sfloat`        | 4             |
| HDR              | 2, 3, 4         | `rgba32_sfloat`     | 16            |

Single-channel detection avoids the old "load grayscale-as-RGBA" 4× memory
waste — common for PBR roughness / AO / metalness / height maps. The
shader can sample `.r` from a single-channel texture; reading `.gba` will
return `(0, 0, 1)` per Vulkan's component swizzle rules, so make sure your
shader is channel-aware.

Pass `img.format` directly to `createTex2D` to match the decoded data, or
pick a different format (e.g. `rgba8_srgb` for sRGB-encoded color textures)
at the cost of needing the bit depth and channel count to line up.
