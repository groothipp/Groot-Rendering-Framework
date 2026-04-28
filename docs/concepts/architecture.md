# Architecture

GRF is a thin layer over Vulkan. Five design decisions determine the shape of
everything else.

## 1. Single bindless descriptor heap

One descriptor set, allocated at startup. Every texture, sampler, storage
image, and (when supported) acceleration structure gets an integer slot in
the heap. Shaders index by slot:

```cpp
auto tex     = grf.createTex2D(grf::Format::rgba8_srgb, 256, 256);
uint32_t i   = tex.heapIndex();
cmd.push(struct { uint32_t albedo; }{ i });
```

```glsl
push { uint albedo; }
out vec4 color;
in vec2 uv;
void main() {
  color = texture(grf_Tex2D[albedo], uv);
}
```

No per-draw descriptor set updates. The pipeline layout is constant; only
push constant integers change.

## 2. Buffer device addresses everywhere

Buffers are addressed by 64-bit GPU pointers (`buf.address()`), not bound
through descriptor sets or vertex input. GSL handles the boilerplate:
declare a `buffer { } name;` block in the shader and the assembler generates
the buffer reference type and adds it to the implicit push block. You push
the address as a `uint64`.

```cpp
cmd.push(struct { uint64_t verts; }{ vbo.address() });
```

```glsl
readonly buffer { Vertex items[]; } verts;

void main() { Vertex v = verts.items[gl_VertexIndex]; ... }
```

See [Shaders (GSL)](shaders.md) for the full DSL. Same mechanism handles
vertex pulling, compute storage access, and (when supported) acceleration
structure inputs.

## 3. PIMPL handles, all value types

Every public type — `Buffer`, `Tex2D`, `Img2D`, `Cubemap`, `DepthImage`,
`Sampler`, `Fence`, `Semaphore`, `CommandBuffer`, `GraphicsPipeline`,
`ComputePipeline`, `Shader` — wraps `std::shared_ptr<Impl>`.

- Copy is one shared_ptr increment.
- Default constructor produces a null handle. Methods on a null handle crash.
- The implementation lives in `internal/`, hidden from consumers.

## 4. Vulkan 1.3 only

GRF requires:

- Dynamic rendering (no render passes, no framebuffers)
- Synchronization2 (no v1 access flags)
- Timeline semaphores (no per-resource binary fences)
- Descriptor indexing with `eUpdateAfterBind` and `ePartiallyBound`
- Buffer device address
- `hostQueryReset`

Legacy paths are not exposed. There is one way to do each thing.

## 5. Tracked resource lifetimes

Every resource Impl carries `m_lastUseValues[3]` — the last timeline value
at which the resource was used on each of the three queues (graphics,
compute, transfer). Command-recording methods bump these. When the last
shared_ptr to a handle drops, the Impl schedules a "grave" entry recording
the values; `endFrame()` destroys graves whose timelines have retired.

Drop a handle the moment you stop touching it on the CPU. The framework
keeps the GPU object alive until the GPU is done.

────────────────────────────────────────────────────────────

## Frame loop

```cpp
auto [idx, dt] = grf.beginFrame();
```

Polls events. Kicks off pending resource uploads. Resets the profiler slot
for this frame. Returns the rotation index `idx ∈ [0, flightFrames)` and the
previous frame's wall-clock duration as `float`.

```cpp
grf.waitFences({ fences[idx] });
grf.resetFences({ fences[idx] });
```

Wait for the GPU to finish its previous use of `cmds[idx]`. Required before
recording over it.

```cpp
auto swap = grf.nextSwapchainImage(acquired[idx]);
```

Acquire next swapchain image. `acquired[idx]` is signaled when the image is
ready.

```cpp
cmds[idx].begin();
// record
cmds[idx].end();

grf.submit(cmds[idx], { acquired[idx] }, { rendered[idx] }, fences[idx]);
grf.present(swap, { rendered[idx] });
grf.endFrame();
```

`submit` waits on `acquired`, signals `rendered` and `fences[idx]`. `present`
waits on `rendered`. `endFrame` rotates `frameIndex` and drains retired
graves.

────────────────────────────────────────────────────────────

## Layout

```
public/        Public headers. Shipped in the install.
internal/      Framework internals. Not shipped.
src/           Implementation.
external/      Bundled third-party (imgui, stb, nlohmann).
cmake/         CMake package config template.
tests/         Catch2 test suite.
docs/          This documentation.
```

Anything in `public/` is part of the ABI. Anything in `internal/` is not.
