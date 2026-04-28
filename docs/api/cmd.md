# `CommandBuffer`

Wrapper over `vk::CommandBuffer` plus per-buffer state (current pipeline,
profiler zone stack, last-use bookkeeping).

Default-constructible to a null handle. Obtain real ones from
`grf.createCmdRing(QueueType)`. The queue type is fixed at ring creation.

## Lifecycle

```cpp
void begin();
void end();
```

`begin` resets the underlying `VkCommandBuffer`, starts recording with
`eOneTimeSubmit`, binds the global descriptor set for both graphics and
compute pipeline points, clears the profiler zone stack, and reserves a
new timeline value for tracking resource last-use on this submission.

`end` finalizes recording. Required before `grf.submit(cmd, ...)`.

────────────────────────────────────────────────────────────

## Pipelines

```cpp
void bindPipeline(const ComputePipeline&);
void bindPipeline(const GraphicsPipeline&);
```

Binds the pipeline. Updates resource lifetime tracking.

```cpp
void bindIndexBuffer(const Buffer&, IndexFormat = IndexFormat::U32,
                      std::size_t offset = 0);
```

For indexed draws.

────────────────────────────────────────────────────────────

## Push constants

```cpp
template <typename T>
void push(const T& data, uint32_t offset = 0);    // T must be trivially copyable

void pushBytes(std::span<const std::byte>, uint32_t offset = 0);
```

`push(struct)` writes a single trivially-copyable value into the pipeline's
push constant block. The struct's layout must match the GSL `push { }`
block under `std430` rules.

Common gotcha: `glm::vec3` is 16-byte aligned with 4 bytes of padding
before the next field. Use `glm::vec4` for predictable packing.

Panics if `offset + sizeof(T)` exceeds the pipeline's push constant size.

────────────────────────────────────────────────────────────

## Profiling

```cpp
void beginProfile(const std::string& name);
void endProfile();
```

Wraps `vkCmdWriteTimestamp2` calls. Zones are stack-based — every
`beginProfile` must be matched by an `endProfile` (panics on mismatch).
Results are read back asynchronously and aggregated in the `Profiler`.

────────────────────────────────────────────────────────────

## Rendering

```cpp
void beginRendering(
  std::span<const ColorAttachment>     colors,
  const std::optional<DepthAttachment>& depth = std::nullopt
);
void endRendering();
```

Dynamic rendering. No render passes, no framebuffers.

```cpp
struct ColorAttachment {
  std::variant<Tex2D, Img2D, SwapchainImage> img;
  LoadOp                loadOp     = LoadOp::Load;
  StoreOp               storeOp    = StoreOp::Store;
  std::array<float, 4>  clearValue = { 0, 0, 0, 1 };
};

struct DepthAttachment {
  DepthImage  img;
  LoadOp      loadOp     = LoadOp::Clear;
  StoreOp     storeOp    = StoreOp::Store;
  float       clearValue = 1.0f;
};
```

Color attachments must be in `Layout::ColorAttachmentOptimal`. Depth in
`Layout::DepthAttachmentOptimal`. Transition before calling.

────────────────────────────────────────────────────────────

## Draws

```cpp
void draw(uint32_t vertexCount,
          uint32_t instanceCount = 1,
          uint32_t firstVertex   = 0,
          uint32_t firstInstance = 0);

void drawIndexed(uint32_t indexCount,
                 uint32_t instanceCount = 1,
                 uint32_t firstIndex    = 0,
                 int32_t  vertexOffset  = 0,
                 uint32_t firstInstance = 0);

void drawIndirect(const Buffer&, std::size_t offset,
                   uint32_t drawCount, uint32_t stride);

void drawIndexedIndirect(const Buffer&, std::size_t offset,
                          uint32_t drawCount, uint32_t stride);
```

Plain Vulkan draw semantics. Vertex data is pulled in-shader via buffer
references (push the buffer's BDA as a `uint64`); the framework does not
expose vertex input bindings.

────────────────────────────────────────────────────────────

## Compute

```cpp
void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
void dispatchIndirect(const Buffer&, std::size_t offset = 0);
```

────────────────────────────────────────────────────────────

## Image layout transitions

```cpp
void transition(const TransitionImage& img, Layout from, Layout to);
```

Where `TransitionImage` is `std::variant<Tex2D, Tex3D, Cubemap, Img2D,
Img3D, DepthImage, SwapchainImage>`.

Pipeline barrier with layout transition. Stages and access masks are
inferred from the layouts. `from` must match the image's currently-tracked
layout (the framework records this internally).

`Undefined → X` is the only transition that does not preserve contents.

```cpp
void release(const TransitionImage&, Layout from, Layout to, QueueType dstQueue);
void acquire(const TransitionImage&, Layout from, Layout to, QueueType srcQueue);
```

Queue ownership transfer. Required when the image moves between queue
families. No-op when the source and destination map to the same family.

────────────────────────────────────────────────────────────

## Memory barriers

```cpp
void barrier();
```

Global memory barrier. `eAllCommands → eAllCommands` with all access
flags. Pessimistic; drains the pipeline.

```cpp
void barrier(const Buffer&, BufferAccess from, BufferAccess to);
```

Buffer-scoped barrier with explicit access modes. See
[concepts/synchronization.md](../concepts/synchronization.md) for the
`BufferAccess` mapping.

────────────────────────────────────────────────────────────

## Copies and blits

```cpp
void copyBuffer(const Buffer& src, const Buffer& dst);
void copyBuffer(const Buffer& src, const Buffer& dst,
                 std::size_t size,
                 std::size_t srcOffset = 0,
                 std::size_t dstOffset = 0);
```

The single-arg form copies `min(src.size(), dst.size())` bytes from offset
0 to offset 0.

```cpp
void copyBufferToImage(const Buffer& src, const TransitionImage& dst);
void copyImageToBuffer(const TransitionImage& src, const Buffer& dst);
void copyImage(const TransitionImage& src, const TransitionImage& dst);
```

Layout transitions are the user's responsibility. Source needs
`TransferSrcOptimal` (or `General`); destination needs `TransferDstOptimal`
(or `General`).

```cpp
void blitImage(const TransitionImage& src, const TransitionImage& dst,
                Filter filter = Filter::Linear);
```

Source/destination format scaling with filtering. Use for downscaling
swapchain to a smaller buffer or vice versa.
