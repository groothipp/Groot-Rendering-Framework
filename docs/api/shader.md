# `Shader`

Handle to a compiled SPIR-V shader. Default-constructible to a null state.

```cpp
class Shader {
public:
  Shader() = default;

  const ShaderType&  type()  const;
  const std::string& path()  const;
  bool               valid() const;
};
```

A default-constructed `Shader` has empty path and `valid() == false`. Real
shaders are obtained via `grf.compileShader(stage, path)`.

## Compilation

```cpp
Shader compileShader(ShaderType, const std::string& path);
```

Pipeline:

1. Read the `.gsl` file at `path`.
2. Lex / parse / assemble — see [concepts/shaders.md](../concepts/shaders.md#compilation).
3. shaderc compiles the resulting GLSL to SPIR-V.
4. SPIR-V is cached on disk under a content-addressed name.
5. Return a `Shader` handle referencing the SPIR-V.

Subsequent compiles of the same source skip steps 2–4 and load the cached
SPIR-V directly.

## Use

Pass the resulting handle to a pipeline factory:

```cpp
auto vs   = grf.compileShader(grf::ShaderType::Vertex,   "tri.vert.gsl");
auto fs   = grf.compileShader(grf::ShaderType::Fragment, "tri.frag.gsl");
auto pipe = grf.createGraphicsPipeline(vs, fs, /* settings */);

auto cs   = grf.compileShader(grf::ShaderType::Compute, "blur.gsl");
auto pipe = grf.createComputePipeline(cs);
```

The `Shader` handle can be dropped after pipeline creation — the pipeline
holds onto whatever it needs internally.
