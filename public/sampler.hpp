#pragma once

#include "./enums.hpp"

#include <memory>

namespace grf {

class Sampler {
  friend class Allocator;
  friend class GRF;
  friend class DescriptorHeap;

  class Impl;
  std::weak_ptr<Impl> m_impl;

public:
  Filter magFilter() const;
  Filter minFilter() const;
  SampleMode uMode() const;
  SampleMode vMode() const;
  SampleMode wMode() const;
  bool anisotropicFiltering() const;
  uint32_t heapIndex() const;
  bool valid() const;

private:
  explicit Sampler(std::weak_ptr<Impl>);
};

}