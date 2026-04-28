# Math

Numeric and vector aliases. The framework bundles glm and exposes its types
under shorter names. Including `<grf/math.hpp>` (or `<grf/grf.hpp>`) is
enough — no `find_package(glm)` needed in consumer projects.

## Numeric aliases (Rust-style)

| Alias    | Underlying            |
|----------|-----------------------|
| `i8`     | `std::int8_t`         |
| `i16`    | `std::int16_t`        |
| `i32`    | `std::int32_t`        |
| `i64`    | `std::int64_t`        |
| `u8`     | `std::uint8_t`        |
| `u16`    | `std::uint16_t`       |
| `u32`    | `std::uint32_t`       |
| `u64`    | `std::uint64_t`       |
| `isize`  | `std::ptrdiff_t`      |
| `usize`  | `std::size_t`         |
| `f32`    | `float`               |
| `f64`    | `double`              |

`int` and `unsigned int` are not aliased — `int` is a C++ keyword and
cannot be redeclared.

## Vector and matrix aliases

| Alias    | Underlying       |
|----------|------------------|
| `vec2`   | `glm::vec2`      |
| `vec3`   | `glm::vec3`      |
| `vec4`   | `glm::vec4`      |
| `ivec2`  | `glm::ivec2`     |
| `ivec3`  | `glm::ivec3`     |
| `ivec4`  | `glm::ivec4`     |
| `uvec2`  | `glm::uvec2`     |
| `uvec3`  | `glm::uvec3`     |
| `uvec4`  | `glm::uvec4`     |
| `mat2`   | `glm::mat2`      |
| `mat3`   | `glm::mat3`      |
| `mat4`   | `glm::mat4`      |
| `quat`   | `glm::quat`      |

Aliases are `using` declarations, not new types. All glm free functions
work on them unchanged:

```cpp
grf::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
grf::mat4 view = glm::lookAt(grf::vec3{0,0,5}, grf::vec3{0}, grf::vec3{0,1,0});
grf::vec3 n    = glm::normalize(grf::vec3{1,2,3});
grf::quat q    = glm::angleAxis(glm::radians(45.0f), grf::vec3{0,1,0});
grf::mat4 mvp  = proj * view * model;
```

## glm headers included

`<grf/math.hpp>` pulls in:

- `<glm/vec2.hpp>`, `<glm/vec3.hpp>`, `<glm/vec4.hpp>`
- `<glm/mat3x3.hpp>`, `<glm/mat4x4.hpp>`
- `<glm/common.hpp>` (min/max/clamp/mix)
- `<glm/geometric.hpp>` (dot/cross/normalize/length)
- `<glm/trigonometric.hpp>` (sin/cos/...)
- `<glm/exponential.hpp>` (sqrt/pow/...)
- `<glm/gtc/quaternion.hpp>`
- `<glm/gtc/matrix_transform.hpp>` (perspective/lookAt/translate/rotate/scale)

Other glm headers are available via direct include (`#include
<glm/gtx/...>`).

## `GLM_FORCE_DEPTH_ZERO_TO_ONE`

Defined as a public compile definition by `target_link_libraries(grf::grf)`.
Makes `glm::perspective` produce depth values in the Vulkan range `[0, 1]`
instead of OpenGL's `[-1, 1]`.

If you include glm directly in a TU that does *not* include any GRF
header, this define may not be set there. Either include via
`<grf/math.hpp>`, or set the define yourself in such TUs.
