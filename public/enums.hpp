#pragma once

namespace grf {

enum class ShaderType {
  Vertex,
  Fragment,
  TessCtrl,
  TessEval,
  Compute
};

enum class BufferIntent {
  GPUOnly,
  SingleUpdate,
  FrequentUpdate,
  Readable
};

enum class Format {
  undefined           = 0,
  r8_unorm            = 9,
  r8_snorm            = 10,
  r8_uint             = 13,
  r8_sint             = 14,
  rg8_unorm           = 16,
  rg8_snorm           = 17,
  rg8_uint            = 20,
  rg8_sint            = 21,
  rgba8_unorm         = 37,
  rgba8_snorm         = 38,
  rgba8_uint          = 41,
  rgba8_sint          = 42,
  rgba8_srgb          = 43,
  bgra8_unorm         = 44,
  bgra8_srgb          = 50,
  r16_unorm           = 70,
  r16_snorm           = 71,
  r16_uint            = 74,
  r16_sint            = 75,
  r16_sfloat          = 76,
  rg16_unorm          = 77,
  rg16_snorm          = 78,
  rg16_uint           = 81,
  rg16_sint           = 82,
  rg16_sfloat         = 83,
  rgba16_unorm        = 91,
  rgba16_snorm        = 92,
  rgba16_uint         = 95,
  rgba16_sint         = 96,
  rgba16_sfloat       = 97,
  r32_uint            = 98,
  r32_sint            = 99,
  r32_sfloat          = 100,
  rg32_uint           = 101,
  rg32_sint           = 102,
  rg32_sfloat         = 103,
  rgb32_uint          = 104,
  rgb32_sint          = 105,
  rgb32_sfloat        = 106,
  rgba32_uint         = 107,
  rgba32_sint         = 108,
  rgba32_sfloat       = 109,
  bc1_rgb_unorm       = 131,
  bc1_rgb_srgb        = 132,
  bc1_rgba_unorm      = 133,
  bc1_rgba_srgb       = 134,
  bc2_unorm           = 135,
  bc2_srgb            = 136,
  bc3_unorm           = 137,
  bc3_srgb            = 138,
  bc4_unorm           = 139,
  bc4_snorm           = 140,
  bc5_unorm           = 141,
  bc5_snorm           = 142,
  bc6h_ufloat         = 143,
  bc6h_sfloat         = 144,
  bc7_unorm           = 145,
  bc7_srgb            = 146
};

enum class SampleMode {
  Repeat,
  MirroredRepeat,
  ClampToEdge,
  ClampToBorder
};

enum class Filter {
  Nearest,
  Linear
};

}