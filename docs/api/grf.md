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
  std::string                   windowTitle        = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize         = { 1280u, 720u };
  std::string                   applicationVersion = "1.0.0";
  uint32_t                      flightFrames       = 2;
  Format                        swapchainFormat    = Format::bgra8_srgb;
  PresentMode                   presentMode        = PresentMode::VSync;
};
```

`flightFrames` controls how many frames the GPU can have in flight at once.
Most ring sizes (`createCmdRing`, `createFenceRing`) match this. Setting
higher than 3 is unusual.

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

Block until all queued buffer/image uploads have completed on the GPU.
Already called internally by `beginFrame`; only call directly if you need
the data ready before submitting subsequent commands.

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
SwapchainImage nextSwapchainImage(const Semaphore& signalOnAcquire);
```

Acquires the next swapchain image. `signalOnAcquire` is signaled by the
swapchain when the image is ready. Pass it as a wait to `submit`.

```cpp
void present(const SwapchainImage&, std::span<const Semaphore> waits = {});
```

Queue the image for the compositor. `waits` should include the semaphore
your render submission signaled, so present does not happen until rendering
completes.

────────────────────────────────────────────────────────────

## Submission

```cpp
void submit(
  const CommandBuffer&,
  std::span<const Semaphore> waits   = {},
  std::span<const Semaphore> signals = {},
  std::optional<Fence>       signalFence = std::nullopt
);
```

Submits the command buffer to its target queue (determined by the queue
type the cmd ring was created with). Waits on `waits` before starting,
signals `signals` and `signalFence` on completion.

────────────────────────────────────────────────────────────

## Fences and semaphores

```cpp
Fence     createFence(bool signaled = false);
Semaphore createSemaphore();

Ring<Fence>     createFenceRing(bool signaled = false);
Ring<Semaphore> createSemaphoreRing();
```

`createSemaphoreRing` sizes to `max(flightFrames, swapchainImageCount)` —
larger than other rings to avoid swapchain semaphore reuse hazards.

```cpp
void waitFences(const std::vector<Fence>&);
void resetFences(const std::vector<Fence>&);
```

Block until all listed fences are signaled / reset them to unsignaled.

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
ImageData readImage(const std::string& path);
```

Decodes an image file (PNG, JPG, etc.) via stb_image. Returns
`ImageData{ bytes, width, height }` for uploading to a texture via
`tex.write(...)`.
