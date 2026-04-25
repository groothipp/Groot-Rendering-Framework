#pragma once

#include "./types.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace grf {

class Image;

class Buffer {
  friend class Allocator;
  friend class ResourceManager;
  friend struct BufferUpdateInfo;

  class Impl;
  std::shared_ptr<Impl> m_impl;

public:

  std::size_t size() const;
  BufferIntent intent() const;
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
  Buffer(std::shared_ptr<Impl>);
  void scheduleWrite(std::span<const std::byte>, std::size_t);
  void retrieveData(std::span<std::byte>, std::size_t);
};

class Img2D {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::pair<uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  void write(std::span<const std::byte>, Layout);

private:
  explicit Img2D(std::shared_ptr<Image>);
};

class Img3D {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::tuple<uint32_t, uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  void write(uint32_t, std::span<const std::byte>, Layout);

private:
  explicit Img3D(std::shared_ptr<Image>);
};

class Tex2D {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::pair<uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  void write(std::span<const std::byte>, Layout);

private:
  explicit Tex2D(std::shared_ptr<Image>);
};

class Tex3D {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::tuple<uint32_t, uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  void write(uint32_t, std::span<const std::byte>, Layout);

private:
  explicit Tex3D(std::shared_ptr<Image>);
};

class Cubemap {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::pair<uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  void write(CubeFace, std::span<const std::byte>, Layout);

private:
  explicit Cubemap(std::shared_ptr<Image>);
};

class DepthImage {
  friend class GRF;

  std::shared_ptr<Image> m_img;

public:

  std::pair<uint32_t, uint32_t> dims() const;
  Format format() const;
  uint32_t heapIndex() const;

private:
  explicit DepthImage(std::shared_ptr<Image>);
};

class Sampler {
  friend class Allocator;
  friend class GRF;
  friend class DescriptorHeap;

  class Impl;
  std::shared_ptr<Impl> m_impl;

public:

  Filter magFilter() const;
  Filter minFilter() const;
  SampleMode uMode() const;
  SampleMode vMode() const;
  SampleMode wMode() const;
  bool anisotropicFiltering() const;
  uint32_t heapIndex() const;

private:
  explicit Sampler(std::shared_ptr<Impl>);
};

class SwapchainImage {
  friend class GRF;

  class Impl;
  std::shared_ptr<Impl> m_impl;

public:
  uint32_t heapIndex() const;

private:
  explicit SwapchainImage(std::shared_ptr<Impl>);
};

}
