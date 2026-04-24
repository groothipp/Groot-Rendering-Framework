#include "public/img.hpp"

#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/log.hpp"

namespace grf {

Img2D::Img2D(std::weak_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Img2D::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img2D");
  return { ptr->m_width, ptr->m_height };
}

std::size_t Img2D::size() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid Img2D");
  return ptr->m_size;
}

Format Img2D::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img2D");
  return static_cast<Format>(ptr->m_format);
}

uint32_t Img2D::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img2D");
  return ptr->m_heapIndexSampled;
}

bool Img2D::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

void Img2D::write(std::span<const std::byte> data, Layout layout) {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to write to invalid Img2D");

  ptr->m_resourceManager->writeImage(ImageWriteInfo{
    .img    = ptr,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout)
  });
}

Img3D::Img3D(std::weak_ptr<Image> img) : m_img(img) {}

std::tuple<uint32_t, uint32_t, uint32_t> Img3D::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img3D");
  return { ptr->m_width, ptr->m_height, ptr->m_depth };
}

std::size_t Img3D::size() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid Img3D");
  return ptr->m_size;
}

Format Img3D::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img3D");
  return static_cast<Format>(ptr->m_format);
}

uint32_t Img3D::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid Img3D");
  return ptr->m_heapIndexSampled;
}

bool Img3D::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

void Img3D::write(uint32_t depth, std::span<const std::byte> data, Layout layout) {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to write to invalid Img3D");

  if (depth > ptr->m_depth)
    GRF_PANIC("Tried to write to invalid depth of Img3D");

  if (data.size() > ptr->m_size)
    GRF_PANIC("Out of bounds Img3D write. Byte size should be less than or equal to the allocation size");

  ptr->m_resourceManager->writeImage(ImageWriteInfo{
    .img    = ptr,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout),
    .depth  = static_cast<int32_t>(depth)
  });
}

}