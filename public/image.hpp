#pragma once

#include "./enums.hpp"

#include <memory>

namespace grf {

class Image {
  friend class Allocator;
  friend class GPU;
  friend class DescriptorHeap;

  class Impl;
  std::weak_ptr<Impl> m_impl;

public:
  Format format() const;
  std::tuple<uint32_t, uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  uint32_t heapIndex() const;
  bool valid() const;

private:
  explicit Image(std::weak_ptr<Impl>);
};

class Sampler {
  friend class Allocator;
  friend class GPU;
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

using Tex2D = Image;
using Tex3D = Image;
using Cubemap = Image;
using Img2D = Image;
using Img3D = Image;

}