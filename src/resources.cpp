#include "public/resources.hpp"
#include "public/types.hpp"

#include "internal/resources/buffer.hpp"
#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/resources/sampler.hpp"
#include "internal/log.hpp"

namespace grf {

Buffer::Buffer(std::shared_ptr<Buffer::Impl> impl) : m_impl(impl) {}

std::size_t Buffer::size() const {
  return m_impl->m_size;
}

BufferIntent Buffer::intent() const {
  return m_impl->m_intent;
}

uint64_t Buffer::address() const {
  return m_impl->m_address;
}

void Buffer::scheduleWrite(std::span<const std::byte> data, std::size_t offset) {
  if (offset + data.size() > m_impl->m_size)
    GRF_PANIC("Out of bounds buffer write. Offset + size should be less than or equal to the allocation size");

  auto rm = m_impl->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeBuffer(BufferUpdateInfo{
    .buf    = m_impl,
    .data   = std::vector<std::byte>(data.begin(), data.end()),
    .offset = offset
  });
}

void Buffer::retrieveData(std::span<std::byte> data, std::size_t offset) {
  if (m_impl->m_intent != BufferIntent::Readable) {
    log::warning("Attempted to read from a buffer that was not made with the readable intent");
    return;
  }

  if (offset + data.size() > m_impl->m_size)
    GRF_PANIC("Out of bounds buffer read. Offset + size should be less than or equal to the allocation size");

  auto rm = m_impl->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->readBuffer(m_impl->m_address, data, offset);
}

Buffer::Impl::Impl(
  std::weak_ptr<ResourceManager> resourceManager, VmaAllocation alloc,
  vk::Buffer buffer, vk::DeviceAddress addr, vk::DeviceSize size, BufferIntent intent
)
: m_resourceManager(resourceManager),
  m_allocation(alloc),
  m_buffer(buffer),
  m_address(addr),
  m_size(size),
  m_intent(intent)
{}

Buffer::Impl::~Impl() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind         = ResourceKind::Buffer,
    .retireValues = m_lastUseValues,
    .buffer       = m_buffer,
    .allocation   = m_allocation,
    .address      = m_address
  });
}

Image::Image(std::weak_ptr<ResourceManager> resourceManager, const ImageInfo& info)
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

Image::~Image() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind           = ResourceKind::Image,
    .retireValues   = m_lastUseValues,
    .image          = m_image,
    .view           = m_view,
    .allocation     = m_allocation,
    .imageId        = m_id,
    .storageBinding = m_storageBinding,
    .storageSlot    = m_heapIndexStorage,
    .sampledBinding = m_sampledBinding,
    .sampledSlot    = m_heapIndexSampled
  });
}

Img2D::Img2D(std::shared_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Img2D::dims() const {
  return { m_img->m_width, m_img->m_height };
}

std::size_t Img2D::size() const {
  return m_img->m_size;
}

Format Img2D::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t Img2D::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

void Img2D::write(std::span<const std::byte> data, Layout layout) {
  auto rm = m_img->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeImage(ImageWriteInfo{
    .img    = m_img,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout)
  });
}

Img3D::Img3D(std::shared_ptr<Image> img) : m_img(img) {}

std::tuple<uint32_t, uint32_t, uint32_t> Img3D::dims() const {
  return { m_img->m_width, m_img->m_height, m_img->m_depth };
}

std::size_t Img3D::size() const {
  return m_img->m_size;
}

Format Img3D::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t Img3D::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

void Img3D::write(uint32_t depth, std::span<const std::byte> data, Layout layout) {
  if (depth > m_img->m_depth)
    GRF_PANIC("Tried to write to invalid depth of Img3D");

  if (data.size() > m_img->m_size)
    GRF_PANIC("Out of bounds Img3D write. Byte size should be less than or equal to the allocation size");

  auto rm = m_img->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeImage(ImageWriteInfo{
    .img    = m_img,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout),
    .depth  = static_cast<int32_t>(depth)
  });
}

Tex2D::Tex2D(std::shared_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Tex2D::dims() const {
  return { m_img->m_width, m_img->m_height };
}

std::size_t Tex2D::size() const {
  return m_img->m_size;
}

Format Tex2D::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t Tex2D::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

void Tex2D::write(std::span<const std::byte> data, Layout layout) {
  auto rm = m_img->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeImage(ImageWriteInfo{
    .img    = m_img,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout)
  });
}

Tex3D::Tex3D(std::shared_ptr<Image> img) : m_img(img) {}

std::tuple<uint32_t, uint32_t, uint32_t> Tex3D::dims() const {
  return { m_img->m_width, m_img->m_height, m_img->m_depth };
}

std::size_t Tex3D::size() const {
  return m_img->m_size;
}

Format Tex3D::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t Tex3D::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

void Tex3D::write(uint32_t depth, std::span<const std::byte> data, Layout layout) {
  auto rm = m_img->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeImage(ImageWriteInfo{
    .img    = m_img,
    .data   = data,
    .layout = static_cast<vk::ImageLayout>(layout),
    .depth  = static_cast<int32_t>(depth)
  });
}

Cubemap::Cubemap(std::shared_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> Cubemap::dims() const {
  return { m_img->m_width, m_img->m_height };
}

std::size_t Cubemap::size() const {
  return m_img->m_size;
}

Format Cubemap::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t Cubemap::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

void Cubemap::write(CubeFace face, std::span<const std::byte> data, Layout layout) {
  auto rm = m_img->m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->writeImage(ImageWriteInfo{
    .img        = m_img,
    .data       = data,
    .layout     = static_cast<vk::ImageLayout>(layout),
    .face       = face,
    .isCubemap  = true
  });
}

DepthImage::DepthImage(std::shared_ptr<Image> img) : m_img(img) {}

std::pair<uint32_t, uint32_t> DepthImage::dims() const {
  return { m_img->m_width, m_img->m_height };
}

Format DepthImage::format() const {
  return static_cast<Format>(m_img->m_format);
}

uint32_t DepthImage::heapIndex() const {
  return m_img->m_heapIndexSampled;
}

Sampler::Sampler(std::shared_ptr<Sampler::Impl> impl) : m_impl(impl) {}

Filter Sampler::magFilter() const {
  return static_cast<Filter>(m_impl->m_magFilter);
}

Filter Sampler::minFilter() const {
  return static_cast<Filter>(m_impl->m_minFilter);
}

SampleMode Sampler::uMode() const {
  return static_cast<SampleMode>(m_impl->m_uMode);
}

SampleMode Sampler::vMode() const {
  return static_cast<SampleMode>(m_impl->m_vMode);
}

SampleMode Sampler::wMode() const {
  return static_cast<SampleMode>(m_impl->m_wMode);
}

bool Sampler::anisotropicFiltering() const {
  return m_impl->m_anisotropicFiltering;
}

uint32_t Sampler::heapIndex() const {
  return m_impl->m_index;
}

Sampler::Impl::Impl(
  std::weak_ptr<ResourceManager> resourceManager,
  uint64_t id, vk::Sampler sampler, const SamplerSettings& settings
)
: m_resourceManager(resourceManager), m_id(id), m_sampler(sampler) {
  m_magFilter = static_cast<vk::Filter>(settings.magFilter);
  m_minFilter = static_cast<vk::Filter>(settings.minFilter);
  m_uMode = static_cast<vk::SamplerAddressMode>(settings.uMode);
  m_vMode = static_cast<vk::SamplerAddressMode>(settings.vMode);
  m_wMode = static_cast<vk::SamplerAddressMode>(settings.wMode);
  m_anisotropicFiltering = settings.anisotropicFiltering;
}

Sampler::Impl::~Impl() {
  auto rm = m_resourceManager.lock();
  if (rm == nullptr) return;

  rm->scheduleDestruction(Grave{
    .kind           = ResourceKind::Sampler,
    .retireValues   = m_lastUseValues,
    .sampler        = m_sampler,
    .samplerId      = m_id,
    .sampledBinding = m_binding,
    .sampledSlot    = m_index
  });
}

}
