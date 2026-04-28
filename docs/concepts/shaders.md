# Shaders (GSL)

GSL (Groot Shading Language) is a thin DSL on top of GLSL 460. The framework
compiles `.gsl` files at runtime via `grf.compileShader`. Compiled SPIR-V is
cached on disk; unchanged sources skip recompilation.

## Injected preamble

The assembler prepends every shader with:

```glsl
#version 460

#extension GL_EXT_buffer_reference            : require
#extension GL_EXT_nonuniform_qualifier        : require
#extension GL_EXT_scalar_block_layout         : require
#extension GL_EXT_shader_image_load_formatted : require

layout(set = 0, binding = 0) uniform texture2D    grf_Tex2D[];
layout(set = 0, binding = 1) uniform texture3D    grf_Tex3D[];
layout(set = 0, binding = 2) uniform textureCube  grf_Cubemap[];
layout(set = 0, binding = 3) uniform image2D      grf_Img2D[];
layout(set = 0, binding = 4) uniform image3D      grf_Img3D[];
layout(set = 0, binding = 5) uniform sampler      grf_Sampler[];
```

Index by `<resource>.heapIndex()`, pushed in via push constants.

## Stages

`ShaderType` enum:

- `Vertex`
- `Fragment`
- `TessCtrl`
- `TessEval`
- `Compute`

Geometry / mesh / task shaders are not supported.

────────────────────────────────────────────────────────────

## Triangle example

```glsl
// triangle.vert.gsl
const vec2 positions[3] = vec2[3](
  vec2( 0.0, -0.5),
  vec2( 0.5,  0.5),
  vec2(-0.5,  0.5)
);

const vec3 colors[3] = vec3[3](
  vec3(1, 0, 0),
  vec3(0, 1, 0),
  vec3(0, 0, 1)
);

out vec3 vColor;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
  vColor      = colors[gl_VertexIndex];
}
```

```glsl
// triangle.frag.gsl
in vec3 vColor;
out vec4 color;

void main() {
  color = vec4(vColor, 1);
}
```

────────────────────────────────────────────────────────────

## `in` / `out`

Auto-located in declaration order: first `out` is `location = 0`, second is
`1`, etc. Same for `in`s. Do not write `layout(location = N)` yourself.

Vertex `out` order must match fragment `in` order — this is a SPIR-V
interface requirement, not a GSL one.

Interpolation qualifiers (`flat`, `smooth`, `noperspective`, `centroid`)
work as in GLSL:

```glsl
flat out int matID;
noperspective out vec2 screenUV;
```

────────────────────────────────────────────────────────────

## `push { ... };`

Push constant block. Field names become directly accessible in `main` — no
struct-member access:

```glsl
push {
  mat4   viewProj;
  uint   albedoTex;
  uint   sampler;
  vec3   lightDir;
  float  time;
};

void main() {
  vec3 L = normalize(lightDir);   // direct, not pushBlock.lightDir
  ...
}
```

C++ side:

```cpp
struct PushData {
  glm::mat4 viewProj;
  uint32_t  albedoTex;
  uint32_t  sampler;
  glm::vec3 lightDir;
  float     time;
};

cmd.push(PushData{ vp, tex.heapIndex(), s.heapIndex(), L, t });
```

The C++ struct must use `std430` layout. Common gotcha: `vec3` is 16-byte
aligned with 4 bytes of padding before the next field. Use `vec4` if you
want predictable packing.

The framework checks at runtime that you do not push more than the
pipeline's push constant size and panics if you do.

────────────────────────────────────────────────────────────

## `readonly buffer { ... } name;` / `writeonly buffer { ... } name;`

Storage buffers, accessed via buffer device address. `readonly` or
`writeonly` qualifier required.

```glsl
readonly buffer {
  vec4 positions[];
} verts;

writeonly buffer {
  uint outputs[];
} dst;

push {
  uint count;
};
```

The assembler:

1. Generates a `_GrfBuf_N` buffer reference type per block.
2. Adds a field of that type to the implicit push block, named after the
   `} name;` instance.
3. Lets you use `verts.positions[i]` and `dst.outputs[i]` like normal GLSL
   buffer fields.

C++ side: push the buffer's address. The assembled push block is laid out
**buffer references first (in declaration order), then user `push { }`
fields**:

```cpp
struct PushData {
  uint64_t verts;     // first buffer block
  uint64_t dst;       // second buffer block
  uint32_t count;     // user push field
};

cmd.push(PushData{ positions.address(), results.address(), count });
```

This ordering means the `uint64_t` buffer pointers always sit at 8-byte
aligned offsets without needing manual padding in the C++ struct, no matter
what types the user puts in `push { }`.

### Anonymous form

No instance name. Fields become globally accessible:

```glsl
readonly buffer {
  uint counts[];
};

void main() {
  uint c = counts[gl_GlobalInvocationID.x];
}
```

Use named blocks for multi-buffer shaders, anonymous for single-buffer.

────────────────────────────────────────────────────────────

## `thread_group [x, y, z];`

Compute workgroup size. All three dimensions required:

```glsl
thread_group [64, 1, 1];

readonly buffer { uint values[]; } src;
writeonly buffer { uint values[]; } dst;

push { uint count; };

void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= count) return;
  dst.values[i] = src.values[i];
}
```

Translates to `layout(local_size_x = X, local_size_y = Y, local_size_z = Z) in;`.

Using `thread_group` in a non-compute stage is a compile error.

────────────────────────────────────────────────────────────

## Sampling textures

```glsl
push {
  uint albedoTex;
  uint linearSampler;
};

in vec2 uv;
out vec4 color;

void main() {
  color = texture(sampler2D(grf_Tex2D[albedoTex],
                             grf_Sampler[linearSampler]), uv);
}
```

Three textures + one sampler = four heap slots, not three combined-image-samplers.

────────────────────────────────────────────────────────────

## Storage images

```glsl
push { uint outputImg; };
thread_group [8, 8, 1];

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  imageStore(grf_Img2D[outputImg], px, vec4(1, 0, 1, 1));
}
```

`GL_EXT_shader_image_load_formatted` is auto-enabled, so you do not
declare the image format. The actual format of the resource is used at
runtime.

`Img2D` and `Img3D` are also exposed in the sampled-image arrays — see
[Resources](resources.md) for which slot is which.

────────────────────────────────────────────────────────────

## Vertex pulling

No vertex input bindings. Pull from a buffer reference indexed by
`gl_VertexIndex`:

```glsl
struct Vertex { vec3 pos; vec3 normal; vec2 uv; };

readonly buffer { Vertex items[]; } verts;

push { mat4 mvp; };

out vec3 normal;
out vec2 uv;

void main() {
  Vertex v = verts.items[gl_VertexIndex];
  gl_Position = mvp * vec4(v.pos, 1);
  normal      = v.normal;
  uv          = v.uv;
}
```

C++ side, `Vertex` and the GSL `struct Vertex` must match exactly under
`std430` rules.

────────────────────────────────────────────────────────────

## Compilation

`grf.compileShader(stage, path)` runs:

1. Read `.gsl` file.
2. Lex.
3. Parse: convert `push`, `buffer`, `thread_group` to a structured AST;
   keep the rest verbatim.
4. Assemble: emit framework preamble, generated push block + buffer
   reference types, then the user code with `#line` directives mapping
   back to source.
5. shaderc compiles GLSL to SPIR-V.
6. Cache SPIR-V on disk (content-addressed). Subsequent loads skip 4–5.

Errors from glslang include the original `.gsl` filename and line number
(via the injected `#line` directives):

```
myshader.vert.gsl:14: 'normalixe' : no matching overloaded function found
```

────────────────────────────────────────────────────────────

## Plain GLSL features

Math, control flow, structs, arrays, built-in variables (`gl_VertexIndex`,
`gl_GlobalInvocationID`, ...), built-in functions (`normalize`, `dot`,
`texture`, ...), and preprocessor directives all work as in GLSL 460 with
the four extensions GSL enables.
