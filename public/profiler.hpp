#pragma once

#include <memory>

namespace grf {

class Profiler {
  friend class GRF;

public:
  class Impl;

  ~Profiler();
  Profiler(const Profiler&)            = delete;
  Profiler& operator=(const Profiler&) = delete;
  Profiler(Profiler&&)                 = delete;
  Profiler& operator=(Profiler&&)      = delete;

  void render();

private:
  std::unique_ptr<Impl> m_impl;
  explicit Profiler(std::unique_ptr<Impl>);
};

}
