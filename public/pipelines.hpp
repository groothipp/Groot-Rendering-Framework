#pragma once

#include <memory>

namespace grf {

class Pipeline;

class ComputePipeline {
  friend class GRF;
  friend class CommandBuffer;

  std::shared_ptr<Pipeline> m_impl;

public:
  ComputePipeline() = default;

private:
  explicit ComputePipeline(std::shared_ptr<Pipeline>);
};

class GraphicsPipeline {
  friend class GRF;
  friend class CommandBuffer;

  std::shared_ptr<Pipeline> m_impl;

public:
  GraphicsPipeline() = default;

private:
  explicit GraphicsPipeline(std::shared_ptr<Pipeline>);
};

}