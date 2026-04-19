#pragma once

#include <string>

namespace grf {

struct Settings {
  std::string                   windowTitle = "GRF Application";
  std::pair<uint32_t, uint32_t> windowSize = { 1280u, 720u };
  std::string                   applicationVersion = "1.0.0";
};

}