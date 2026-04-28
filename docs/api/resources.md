# Resources — `Buffer`, `Tex*`, `Img*`, `Cubemap`, `DepthImage`, `Sampler`, `SwapchainImage`

All resource handles are PIMPL types wrapping `shared_ptr<Impl>`.
Default-constructible to a null state. Obtained from `GRF` factories.

For the model and access patterns see
[concepts/resources.md](../concepts/resources.md). This page is signature
reference.

────────────────────────────────────────────────────────────

## `Buffer`

```cpp
class Buffer {
public:
  Buffer() = default;

  std::size_t  size()    const;
  BufferIntent intent()  const;
  uint64_t     address() const;     // BDA pointer to push to shaders

  template <typename T>
    requires std::is_trivially_copyable_v<T>
          && (!std::ranges::contiguous_range<T>)
  void write(const T& data, std::size_t offset = 0);

  template <std::ranges::contiguous_range R>
    requires std::is_trivially_copyable_v<std::ranges::range_value_t<R>>
  void write(R&& data, std::size_t offset = 0);

  template <typename T>
    requires std::is_trivially_copyable_v<T>
  T read(std::size_t offset = 0);

  template <std::ranges::contiguous_range R>
    requires std::is_trivially_copyable_v<std::ranges::range_value_t<R>>
  void read(R&& out, std::size_t offset = 0);
};
```

### `write` overloads

Two overloads, disjoint via constraints:

- Single trivially-copyable value (excluding ranges).
- Any contiguous range of trivially-copyable elements (covers `vector`,
  `array`, raw arrays, `span`).

`std::array<T, N>` matches the range overload (writes the elements as
bytes, not the wrapping struct). Same byte content; the explicit range
match is the principled choice.

### `read` overloads

Same disjoint pair:

- `read<T>()` returns `T` by value. Picks `T` explicitly:
  `auto u = buf.read<Uniforms>();`
- `read(range)` fills the range. Picks the type from the argument.

Reading a non-`Readable` buffer logs a warning and returns zeros.

### `address`

Buffer device address, suitable to push to shaders as a `uint64`. Stable
for the buffer's lifetime.

────────────────────────────────────────────────────────────

## `Tex2D` / `Tex3D` / `Cubemap`

Sampled, read-only from shader's perspective.

```cpp
class Tex2D {
public:
  Tex2D() = default;

  std::pair<uint32_t, uint32_t> dims()       const;
  std::size_t                   size()       const;
  Format                        format()     const;
  uint32_t                      heapIndex()  const;     // grf_Tex2D[] slot

  void write(std::span<const std::byte> bytes, Layout finalLayout);
};

class Tex3D {
public:
  Tex3D() = default;

  std::tuple<uint32_t, uint32_t, uint32_t> dims() const;
  // ... same as Tex2D ...

  void write(uint32_t z, std::span<const std::byte> bytes, Layout finalLayout);
};

class Cubemap {
public:
  Cubemap() = default;

  std::pair<uint32_t, uint32_t> dims() const;
  // ... same as Tex2D ...

  void write(CubeFace face, std::span<const std::byte> bytes, Layout finalLayout);
};
```

`write` takes the final layout the texture should be in after upload (the
framework handles `Undefined → TransferDstOptimal → finalLayout`).

────────────────────────────────────────────────────────────

## `Img2D` / `Img3D`

Storage images, shaders can write.

```cpp
class Img2D {
public:
  Img2D() = default;

  std::pair<uint32_t, uint32_t> dims()             const;
  std::size_t                   size()             const;
  Format                        format()           const;
  uint32_t                      sampledHeapIndex() const;  // grf_Tex2D[] slot
  uint32_t                      storageHeapIndex() const;  // grf_Img2D[] slot

  void write(std::span<const std::byte>, Layout finalLayout);
};

class Img3D {
public:
  Img3D() = default;

  std::tuple<uint32_t, uint32_t, uint32_t> dims()             const;
  std::size_t                              size()             const;
  Format                                   format()           const;
  uint32_t                                 sampledHeapIndex() const;  // grf_Tex3D[] slot
  uint32_t                                 storageHeapIndex() const;  // grf_Img3D[] slot

  void write(uint32_t z, std::span<const std::byte>, Layout finalLayout);
};
```

`Img2D` and `Img3D` are bound to two heap slots:

- `sampledHeapIndex()` — slot in `grf_Tex2D[]` / `grf_Tex3D[]`. Use for
  sampling via `texture(sampler2D(grf_Tex2D[i], grf_Sampler[s]), uv)`.
  Image must be in `Layout::ShaderReadOptimal`.
- `storageHeapIndex()` — slot in `grf_Img2D[]` / `grf_Img3D[]`. Use for
  `imageLoad` / `imageStore`. Image must be in `Layout::General`.

Pushing the wrong index (storage to a sampled binding or vice versa) is a
validation error — the descriptor types do not match.

────────────────────────────────────────────────────────────

## `DepthImage`

```cpp
class DepthImage {
public:
  DepthImage() = default;

  std::pair<uint32_t, uint32_t> dims()      const;
  Format                        format()    const;
  uint32_t                      heapIndex() const;    // valid if sampled=true at create
};
```

No upload methods — depth contents are written by the GPU during rendering.

────────────────────────────────────────────────────────────

## `Sampler`

```cpp
class Sampler {
public:
  Sampler() = default;

  Filter      magFilter()             const;
  Filter      minFilter()             const;
  SampleMode  uMode()                 const;
  SampleMode  vMode()                 const;
  SampleMode  wMode()                 const;
  bool        anisotropicFiltering()  const;
  uint32_t    heapIndex()             const;    // grf_Sampler[] slot
};
```

────────────────────────────────────────────────────────────

## `SwapchainImage`

```cpp
class SwapchainImage {
public:
  SwapchainImage() = default;
  uint32_t heapIndex() const;
};
```

Returned by `grf.nextSwapchainImage(acquired)`. Owned by the swapchain;
not user-allocated. Usable as color attachment / copy source / copy
destination / blit source / blit destination. Not sampled, not stored to.
