#pragma once

#include "public/grf.hpp"
#include "public/math.hpp"

#include <random>

using namespace grf;

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
  static const u32  g_maxParticleCount = 1000000;

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
public:
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

private:
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

class LVBHTree {
  BoundsPass m_boundsPass;

public:
  LVBHTree(GRF&, const std::string&);
  void construct(CommandBuffer&, u32, u32, u64);
};