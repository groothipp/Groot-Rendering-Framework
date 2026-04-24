#pragma once

#include "./enums.hpp"

#include <memory>
#include <span>

namespace grf {

class Image;

class Tex2D {
  friend class GRF;

  std::weak_ptr<Image> m_img;

public:
  std::pair<uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  bool valid() const;

  void write(const std::string&);
  void write(std::span<const std::byte>);

private:
  explicit Tex2D(std::weak_ptr<Image>);
};

class Tex3D {
  friend class GRF;

  std::weak_ptr<Image> m_img;

public:
  std::tuple<uint32_t, uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  bool valid() const;

  void write(uint32_t, const std::string&);
  void write(uint32_t, std::span<const std::byte>);

private:
  explicit Tex3D(std::weak_ptr<Image>);
};

class Cubemap {
  friend class GRF;

  std::weak_ptr<Image> m_img;

public:
  std::pair<uint32_t, uint32_t> dims() const;
  std::size_t size() const;
  Format format() const;
  uint32_t heapIndex() const;
  bool valid() const;

  void write(CubeFace, const std::string&);
  void write(CubeFace, std::span<const std::byte>);

private:
  explicit Cubemap(std::weak_ptr<Image>);
};

}