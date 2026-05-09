#pragma once

#include "public/grf.hpp"
#include "public/math.hpp"

#include <random>

using namespace grf;

static const u32 g_maxParticleCount = 1000000;

class Particles {
  using RealDist = std::uniform_real_distribution<f32>;

public:
  struct Data {
    u64   posBufAddr;
    u64   velBufAddr;
    uvec2 screenDims;
    u32   particleCount;
    f32   particleRadius;
  };

private:
  GRF&              m_grf;
  Shader            m_vertShader;
  Shader            m_fragShader;
  GraphicsPipeline  m_pipeline;
  Ring<Buffer>      m_posRing;
  Ring<Buffer>      m_velRing;
  std::mt19937_64   m_rng = std::mt19937_64(std::random_device{}());

public:
  Particles(GRF&, const std::string&, u32);

  std::pair<Buffer&, Buffer&> buffers(u32);

  void render(CommandBuffer&, u32, Data);

  void reset(u32);
  u32 spawn(vec2, u32, u32);
};

class BoundsPass {
  struct TileData {
    u64 posBufAddr;
    u64 scratchBufAddr;
    u32 particleCount;
  };

  struct SceneData {
    u64 scratchBufAddr;
    u64 boundsBufAddr;
    u32 tileCount;
  };

  GRF&            m_grf;
  Shader          m_tileShader;
  Shader          m_sceneShader;
  ComputePipeline m_tilePipeline;
  ComputePipeline m_scenePipeline;
  Ring<Buffer>    m_boundsRing;

public:
  BoundsPass(GRF&, const std::string&);
  Buffer& boundsBuffer(u32);
  void dispatch(CommandBuffer&, u32, u32, u64);
};

class EncodePass {
public:
  struct Data {
    u64 posBufAddr;
    u64 boundsBufAddr;
    u64 mortonBufAddr;
    u64 indexBufAddr;
    u32 particleCount;
  };

private:
  Shader          m_encodeShader;
  ComputePipeline m_encodePipeline;
  Ring<Buffer>    m_mortonRing;
  Ring<Buffer>    m_indexRing;

public:
  EncodePass(GRF&, const std::string&);
  Buffer& mortonBuffer(u32);
  Buffer& indexBuffer(u32);
  void dispatch(CommandBuffer&, u32, Data);
};

class RadixSortPass {
  struct HistData {
    u64 keysBufAddr;
    u64 histBufAddr;
    u32 particleCount;
    u32 shift;
    u32 workgroups;
    u32 blocksPerWorkgroup;
  };

  struct ScatterData {
    u64 keysInAddr;
    u64 valsInAddr;
    u64 keysOutAddr;
    u64 valsOutAddr;
    u64 histAddr;
    u32 particleCount;
    u32 shift;
    u32 workgroups;
    u32 blocksPerWorkgroup;
  };

  static const u32 g_numWorkgroups = 256;
  static const u32 g_bins = 256;

  GRF&            m_grf;
  Shader          m_histShader;
  Shader          m_scatterShader;
  ComputePipeline m_histPipeline;
  ComputePipeline m_scatterPipeline;
  Ring<Buffer>    m_histRing;

public:
  RadixSortPass(GRF&, const std::string&);
  void dispatch(CommandBuffer&, u32, u32, u64, u64);
};

class BuildPass {
public:
  struct Data {
    u64 mortonBufAddr;
    u64 parentBufAddr;
    u64 childBufAddr;
    u32 particleCount;
  };

private:
  Shader          m_buildShader;
  ComputePipeline m_buildPipeline;
  Ring<Buffer>    m_parentRing;
  Ring<Buffer>    m_childRing;

public:
  BuildPass(GRF&, const std::string&);
  Buffer& parentBuffer(u32);
  Buffer& childBuffer(u32);
  void dispatch(CommandBuffer&, u32, u32, u64);
};

class LVBHTree {
  BoundsPass    m_boundsPass;
  EncodePass    m_encodePass;
  RadixSortPass m_radixSortPass;
  BuildPass     m_buildPass;

public:
  LVBHTree(GRF&, const std::string&);
  void construct(CommandBuffer&, u32, u32, u64);
};