#include "public/tex.hpp"

#include "internal/resources/image.hpp"
#include "internal/log.hpp"

namespace grf {

Tex2D::Tex2D(std::weak_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Tex2D::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex2D");
  return { ptr->m_width, ptr->m_height };
}

std::size_t Tex2D::size() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid Tex2D");
  return ptr->m_size;
}

Format Tex2D::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex2D");
  return static_cast<Format>(ptr->m_format);
}

uint32_t Tex2D::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex2D");
  return ptr->m_heapIndexSampled;
}

bool Tex2D::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

void Tex2D::write(std::span<const std::byte>) {

}

void Tex2D::write(const std::string&) {

}

Tex3D::Tex3D(std::weak_ptr<Image> img) : m_img(img) {}

std::tuple<uint32_t, uint32_t, uint32_t> Tex3D::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex3D");
  return { ptr->m_width, ptr->m_height, ptr->m_depth };
}

std::size_t Tex3D::size() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid Tex3D");
  return ptr->m_size;
}

Format Tex3D::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex3D");
  return static_cast<Format>(ptr->m_format);
}

uint32_t Tex3D::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Tex3D");
  return ptr->m_heapIndexSampled;
}

bool Tex3D::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

void Tex3D::write(uint32_t index, std::span<const std::byte>) {

}

void Tex3D::write(uint32_t index, const std::string&) {

}

Cubemap::Cubemap(std::weak_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Cubemap::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Cubemap");
  return { ptr->m_width, ptr->m_height };
}

std::size_t Cubemap::size() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid Cubemap");
  return ptr->m_size;
}

Format Cubemap::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Cubemap");
  return static_cast<Format>(ptr->m_format);
}

uint32_t Cubemap::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Cubemap");
  return ptr->m_heapIndexSampled;
}

bool Cubemap::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

void Cubemap::write(CubeFace face, std::span<const std::byte>) {

}

void Cubemap::write(CubeFace face, const std::string&) {

}

}