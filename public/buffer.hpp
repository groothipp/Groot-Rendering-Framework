#pragma once

#include "public/enums.hpp"

#include <memory>
#include <span>
#include <type_traits>

namespace grf {

class Buffer {
  friend class Allocator;
  friend class ResourceManager;
  friend struct BufferUpdateInfo;

  class Impl;
  std::weak_ptr<Impl> m_impl;

public:
  std::size_t size() const;
  BufferIntent intent() const;
  bool valid() const;
  uint64_t address() const;

  template <typename T>
  void write(const T& data, std::size_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>);
    scheduleWrite(std::as_bytes(std::span{&data, 1}), offset);
  }

  template <typename T>
  void write(std::span<const T> data, std::size_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>);
    scheduleWrite(std::as_bytes(data), offset);
  }

  template <typename T>
  T read(std::size_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>);

    T out{};
    retrieveData(std::as_writable_bytes(std::span{&out, 1}), offset);

    return out;
  }

  template <typename T>
  void read(std::span<T> out, std::size_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>);
    retrieveData(std::as_writable_bytes(out), offset);
  }

private:
  Buffer(std::weak_ptr<Impl>);
  void scheduleWrite(std::span<const std::byte>, std::size_t);
  void retrieveData(std::span<std::byte>, std::size_t);
};

}