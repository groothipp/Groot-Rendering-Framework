#pragma once

#include "./enums.hpp"
#include "./structs.hpp"
#include "./shader.hpp"
#include "./buffer.hpp"
#include "./tex.hpp"
#include "./img.hpp"
#include "./sampler.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace grf {

class GRF {
  class Impl;
  std::unique_ptr<Impl> m_impl;

public:
  explicit GRF(const Settings& settings = Settings{});
  ~GRF();

  void run(std::function<void(double)> main = [](double){});
  void beginResourceUpdates();
  void waitForResourceUpdates();

  Shader compileShader(ShaderType, const std::string&);

  Buffer createBuffer(BufferIntent, std::size_t);

  Tex2D createTex2D(Format, uint32_t, uint32_t);
  Tex3D createTex3D(Format, uint32_t, uint32_t, uint32_t);
  Cubemap createCubemap(Format, uint32_t, uint32_t);
  Img2D createImg2D(Format, uint32_t, uint32_t);
  Img3D createImg3D(Format, uint32_t, uint32_t, uint32_t);
  Sampler createSampler(const SamplerSettings&);
};

ImageData readImage(const std::string&);

}