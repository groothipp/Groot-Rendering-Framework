# Resources

Resources are the GPU-backed objects you create and use: buffers, four
flavors of image, samplers. This page describes the model. For per-method
reference see [api/resources.md](../api/resources.md).

## Access summary

| Resource         | Memory                       | Shader access            |
|------------------|------------------------------|--------------------------|
| `Buffer`         | Per `BufferIntent`           | BDA pointer (`buf.address()`) |
| `Tex2D`/`Tex3D`  | Device-local                 | `grf_Tex2D[]`/`grf_Tex3D[]` slot |
| `Cubemap`        | Device-local                 | `grf_Cubemap[]` slot (sampled) + `grf_CubemapStorage[]` slot (image2DArray, for compute writes) |
| `Img2D`/`Img3D`  | Device-local                 | `grf_Img2D[]`/`grf_Img3D[]` slot (also exposed as sampled) |
| `DepthImage`     | Device-local                 | Optional `grf_Tex2D[]` slot if `sampled=true` |
| `Sampler`        | (none)                       | `grf_Sampler[]` slot     |
| `SwapchainImage` | Driver-controlled            | Color attachment / copy / blit only |

────────────────────────────────────────────────────────────

## `Buffer`

Linear chunk of GPU memory.

```cpp
auto buf = grf.createBuffer(grf::BufferIntent::GPUOnly,
                             sizeof(MyVertex) * 1024);
```

### `BufferIntent`

| Intent           | Memory       | CPU writes        | Use for                                |
|------------------|--------------|-------------------|----------------------------------------|
| `GPUOnly`        | Device-local | Via staging       | Vertex data, large constants           |
| `SingleUpdate`   | Device-local | Via staging       | One-shot uploads                       |
| `FrequentUpdate` | UMA-aware    | Direct (mapped)   | Per-frame uniforms, particle state     |
| `Readable`       | Host-cached  | (read only)       | GPU → CPU readback                     |

`GPUOnly` and `SingleUpdate`: `buf.write(data)` queues a staging copy that
runs during the next `waitForResourceUpdates` (which `beginFrame` triggers).

`FrequentUpdate`: `buf.write(data)` writes directly into mapped memory.
Requires hardware with device-local + host-visible memory (UMA, integrated,
Apple Silicon, most modern discrete GPUs).

`Readable`: `buf.read<T>()` and `buf.read(span)` retrieve GPU writes.
Reading from a non-`Readable` buffer logs and returns zeros.

### Write

```cpp
buf.write(myStruct);                  // single value
buf.write(myVector);                  // contiguous range
buf.write(myArray);                   // ditto
buf.write(myStruct, /*offset=*/64);   // both take optional byte offset
```

The two overloads (single-value vs contiguous range) are picked via concept
constraints. Non-trivially-copyable types fail to compile.

### Read

```cpp
auto u = buf.read<MyUniforms>();        // single value, returned by value
buf.read(myVector);                     // fills the range
buf.read(std::span<MyType>{ ptr, n });  // explicit span
```

### Buffer device address

Every buffer has a 64-bit GPU address:

```cpp
struct PushData { uint64_t verts; };
cmd.push(PushData{ vbo.address() });
```

Shader-side use is in [Shaders (GSL)](shaders.md).

────────────────────────────────────────────────────────────

## Texture types (`Tex2D`, `Tex3D`, `Cubemap`)

Sampled in shaders. `Tex2D` and `Tex3D` are read-only from the shader's
perspective; `Cubemap` also has a storage view so compute shaders can
populate it via `imageStore(grf_CubemapStorage[i], ivec3(x, y, face), c)` —
the common path for baking an equirect HDR into a cubemap and for IBL
prefilter passes. Device-local memory. Initial layout `Undefined`.

```cpp
auto albedo = grf.createTex2D(grf::Format::rgba8_srgb, 1024, 1024);
auto noise  = grf.createTex3D(grf::Format::r8_unorm, 64, 64, 64);
auto sky    = grf.createCubemap(grf::Format::rgba16_sfloat, 512, 512);
```

### Upload

```cpp
albedo.write(pixels, grf::Layout::ShaderReadOptimal);

for (auto face : { CubeFace::Right, CubeFace::Left, ... })
  sky.write(face, faceData[face], grf::Layout::ShaderReadOptimal);

for (uint32_t z = 0; z < depth; ++z)
  noise.write(z, sliceData[z], grf::Layout::ShaderReadOptimal);
```

See [Uploads](#uploads) below for how the staging pipeline actually runs.

### Sample

```glsl
push { uint albedo; uint linearSampler; }

vec4 c = texture(sampler2D(grf_Tex2D[albedo],
                             grf_Sampler[linearSampler]), uv);
```

`sampler3D(...)` for `Tex3D`, `samplerCube(...)` for `Cubemap`.

────────────────────────────────────────────────────────────

## Storage images (`Img2D`, `Img3D`)

Same shape as `Tex2D`/`Tex3D` plus storage usage — shaders can read AND
write.

```cpp
auto target = grf.createImg2D(grf::Format::rgba8_unorm, 1920, 1080);
```

```glsl
push { uint target; }
thread_group [8, 8, 1];

void main() {
  imageStore(grf_Img2D[target],
             ivec2(gl_GlobalInvocationID.xy),
             vec4(...));
}
```

`Img2D`/`Img3D` get *two* heap slots: `storageHeapIndex()` returns the
slot in `grf_Img2D[]`/`grf_Img3D[]` for storage access (layout `General`).
`sampledHeapIndex()` returns the slot in `grf_Tex2D[]`/`grf_Tex3D[]` for
sampled access (layout `ShaderReadOptimal`). Push whichever index matches
how the shader will use the image; pushing the wrong one is a validation
error.

────────────────────────────────────────────────────────────

## `DepthImage`

```cpp
auto depth = grf.createDepthImage(grf::Format::d32_sfloat, 1920, 1080,
                                   /*sampled=*/false);
```

Formats: `d16_unorm`, `d32_sfloat`, `d24_unorm_s8_uint`.

If `sampled=true`, the depth image is added to `grf_Tex2D[]` and can be
sampled from shaders (e.g., screen-space effects reading the depth buffer).

────────────────────────────────────────────────────────────

## `SwapchainImage`

Returned by `grf.nextSwapchainImage()`. Owned by the swapchain. Usable as
color attachment, copy/blit source, copy/blit destination. Not sampled,
not stored to.

`SwapchainImage::sync()` returns the acquire `Sync` that gates rendering
into the image — pass it to the first submit that touches it. See
[api/resources.md#swapchainimage](../api/resources.md#swapchainimage).

────────────────────────────────────────────────────────────

## `Sampler`

Independent of textures. Combined at the shader use site, not at allocation.

```cpp
auto sampler = grf.createSampler(grf::SamplerSettings{
  .magFilter            = grf::Filter::Linear,
  .minFilter            = grf::Filter::Linear,
  .uMode                = grf::SampleMode::Repeat,
  .vMode                = grf::SampleMode::Repeat,
  .wMode                = grf::SampleMode::Repeat,
  .anisotropicFiltering = true,
});

uint32_t slot = sampler.heapIndex();
```

A handful of samplers serves all your textures; you do not need one per
texture.

────────────────────────────────────────────────────────────

## Image layouts

| `Layout`                 | Used when                                   |
|--------------------------|---------------------------------------------|
| `Undefined`              | Initial state. Contents are garbage.        |
| `General`                | Storage image read/write.                   |
| `ColorAttachmentOptimal` | Color attachment write.                     |
| `ShaderReadOptimal`      | Sampled from shader.                        |
| `TransferSrcOptimal`     | Copy/blit source.                           |
| `TransferDstOptimal`     | Copy/blit destination.                      |
| `DepthAttachmentOptimal` | Depth attachment write.                     |
| `PresentSrc`             | Awaiting present.                           |

`cmd.transition(img, from, to)` moves between layouts. The framework tracks
the current layout per image internally; you must specify the correct
`from` at every transition.

`Undefined → X` is the only transition that does not preserve contents.

────────────────────────────────────────────────────────────

## Uploads

When you call `buf.write(data)` (for `GPUOnly`/`SingleUpdate` buffers) or
`tex.write(...)` / `img.write(...)`, the framework queues an upload to its
internal transfer queue. The pipeline:

1. **Schedule.** `write` copies your data into an internal queue entry and
   returns immediately. CPU not blocked.
2. **Batch (`beginFrame`).** At each frame's start, the resource manager
   drains up to `Settings::uploadBytesPerFrame` bytes (default 64 MB) from
   the queue into the next transfer submission. Leftover entries stay
   queued for subsequent frames — this is the **time-slicing** mechanism
   that keeps any single frame's transfer work bounded.
3. **Stage.** A worker thread allocates from the persistent staging ring
   (a single large `HOST_VISIBLE | SEQUENTIAL_WRITE | MAPPED` buffer
   created at startup), `memcpy`s the user data into the ring at the next
   free offset, records `vkCmdCopyBufferToImage` / `vkCmdCopyBuffer`,
   submits to the transfer queue.
4. **Auto-wait at `submit`.** When you call `grf.submit(cmd, waits)`, the
   framework auto-injects a wait on the pending transfer timeline value.
   The GPU work runs in parallel with your CPU recording until the actual
   submit, at which point the timeline ensures GPU ordering.
5. **Drain (next `beginFrame`).** When the previous frame's transfer
   completes (queried via the transfer timeline), the staging ring head
   resets and the cmd buffer is recycled.

Items larger than `uploadBytesPerFrame` get a one-shot staging buffer
allocated outside the ring (fallback path); the ring handles the common
case of many small-to-medium uploads.

The host-visible `FrequentUpdate` intent bypasses staging entirely —
`buf.write(data)` writes directly into mapped memory.

### Tuning

Bump `Settings::uploadBytesPerFrame` if you have a small number of large
textures and want them resident faster (cost: more memory pressure +
longer per-frame transfer work). Reduce it if you have many small uploads
per frame and want smoother first-frame timing.

────────────────────────────────────────────────────────────

## Lifetime

Every handle is a `shared_ptr<Impl>`. Dropping the last reference does not
destroy the GPU object immediately:

1. `Impl` destructor schedules a "grave" entry in the resource manager,
   recording the resource's last-use timeline values per queue.
2. Each `endFrame()` checks which graves have all their last-use values
   retired (GPU caught up on every queue) and destroys those.

You can drop a handle the moment you stop touching it on the CPU. The
framework guarantees the GPU object stays alive until the GPU is done.
