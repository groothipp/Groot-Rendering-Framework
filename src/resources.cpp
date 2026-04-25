#include "public/resources.hpp"
#include "public/types.hpp"

#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/resources/sampler.hpp"
#include "internal/log.hpp"

namespace grf {

Buffer::Buffer(std::weak_ptr<Buffer::Impl> impl) : m_impl(impl) {}

std::size_t Buffer::size() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access size of invalid buffer");
  return ptr->m_size;
}

BufferIntent Buffer::intent() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access intent of invalid buffer");
  return ptr->m_intent;
}

bool Buffer::valid() const {
  auto ptr = m_impl.lock();
  return ptr != nullptr;
}

uint64_t Buffer::address() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access address of invalid buffer");
  return ptr->m_address;
}

void Buffer::scheduleWrite(std::span<const std::byte> data, std::size_t offset) {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to schedule buffer write for invalid buffer");

  if (offset + data.size() > ptr->m_size)
    GRF_PANIC("Out of bounds buffer write. Offset + size should be less than or equal to the allocation size");

  ptr->m_resourceManager->writeBuffer(BufferUpdateInfo{
    .buf    = ptr,
    .data   = std::vector<std::byte>(data.begin(), data.end()),
    .offset = offset
  });
}

void Buffer::retrieveData(std::span<std::byte> data, std::size_t offset) {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
      GRF_PANIC("Tried to retrieve data from invalid buffer");

  if (ptr->m_intent != BufferIntent::Readable) {
    log::warning("Attempted to read from a buffer that was not made with the readable intent");
    return;
  }

  if (offset + data.size() > ptr->m_size)
    GRF_PANIC("Out of bounds buffer read. Offset + size should be less than or equal to the allocation size");

  ptr->m_resourceManager->readBuffer(ptr->m_address, data, offset);
}

Buffer::Impl::Impl(
  std::unique_ptr<ResourceManager>& resourceManager, VmaAllocation alloc,
  vk::Buffer buffer, vk::DeviceAddress addr, vk::DeviceSize size, BufferIntent intent
)
: m_allocation(alloc),
  m_buffer(buffer),
  m_address(addr),
  m_size(size),
  m_intent(intent),
  m_resourceManager(resourceManager)
{}

Image::Image(std::unique_ptr<ResourceManager>& resourceManager, const ImageInfo& info)
: m_resourceManager(resourceManager),
  m_id(info.id),
  m_allocation(info.alloc),
  m_image(info.image),
  m_format(info.format),
  m_size(info.size),
  m_width(info.width),
  m_height(info.height),
  m_depth(info.depth)
{}

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

void Tex2D::write(std::span<const std::byte> data, Layout layout) {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to write to invalid Tex2D");

  ptr->m_resourceManager->writeImage(ImageWriteInfo{
    .img    = ptr,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout)
  });
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

void Tex3D::write(uint32_t depth, std::span<const std::byte> data, Layout layout) {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to write to invalid Tex3D");

  ptr->m_resourceManager->writeImage(ImageWriteInfo{
    .img    = ptr,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout),
    .depth  = static_cast<int32_t>(depth)
  });
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

void Cubemap::write(CubeFace face, std::span<const std::byte> data, Layout layout) {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to write to invalid Cubemap");

  ptr->m_resourceManager->writeImage(ImageWriteInfo{
    .img        = ptr,
    .data       = data,
    .layout     = static_cast<vk::ImageLayout>(layout),
    .face       = face,
    .isCubemap  = true
  });
}

DepthImage::DepthImage(std::weak_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> DepthImage::dims() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access dimensions of invalid DepthImage");
  return { ptr->m_width, ptr->m_height };
}

Format DepthImage::format() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access format of invalid DepthImage");
  return static_cast<Format>(ptr->m_format);
}

uint32_t DepthImage::heapIndex() const {
  auto ptr = m_img.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access heap index of invalid DepthImage");
  return ptr->m_heapIndexSampled;
}

bool DepthImage::valid() const {
  auto ptr = m_img.lock();
  return ptr != nullptr;
}

Sampler::Sampler(std::weak_ptr<Sampler::Impl> impl) : m_impl(impl) {}

Filter Sampler::magFilter() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access mag filter on invalid sampler");
  return static_cast<Filter>(ptr->m_magFilter);
}

Filter Sampler::minFilter() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access min filter on invalid sampler");
  return static_cast<Filter>(ptr->m_minFilter);
}

SampleMode Sampler::uMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access u sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_uMode);
}

SampleMode Sampler::vMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access v sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_vMode);
}

SampleMode Sampler::wMode() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access w sample mode on invalid sampler");
  return static_cast<SampleMode>(ptr->m_wMode);
}

bool Sampler::anisotropicFiltering() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access anisotropic filtering on invalid sampler");
  return ptr->m_anisotropicFiltering;
}

uint32_t Sampler::heapIndex() const {
  auto ptr = m_impl.lock();
  if (ptr == nullptr)
    GRF_PANIC("Tried to access heap index on invalid sampler");
  return ptr->m_index;
}

bool Sampler::valid() const {
  auto ptr = m_impl.lock();
  return ptr != nullptr;
}

Sampler::Impl::Impl(uint64_t id, vk::Sampler sampler, const SamplerSettings& settings)
: m_id(id), m_sampler(sampler) {
  m_magFilter = static_cast<vk::Filter>(settings.magFilter);
  m_minFilter = static_cast<vk::Filter>(settings.minFilter);
  m_uMode = static_cast<vk::SamplerAddressMode>(settings.uMode);
  m_vMode = static_cast<vk::SamplerAddressMode>(settings.vMode);
  m_wMode = static_cast<vk::SamplerAddressMode>(settings.wMode);
  m_anisotropicFiltering = settings.anisotropicFiltering;
}

}
