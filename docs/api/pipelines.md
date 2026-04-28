# Pipelines

Two pipeline types: `ComputePipeline` and `GraphicsPipeline`. Both are thin
PIMPL handles wrapping a shared internal `Pipeline` impl.

```cpp
class ComputePipeline {
public:
  ComputePipeline() = default;
};

class GraphicsPipeline {
public:
  GraphicsPipeline() = default;
};
```

Default constructors produce null handles. Methods on a null pipeline
crash. Get real pipelines from `grf.createComputePipeline(...)` /
`grf.createGraphicsPipeline(...)`.

────────────────────────────────────────────────────────────

## `createComputePipeline`

```cpp
ComputePipeline createComputePipeline(Shader);
```

Builds a compute pipeline from a single shader of `ShaderType::Compute`.
The shader must declare a `thread_group [x, y, z];` workgroup size.

────────────────────────────────────────────────────────────

## `createGraphicsPipeline`

```cpp
GraphicsPipeline createGraphicsPipeline(
  Shader vertex,
  Shader fragment,
  const GraphicsPipelineSettings&
);
```

```cpp
struct GraphicsPipelineSettings {
  std::vector<Format>     colorFormats;
  Format                  depthFormat     = Format::undefined;
  Topology                topology        = Topology::TriangleList;
  PolygonMode             polygonMode     = PolygonMode::Fill;
  CullMode                cullMode        = CullMode::Back;
  FrontFace               frontFace       = FrontFace::CounterClockwise;
  bool                    depthTest       = false;
  bool                    depthWrite      = false;
  CompareOp               depthCompareOp  = CompareOp::Less;
  std::vector<BlendState> blends;
  SampleCount             sampleCount     = SampleCount::e1;
};
```

### `colorFormats`

The format of each color attachment the pipeline will be used with.
Required. Order matches `beginRendering`'s color attachment order.

### `depthFormat`

`Format::undefined` if the pipeline does not write depth. Otherwise the
format of the depth attachment.

### `topology`

`PointList`, `LineList`, `LineStrip`, `TriangleList`, `TriangleStrip`,
`TriangleFan`. Matches Vulkan / GLSL.

### `polygonMode`

`Fill`, `Line`, `Point`. `Line` and `Point` require the
`fillModeNonSolid` device feature (the framework does not enable this by
default; `Fill` is what works on all hardware).

### `cullMode` / `frontFace`

`CullMode::None` for both-sided rendering; `Front`/`Back`/`FrontAndBack`
for culling. `FrontFace::CounterClockwise` is the GL-style convention
(also Vulkan's default with `GLM_FORCE_DEPTH_ZERO_TO_ONE`).

### Depth

```cpp
.depthFormat    = Format::d32_sfloat,
.depthTest      = true,
.depthWrite     = true,
.depthCompareOp = CompareOp::Less,
```

`depthTest=false` skips depth comparison entirely. `depthWrite` only
matters when depth testing is on.

### `blends`

One `BlendState` per color attachment, or empty for default opaque blending
on all attachments.

```cpp
struct BlendState {
  bool        enable          = false;
  BlendFactor srcColorFactor  = BlendFactor::One;
  BlendFactor dstColorFactor  = BlendFactor::Zero;
  BlendOp     colorOp         = BlendOp::Add;
  BlendFactor srcAlphaFactor  = BlendFactor::One;
  BlendFactor dstAlphaFactor  = BlendFactor::Zero;
  BlendOp     alphaOp         = BlendOp::Add;
};
```

Standard alpha blending:

```cpp
BlendState{
  .enable         = true,
  .srcColorFactor = BlendFactor::SrcAlpha,
  .dstColorFactor = BlendFactor::OneMinusSrcAlpha,
  .srcAlphaFactor = BlendFactor::One,
  .dstAlphaFactor = BlendFactor::OneMinusSrcAlpha,
}
```

### `sampleCount`

`e1` / `e2` / `e4` / `e8` / `e16` for MSAA. Must match the sample count
of the attachments the pipeline renders to.

────────────────────────────────────────────────────────────

## Examples

### Triangle

```cpp
auto vs   = grf.compileShader(grf::ShaderType::Vertex,   "tri.vert.gsl");
auto fs   = grf.compileShader(grf::ShaderType::Fragment, "tri.frag.gsl");
auto pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
  .colorFormats = { grf::Format::bgra8_srgb },
  .cullMode     = grf::CullMode::None,
});
```

### Mesh with depth

```cpp
auto pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
  .colorFormats   = { grf::Format::bgra8_srgb },
  .depthFormat    = grf::Format::d32_sfloat,
  .cullMode       = grf::CullMode::Back,
  .depthTest      = true,
  .depthWrite     = true,
  .depthCompareOp = grf::CompareOp::Less,
});
```

### Compute

```cpp
auto cs   = grf.compileShader(grf::ShaderType::Compute, "blur.gsl");
auto pipe = grf.createComputePipeline(cs);

cmd.bindPipeline(pipe);
cmd.push(MyPush{ ... });
cmd.dispatch(width / 8, height / 8, 1);
```
