#pragma once

#include <memory>

namespace grf {

class Fence {
  friend class GRF;

  class Impl;
  std::shared_ptr<Impl> m_impl;

private:
  explicit Fence(std::shared_ptr<Impl>);
};

class Semaphore {
  friend class GRF;

  class Impl;
  std::shared_ptr<Impl> m_impl;

private:
  explicit Semaphore(std::shared_ptr<Impl>);
};

}
