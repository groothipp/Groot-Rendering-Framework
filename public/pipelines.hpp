#pragma once

#include <memory>

namespace grf {

class Pipeline;

class ComputePipeline {
  friend class GRF;

  std::shared_ptr<Pipeline> m_impl;

public:
  bool valid() const;

private:
  explicit ComputePipeline(std::shared_ptr<Pipeline>);
};

}