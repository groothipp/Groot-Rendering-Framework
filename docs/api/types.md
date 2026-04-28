# Types and enums

Reference for the public POD types and enums in `<grf/types.hpp>`.

## `ShaderType`

```cpp
enum class ShaderType { Vertex, Fragment, TessCtrl, TessEval, Compute };
```

Stage selector for `grf.compileShader(stage, path)`. No geometry/mesh/task
support.

## `BufferIntent`

```cpp
enum class BufferIntent { GPUOnly, SingleUpdate, FrequentUpdate, Readable };
```

See [concepts/resources.md](../concepts/resources.md) for the memory and
write semantics of each.

## `Format`

Vulkan format enum, matching `VkFormat` numeric values:

| Variant            | Formats                                                         |
|--------------------|-----------------------------------------------------------------|
| `r8_*`             | `unorm`, `snorm`, `uint`, `sint`                                |
| `rg8_*`            | `unorm`, `snorm`, `uint`, `sint`                                |
| `rgba8_*`          | `unorm`, `snorm`, `uint`, `sint`, `srgb`                        |
| `bgra8_*`          | `unorm`, `srgb`                                                 |
| `r16_*`            | `unorm`, `snorm`, `uint`, `sint`, `sfloat`                      |
| `rg16_*`           | `unorm`, `snorm`, `uint`, `sint`, `sfloat`                      |
| `rgba16_*`         | `unorm`, `snorm`, `uint`, `sint`, `sfloat`                      |
| `r32_*`            | `uint`, `sint`, `sfloat`                                        |
| `rg32_*`           | `uint`, `sint`, `sfloat`                                        |
| `rgb32_*`          | `uint`, `sint`, `sfloat`                                        |
| `rgba32_*`         | `uint`, `sint`, `sfloat`                                        |
| `bc1_rgb_*`        | `unorm`, `srgb`                                                 |
| `bc1_rgba_*`       | `unorm`, `srgb`                                                 |
| `bc2_*`            | `unorm`, `srgb`                                                 |
| `bc3_*`            | `unorm`, `srgb`                                                 |
| `bc4_*`            | `unorm`, `snorm`                                                |
| `bc5_*`            | `unorm`, `snorm`                                                |
| `bc6h_*`           | `ufloat`, `sfloat`                                              |
| `bc7_*`            | `unorm`, `srgb`                                                 |
| Depth              | `d16_unorm`, `d32_sfloat`, `d24_unorm_s8_uint`                  |

`Format::undefined` (= 0) means "no format" — used in
`GraphicsPipelineSettings::depthFormat` to indicate "no depth attachment."

## `SampleMode`

```cpp
enum class SampleMode { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };
```

UV/UVW wrap mode. Each axis configurable independently.

## `Filter`

```cpp
enum class Filter { Nearest, Linear };
```

Mag/min filter. Used in `SamplerSettings` and `cmd.blitImage`.

## `CubeFace`

```cpp
enum class CubeFace { Right, Left, Top, Bottom, Back, Front };
```

For `Cubemap::write(face, data, layout)`. Order matches Vulkan's standard
+X/−X/+Y/−Y/+Z/−Z layer convention.

## `QueueType`

```cpp
enum class QueueType { Graphics, Compute, Transfer };
```

For `createCmdRing(QueueType)` and queue-ownership transfers
(`cmd.release` / `cmd.acquire`). On hardware with a unified queue family,
all three resolve to the same family.

## `Layout`

```cpp
enum class Layout {
  Undefined               = 0,
  General                 = 1,
  ColorAttachmentOptimal  = 2,
  ShaderReadOptimal       = 5,
  TransferSrcOptimal      = 6,
  TransferDstOptimal      = 7,
  DepthAttachmentOptimal  = 1000241000,
  PresentSrc              = 1000001002
};
```

Image layouts. Numeric values match `VkImageLayout`. See
[concepts/resources.md](../concepts/resources.md#image-layouts).

## `BufferAccess`

```cpp
enum class BufferAccess {
  ShaderRead, ShaderWrite,
  TransferRead, TransferWrite,
  IndirectRead, IndexRead
};
```

For `cmd.barrier(buf, from, to)`. See
[concepts/synchronization.md](../concepts/synchronization.md#cmdbarrierbuf-bufferaccess-from-bufferaccess-to)
for the stage/access mapping.

## `PresentMode`

```cpp
enum class PresentMode { VSync, Mailbox, Immediate };
```

Set in `Settings::presentMode`. `VSync` is FIFO (always available).
`Mailbox` and `Immediate` may not be supported by all drivers.

## `Topology`

```cpp
enum class Topology { PointList, LineList, LineStrip, TriangleList,
                      TriangleStrip, TriangleFan };
```

## `PolygonMode`

```cpp
enum class PolygonMode { Fill, Line, Point };
```

`Line` and `Point` require `fillModeNonSolid` device feature, which the
framework does not enable.

## `CullMode` / `FrontFace`

```cpp
enum class CullMode  { None, Front, Back, FrontAndBack };
enum class FrontFace { CounterClockwise, Clockwise };
```

## `CompareOp`

```cpp
enum class CompareOp {
  Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always
};
```

For depth testing. Standard Vulkan / GL semantics.

## `BlendFactor` / `BlendOp`

```cpp
enum class BlendFactor {
  Zero, One,
  SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
  SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha,
  ConstantColor, OneMinusConstantColor, ConstantAlpha, OneMinusConstantAlpha,
  SrcAlphaSaturate
};

enum class BlendOp { Add, Subtract, ReverseSubtract, Min, Max };
```

## `SampleCount`

```cpp
enum class SampleCount { e1 = 1, e2 = 2, e4 = 4, e8 = 8, e16 = 16 };
```

For MSAA. Must match the actual sample count of attachments rendered to.

## `IndexFormat`

```cpp
enum class IndexFormat { U16, U32 };
```

For `cmd.bindIndexBuffer(buf, format, offset)`.

## `LoadOp` / `StoreOp`

```cpp
enum class LoadOp  { Load, Clear, DontCare };
enum class StoreOp { Store, DontCare };
```

For `ColorAttachment` and `DepthAttachment`. `DontCare` lets the driver
skip the load/store if it can.

## POD structs

### `Settings`

```cpp
struct Settings {
  std::string                   windowTitle        = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize         = { 1280, 720 };
  std::string                   applicationVersion = "1.0.0";
  uint32_t                      flightFrames       = 2;
  Format                        swapchainFormat    = Format::bgra8_srgb;
  PresentMode                   presentMode        = PresentMode::VSync;
};
```

### `SamplerSettings`

```cpp
struct SamplerSettings {
  Filter      magFilter            = Filter::Linear;
  Filter      minFilter            = Filter::Linear;
  SampleMode  uMode                = SampleMode::Repeat;
  SampleMode  vMode                = SampleMode::Repeat;
  SampleMode  wMode                = SampleMode::Repeat;
  bool        anisotropicFiltering = true;
};
```

### `ImageData`

```cpp
struct ImageData {
  std::vector<std::byte> bytes;
  uint32_t               width;
  uint32_t               height;
};
```

Returned by `grf::readImage(path)`. Pass `bytes` to `Tex2D::write`.

### `BlendState` / `GraphicsPipelineSettings`

See [api/pipelines.md](pipelines.md).
