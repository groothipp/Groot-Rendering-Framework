#include "public/gpu.hpp"
#include "public/buffer.hpp"
#include "public/enums.hpp"

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
  grf::GPU gpu;
  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to host-visible buffer", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));

  Uniforms u{ .frame = 42, .time = 1.5f, .flags = -1 };
  buf.write(u);

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to device-local buffer", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  Uniforms u{ .frame = 0, .time = 0.0f, .flags = 0 };
  buf.write(u);

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: span of trivially-copyable data", "[resource-update]") {
  grf::GPU gpu;
  const std::size_t count = 256;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * count);

  std::vector<uint32_t> data(count);
  for (std::size_t i = 0; i < count; ++i) data[i] = static_cast<uint32_t>(i);
  buf.write(std::span<const uint32_t>(data));

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: temporary bytes survive until flush", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  buf.write(Uniforms{ .frame = 7, .time = 0.1f, .flags = 2 });

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: write with non-zero offset", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * 16);

  uint32_t value = 99;
  buf.write(value, sizeof(uint32_t) * 4);

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: mixed intents in single flush", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer hostBuf = gpu.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));
  grf::Buffer devBuf  = gpu.createBuffer(grf::BufferIntent::GPUOnly,        sizeof(Uniforms));

  hostBuf.write(Uniforms{ .frame = 1, .time = 0.0f, .flags = 0 });
  devBuf.write( Uniforms{ .frame = 2, .time = 0.0f, .flags = 0 });

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: multiple begin+wait cycles", "[resource-update]") {
  grf::GPU gpu;
  grf::Buffer buf = gpu.createBuffer(grf::BufferIntent::SingleUpdate, sizeof(uint32_t) * 16);

  std::array<uint32_t, 16> data{};
  for (int frame = 0; frame < 5; ++frame) {
    for (std::size_t i = 0; i < data.size(); ++i)
      data[i] = static_cast<uint32_t>(frame * 100 + i);
    buf.write(std::span<const uint32_t>(data));

    gpu.beginResourceUpdates();
    gpu.waitForResourceUpdates();
  }
  SUCCEED();
}

TEST_CASE("resource update: many buffers in one flush", "[resource-update]") {
  grf::GPU gpu;
  const std::size_t n = 8;
  std::vector<grf::Buffer> buffers;
  buffers.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    buffers.push_back(gpu.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * 4));

  for (std::size_t i = 0; i < n; ++i) {
    std::array<uint32_t, 4> payload{
      static_cast<uint32_t>(i),
      static_cast<uint32_t>(i + 1),
      static_cast<uint32_t>(i + 2),
      static_cast<uint32_t>(i + 3)
    };
    buffers[i].write(std::span<const uint32_t>(payload));
  }

  gpu.beginResourceUpdates();
  gpu.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: wait without any begin is safe", "[resource-update]") {
  grf::GPU gpu;
  gpu.waitForResourceUpdates();
  SUCCEED();
}
