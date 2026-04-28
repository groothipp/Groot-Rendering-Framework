#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
  bc7_srgb            = 146,
  d16_unorm           = 124,
  d32_sfloat          = 126,
  d24_unorm_s8_uint   = 129
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

enum class CubeFace {
  Right,
  Left,
  Top,
  Bottom,
  Back,
  Front
};

enum class QueueType {
  Graphics,
  Compute,
  Transfer
};

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

enum class BufferAccess {
  ShaderRead,
  ShaderWrite,
  TransferRead,
  TransferWrite,
  IndirectRead,
  IndexRead
};

enum class PresentMode {
  VSync,
  Mailbox,
  Immediate
};

enum class Topology {
  PointList     = 0,
  LineList      = 1,
  LineStrip     = 2,
  TriangleList  = 3,
  TriangleStrip = 4,
  TriangleFan   = 5
};

enum class PolygonMode {
  Fill  = 0,
  Line  = 1,
  Point = 2
};

enum class CullMode {
  None          = 0,
  Front         = 1,
  Back          = 2,
  FrontAndBack  = 3
};

enum class FrontFace {
  CounterClockwise  = 0,
  Clockwise         = 1
};

enum class CompareOp {
  Never           = 0,
  Less            = 1,
  Equal           = 2,
  LessOrEqual     = 3,
  Greater         = 4,
  NotEqual        = 5,
  GreaterOrEqual  = 6,
  Always          = 7
};

enum class BlendFactor {
  Zero                  = 0,
  One                   = 1,
  SrcColor              = 2,
  OneMinusSrcColor      = 3,
  DstColor              = 4,
  OneMinusDstColor      = 5,
  SrcAlpha              = 6,
  OneMinusSrcAlpha      = 7,
  DstAlpha              = 8,
  OneMinusDstAlpha      = 9,
  ConstantColor         = 10,
  OneMinusConstantColor = 11,
  ConstantAlpha         = 12,
  OneMinusConstantAlpha = 13,
  SrcAlphaSaturate      = 14
};

enum class BlendOp {
  Add             = 0,
  Subtract        = 1,
  ReverseSubtract = 2,
  Min             = 3,
  Max             = 4
};

enum class SampleCount {
  e1  = 1,
  e2  = 2,
  e4  = 4,
  e8  = 8,
  e16 = 16
};

enum class IndexFormat {
  U16 = 0,
  U32 = 1
};

enum class LoadOp {
  Load      = 0,
  Clear     = 1,
  DontCare  = 2
};

enum class StoreOp {
  Store     = 0,
  DontCare  = 1
};

struct Settings {
  std::string                   windowTitle = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize = { 1280u, 720u };
  std::string                   applicationVersion = "1.0.0";
  uint32_t                      flightFrames = 2;
  Format                        swapchainFormat = Format::bgra8_srgb;
  PresentMode                   presentMode = PresentMode::VSync;
};

struct SamplerSettings {
  Filter magFilter = Filter::Linear;
  Filter minFilter = Filter::Linear;
  SampleMode uMode = SampleMode::Repeat;
  SampleMode vMode = SampleMode::Repeat;
  SampleMode wMode = SampleMode::Repeat;
  bool anisotropicFiltering = true;
};

struct ImageData {
  std::vector<std::byte> bytes;
  uint32_t               width;
  uint32_t               height;
};

struct BlendState {
  bool        enable          = false;
  BlendFactor srcColorFactor  = BlendFactor::One;
  BlendFactor dstColorFactor  = BlendFactor::Zero;
  BlendOp     colorOp         = BlendOp::Add;
  BlendFactor srcAlphaFactor  = BlendFactor::One;
  BlendFactor dstAlphaFactor  = BlendFactor::Zero;
  BlendOp     alphaOp         = BlendOp::Add;
};

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

}
