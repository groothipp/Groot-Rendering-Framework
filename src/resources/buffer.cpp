#include "internal/resources/buffer.hpp"
#include "internal/resources/resource_manager.hpp"
#include "internal/log.hpp"
#include "public/enums.hpp"

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

}