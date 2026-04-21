#pragma once

#include "./enums.hpp"

#include <string>

namespace grf {

struct Settings {
  std::string                   windowTitle = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize = { 1280u, 720u };
  std::string                   applicationVersion = "1.0.0";
};

struct SamplerSettings {
  Filter magFilter = Filter::Linear;
  Filter minFilter = Filter::Linear;
  SampleMode uMode = SampleMode::Repeat;
  SampleMode vMode = SampleMode::Repeat;
  SampleMode wMode = SampleMode::Repeat;
  bool anisotropicFiltering = true;
};

}