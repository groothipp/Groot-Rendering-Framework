#pragma once

#include "./structs.hpp"

#include <memory>

namespace grf {

class GPU {
  class Impl;
  std::unique_ptr<Impl> m_impl;

public:
  explicit GPU(const Settings& settings = Settings{});
  ~GPU();

  void run();
};

}