#pragma once

#include <memory>

namespace grf {

class SwapchainImage {
  friend class GRF;

  class Impl;
  std::shared_ptr<Impl> m_impl;

private:
  explicit SwapchainImage(std::shared_ptr<Impl>);
};

}