#pragma once

#include "public/enums.hpp"
#include <memory>

namespace grf {

class Buffer {
  friend class Allocator;

  class Impl;
  std::weak_ptr<Impl> m_impl;

public:
  std::size_t size() const;
  BufferIntent intent() const;
  bool valid() const;
  uint64_t address() const;

private:
  Buffer(std::weak_ptr<Impl>);
};

}