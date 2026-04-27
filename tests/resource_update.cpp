#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
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
  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to host-visible buffer", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));

  Uniforms u{ .frame = 42, .time = 1.5f, .flags = -1 };
  buf.write(u);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: single struct to device-local buffer", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  Uniforms u{ .frame = 0, .time = 0.0f, .flags = 0 };
  buf.write(u);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: span of trivially-copyable data", "[resource-update]") {
  grf::GRF grf;
  const std::size_t count = 256;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * count);

  std::vector<uint32_t> data(count);
  for (std::size_t i = 0; i < count; ++i) data[i] = static_cast<uint32_t>(i);
  buf.write(data);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: temporary bytes survive until flush", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));

  buf.write(Uniforms{ .frame = 7, .time = 0.1f, .flags = 2 });

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: write with non-zero offset", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(uint32_t) * 16);

  uint32_t value = 99;
  buf.write(value, sizeof(uint32_t) * 4);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: mixed intents in single flush", "[resource-update]") {
  grf::GRF grf;
  grf::Buffer hostBuf = grf.createBuffer(grf::BufferIntent::FrequentUpdate, sizeof(Uniforms));
  grf::Buffer devBuf  = grf.createBuffer(grf::BufferIntent::GPUOnly,        sizeof(Uniforms));

  hostBuf.write(Uniforms{ .frame = 1, .time = 0.0f, .flags = 0 });
  devBuf.write( Uniforms{ .frame = 2, .time = 0.0f, .flags = 0 });

  grf.beginFrame();
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
    buf.write(data);

    grf.beginFrame();
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
    buffers[i].write(payload);
  }

  grf.beginFrame();
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

  grf.beginFrame();
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
  buf.write(written);

  grf.beginFrame();
  grf.waitForResourceUpdates();

  std::vector<uint32_t> readBack(count);
  buf.read(readBack);
  for (std::size_t i = 0; i < count; ++i)
    CHECK(readBack[i] == written[i]);
}

TEST_CASE("resource read: non-zero offset", "[resource-read]") {
  grf::GRF grf;
  const std::size_t count = 16;
  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::Readable, sizeof(uint32_t) * count);

  std::array<uint32_t, count> written{};
  for (std::size_t i = 0; i < count; ++i) written[i] = static_cast<uint32_t>(i + 100);
  buf.write(written);

  grf.beginFrame();
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
  buf.write(written);

  grf.beginFrame();
  grf.waitForResourceUpdates();

  std::vector<uint32_t> readBack(partial);
  buf.read(readBack);
  for (std::size_t i = 0; i < partial; ++i)
    CHECK(readBack[i] == written[i]);
}

TEST_CASE("resource read: non-readable intent yields zeros", "[resource-read]") {
  grf::GRF grf;

  SECTION("single struct from GPUOnly") {
    grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));
    buf.write(Uniforms{ .frame = 7, .time = 2.0f, .flags = 9 });
    grf.beginFrame();
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
    buf.write(payload);
    grf.beginFrame();
    grf.waitForResourceUpdates();

    std::vector<uint32_t> readBack(count);
    buf.read(readBack);
    for (std::size_t i = 0; i < count; ++i)
      CHECK(readBack[i] == 0u);
  }
}

TEST_CASE("resource update: tex2D write", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 64;
  const uint32_t height = 64;
  grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, width, height);

  std::vector<std::byte> pixels(width * height * 4, std::byte{0xAB});
  tex.write(pixels, grf::Layout::ShaderReadOptimal);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: img2D write", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 64;
  const uint32_t height = 64;
  grf::Img2D img = grf.createImg2D(grf::Format::rgba8_unorm, width, height);

  std::vector<std::byte> pixels(width * height * 4, std::byte{0xCD});
  img.write(pixels, grf::Layout::General);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: tex3D per-slice writes", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 32;
  const uint32_t height = 32;
  const uint32_t depth  = 4;
  grf::Tex3D tex = grf.createTex3D(grf::Format::rgba8_unorm, width, height, depth);

  const std::size_t sliceBytes = width * height * 4;
  std::vector<std::byte> sliceData(sliceBytes, std::byte{0x01});

  for (uint32_t z = 0; z < depth; ++z)
    tex.write(z, sliceData, grf::Layout::ShaderReadOptimal);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: img3D per-slice writes", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 32;
  const uint32_t height = 32;
  const uint32_t depth  = 4;
  grf::Img3D img = grf.createImg3D(grf::Format::rgba8_unorm, width, height, depth);

  const std::size_t sliceBytes = width * height * 4;
  std::vector<std::byte> sliceData(sliceBytes, std::byte{0x02});

  for (uint32_t z = 0; z < depth; ++z)
    img.write(z, sliceData, grf::Layout::General);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: cubemap per-face writes", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 64;
  const uint32_t height = 64;
  grf::Cubemap cube = grf.createCubemap(grf::Format::rgba8_unorm, width, height);

  const std::size_t faceBytes = width * height * 4;
  std::vector<std::byte> faceData(faceBytes, std::byte{0xFF});

  for (auto face : {
    grf::CubeFace::Right, grf::CubeFace::Left,
    grf::CubeFace::Top,   grf::CubeFace::Bottom,
    grf::CubeFace::Back,  grf::CubeFace::Front,
  }) {
    cube.write(face, faceData, grf::Layout::ShaderReadOptimal);
  }

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: cubemap single face write", "[resource-update][image]") {
  grf::GRF grf;
  const uint32_t width  = 64;
  const uint32_t height = 64;
  grf::Cubemap cube = grf.createCubemap(grf::Format::rgba8_unorm, width, height);

  std::vector<std::byte> faceData(width * height * 4, std::byte{0x42});
  cube.write(grf::CubeFace::Top, faceData, grf::Layout::ShaderReadOptimal);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}

TEST_CASE("resource update: mixed buffer and image writes in one flush", "[resource-update][image]") {
  grf::GRF grf;

  grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, sizeof(Uniforms));
  buf.write(Uniforms{ .frame = 1, .time = 0.5f, .flags = 2 });

  const uint32_t w = 32;
  const uint32_t h = 32;

  grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, w, h);
  std::vector<std::byte> texPixels(w * h * 4, std::byte{0x10});
  tex.write(texPixels, grf::Layout::ShaderReadOptimal);

  grf::Img2D img = grf.createImg2D(grf::Format::rgba8_unorm, w, h);
  std::vector<std::byte> imgPixels(w * h * 4, std::byte{0x20});
  img.write(imgPixels, grf::Layout::General);

  grf.beginFrame();
  grf.waitForResourceUpdates();
  SUCCEED();
}
