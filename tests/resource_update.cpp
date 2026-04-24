#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace {

struct Uniforms {
  uint32_t frame;
  float    time;
  int32_t  flags;
};

}

TEST_CASE("resource update: empty flush is a no-op", "[resource-update]") {
  grf::GRF grf;
  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to host-visible buffer", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));

  Uniforms u{ .frame = 42, .time = 1.5f, .flags = -1 };
  buf.write(u);

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to device-local buffer", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  Uniforms u{ .frame = 0, .time = 0.0f, .flags = 0 };
  buf.write(u);

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: span of trivially-copyable data", "[resource-update]") {
  grf::GRF grf;
  const std::size_t count = 256;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * count);

  std::vector<uint32_t> data(count);
  for (std::size_t i = 0; i < count; ++i) data[i] = static_cast<uint32_t>(i);
  buf.write(std::span<const uint32_t>(data));

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: temporary bytes survive until flush", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  buf.write(Uniforms{ .frame = 7, .time = 0.1f, .flags = 2 });

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: write with non-zero offset", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * 16);

  uint32_t value = 99;
  buf.write(value, sizeof(uint32_t) * 4);

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: mixed intents in single flush", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer hostBuf = grf.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));
  grf::Buffer devBuf  = grf.createBuffer(grf::BufferIntent::GPUOnly,        sizeof(Uniforms));

  hostBuf.write(Uniforms{ .frame = 1, .time = 0.0f, .flags = 0 });
  devBuf.write( Uniforms{ .frame = 2, .time = 0.0f, .flags = 0 });

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: multiple begin+wait cycles", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::SingleUpdate, sizeof(uint32_t) * 16);

  std::array<uint32_t, 16> data{};
  for (int frame = 0; frame < 5; ++frame) {
    for (std::size_t i = 0; i < data.size(); ++i)
      data[i] = static_cast<uint32_t>(frame * 100 + i);
    buf.write(std::span<const uint32_t>(data));

    grf.beginResourceUpdates();
    grf.waitForResourceUpdates();
  }
  SUCCEED();
}

TEST_CASE("resource update: many buffers in one flush", "[resource-update]") {
  grf::GRF grf;
  const std::size_t n = 8;
  std::vector<grf::Buffer> buffers;
  buffers.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    buffers.push_back(grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * 4));

  for (std::size_t i = 0; i < n; ++i) {
    std::array<uint32_t, 4> payload{
      static_cast<uint32_t>(i),
      static_cast<uint32_t>(i + 1),
      static_cast<uint32_t>(i + 2),
      static_cast<uint32_t>(i + 3)
    };
    buffers[i].write(std::span<const uint32_t>(payload));
  }

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: wait without any begin is safe", "[resource-update]") {
  grf::GRF grf;
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource read: single struct round-trip", "[resource-read]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::Readable, sizeof(Uniforms));

  Uniforms written{ .frame = 42, .time = 1.5f, .flags = -1 };
  buf.write(written);

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();

  Uniforms readBack = buf.read<Uniforms>();
  CHECK(readBack.frame == written.frame);
  CHECK(readBack.time  == written.time);
  CHECK(readBack.flags == written.flags);
}

TEST_CASE("resource read: span round-trip", "[resource-read]") {
  grf::GRF grf;
  const std::size_t count = 256;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::Readable, sizeof(uint32_t) * count);

  std::vector<uint32_t> written(count);
  for (std::size_t i = 0; i < count; ++i) written[i] = static_cast<uint32_t>(i * 7 + 3);
  buf.write(std::span<const uint32_t>(written));

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();

  std::vector<uint32_t> readBack(count);
  buf.read(std::span<uint32_t>(readBack));
  for (std::size_t i = 0; i < count; ++i)
    CHECK(readBack[i] == written[i]);
}

TEST_CASE("resource read: non-zero offset", "[resource-read]") {
  grf::GRF grf;
  const std::size_t count = 16;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::Readable, sizeof(uint32_t) * count);

  std::array<uint32_t, count> written{};
  for (std::size_t i = 0; i < count; ++i) written[i] = static_cast<uint32_t>(i + 100);
  buf.write(std::span<const uint32_t>(written));

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();

  uint32_t readBack = buf.read<uint32_t>(sizeof(uint32_t) * 4);
  CHECK(readBack == written[4]);
}

TEST_CASE("resource read: partial readback shorter than buffer", "[resource-read]") {
  grf::GRF grf;
  const std::size_t full = 64;
  const std::size_t partial = 10;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::Readable, sizeof(uint32_t) * full);

  std::vector<uint32_t> written(full);
  for (std::size_t i = 0; i < full; ++i) written[i] = static_cast<uint32_t>(i);
  buf.write(std::span<const uint32_t>(written));

  grf.beginResourceUpdates();
  grf.waitForResourceUpdates();

  std::vector<uint32_t> readBack(partial);
  buf.read(std::span<uint32_t>(readBack));
  for (std::size_t i = 0; i < partial; ++i)
    CHECK(readBack[i] == written[i]);
}

TEST_CASE("resource read: non-readable intent yields zeros", "[resource-read]") {
  grf::GRF grf;

  SECTION("single struct from GPUOnly") {
    grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));
    buf.write(Uniforms{ .frame = 7, .time = 2.0f, .flags = 9 });
    grf.beginResourceUpdates();
    grf.waitForResourceUpdates();

    Uniforms readBack = buf.read<Uniforms>();
    CHECK(readBack.frame == 0u);
    CHECK(readBack.time  == 0.0f);
    CHECK(readBack.flags == 0);
  }

  SECTION("vector from FrequentUpdate") {
    const std::size_t count = 8;
    grf::Buffer buf = grf.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(uint32_t) * count);
    std::array<uint32_t, count> payload{ 1, 2, 3, 4, 5, 6, 7, 8 };
    buf.write(std::span<const uint32_t>(payload));
    grf.beginResourceUpdates();
    grf.waitForResourceUpdates();

    std::vector<uint32_t> readBack(count);
    buf.read(std::span<uint32_t>(readBack));
    for (std::size_t i = 0; i < count; ++i)
      CHECK(readBack[i] == 0u);
  }
}
