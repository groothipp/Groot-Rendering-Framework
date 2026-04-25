#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

TEST_CASE("ring: createBufferRing produces flightFrames distinct buffers", "[ring][buffer]") {
  grf::GRF grf;
  const std::size_t bufferSize = sizeof(uint32_t) * 64;

  grf::Ring<grf::Buffer> ring = grf.createBufferRing(grf::BufferIntent::FrequentUpdate, bufferSize);

  std::set<uint64_t> addresses;
  for (uint32_t i = 0; i < 2; ++i) {
    grf::Buffer& buf = ring[i];
    CHECK(buf.size() == bufferSize);
    CHECK(buf.intent() == grf::BufferIntent::FrequentUpdate);
    CHECK(buf.address() != 0x0);
    addresses.insert(buf.address());
  }

  CHECK(addresses.size() == 2);
}

TEST_CASE("ring: buffer ring honors custom flightFrames", "[ring][buffer]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 4 });
  const std::size_t bufferSize = 256;

  grf::Ring<grf::Buffer> ring = grf.createBufferRing(grf::BufferIntent::GPUOnly, bufferSize);

  std::set<uint64_t> addresses;
  for (uint32_t i = 0; i < 4; ++i) {
    grf::Buffer& buf = ring[i];
    CHECK(buf.size() == bufferSize);
    CHECK(buf.intent() == grf::BufferIntent::GPUOnly);
    addresses.insert(buf.address());
  }

  CHECK(addresses.size() == 4);
}

TEST_CASE("ring: subscript returns a stable reference for the same index", "[ring][buffer]") {
  grf::GRF grf;
  grf::Ring<grf::Buffer> ring = grf.createBufferRing(grf::BufferIntent::GPUOnly, 128);

  CHECK(&ring[0] == &ring[0]);
  CHECK(&ring[1] == &ring[1]);
  CHECK(&ring[0] != &ring[1]);
  CHECK(ring[0].address() == ring[0].address());
}

TEST_CASE("ring: const subscript returns the same element as mutable subscript", "[ring][buffer]") {
  grf::GRF grf;
  grf::Ring<grf::Buffer> ring = grf.createBufferRing(grf::BufferIntent::GPUOnly, 128);
  const grf::Ring<grf::Buffer>& cref = ring;

  CHECK(&cref[0] == &ring[0]);
  CHECK(&cref[1] == &ring[1]);
  CHECK(cref[0].address() == ring[0].address());
}

TEST_CASE("ring: createImg2DRing produces flightFrames distinct images", "[ring][img2D]") {
  grf::GRF grf;
  const grf::Format format = grf::Format::rgba16_sfloat;
  const uint32_t width = 64, height = 64;

  grf::Ring<grf::Img2D> ring = grf.createImg2DRing(format, width, height);

  std::set<uint32_t> heapIndices;
  for (uint32_t i = 0; i < 2; ++i) {
    grf::Img2D& img = ring[i];

    auto [w, h] = img.dims();
    CHECK(w == width);
    CHECK(h == height);
    CHECK(img.format() == format);

    const uint32_t idx = img.heapIndex();
    CHECK(idx != 0xFFFFFFFFu);
    heapIndices.insert(idx);
  }

  CHECK(heapIndices.size() == 2);
}

TEST_CASE("ring: createImg3DRing produces flightFrames distinct images", "[ring][img3D]") {
  grf::GRF grf;
  const grf::Format format = grf::Format::rgba16_sfloat;
  const uint32_t width = 32, height = 32, depth = 4;

  grf::Ring<grf::Img3D> ring = grf.createImg3DRing(format, width, height, depth);

  std::set<uint32_t> heapIndices;
  for (uint32_t i = 0; i < 2; ++i) {
    grf::Img3D& img = ring[i];

    auto [w, h, d] = img.dims();
    CHECK(w == width);
    CHECK(h == height);
    CHECK(d == depth);
    CHECK(img.format() == format);

    const uint32_t idx = img.heapIndex();
    CHECK(idx != 0xFFFFFFFFu);
    heapIndices.insert(idx);
  }

  CHECK(heapIndices.size() == 2);
}

TEST_CASE("ring: createFenceRing produces flightFrames valid fences", "[ring][sync]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 3 });

  SECTION("unsignaled") {
    grf::Ring<grf::Fence> ring = grf.createFenceRing(false);
    for (uint32_t i = 0; i < 3; ++i) CHECK(ring[i].valid());
  }

  SECTION("signaled") {
    grf::Ring<grf::Fence> ring = grf.createFenceRing(true);
    for (uint32_t i = 0; i < 3; ++i) CHECK(ring[i].valid());
  }

  SECTION("default argument") {
    grf::Ring<grf::Fence> ring = grf.createFenceRing();
    for (uint32_t i = 0; i < 3; ++i) CHECK(ring[i].valid());
  }
}

TEST_CASE("ring: createSemaphoreRing produces flightFrames valid semaphores", "[ring][sync]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 3 });
  grf::Ring<grf::Semaphore> ring = grf.createSemaphoreRing();
  for (uint32_t i = 0; i < 3; ++i) CHECK(ring[i].valid());
}

TEST_CASE("ring: indexing with beginFrame index walks every slot then wraps", "[ring][frame]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 3 });

  grf::Ring<grf::Buffer> bufferRing = grf.createBufferRing(grf::BufferIntent::GPUOnly, 64);
  grf::Ring<grf::Fence>  fenceRing  = grf.createFenceRing();

  std::vector<uint64_t> addresses;
  for (int i = 0; i < 6; ++i) {
    auto [idx, _] = grf.beginFrame();
    addresses.push_back(bufferRing[idx].address());
    CHECK(fenceRing[idx].valid());
    grf.endFrame();
  }

  REQUIRE(addresses.size() == 6);
  CHECK(addresses[0] != addresses[1]);
  CHECK(addresses[1] != addresses[2]);
  CHECK(addresses[0] != addresses[2]);
  CHECK(addresses[3] == addresses[0]);
  CHECK(addresses[4] == addresses[1]);
  CHECK(addresses[5] == addresses[2]);
}
