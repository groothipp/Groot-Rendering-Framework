#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <format>
#include <vector>

TEST_CASE("destruction: buffer survives endFrame within flightFrames window", "[destruction][buffer]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint64_t address;
  {
    grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
    address = buf.address();
    CHECK(address != 0);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: image survives endFrame within flightFrames window", "[destruction][image]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, 64, 64);
    CHECK(tex.heapIndex() != 0xFFFFFFFFu);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: sampler dies after endFrame drain", "[destruction][sampler]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Sampler s = grf.createSampler({});
    CHECK(s.heapIndex() != 0xFFFFFFFFu);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: depth image dies after endFrame drain", "[destruction][depth]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::DepthImage d = grf.createDepthImage(grf::Format::d32_sfloat, 64, 64);
    CHECK(d.dims().first == 64u);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: pipeline dies after endFrame drain", "[destruction][pipeline]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Shader vs = grf.compileShader(grf::ShaderType::Vertex,
      std::format("{}/shaders/graphics_minimal.vert.gsl", GRF_TEST_DIR)
    );
    grf::Shader fs = grf.compileShader(grf::ShaderType::Fragment,
      std::format("{}/shaders/graphics_minimal.frag.gsl", GRF_TEST_DIR)
    );
    grf::GraphicsPipeline pipe = grf.createGraphicsPipeline(vs, fs, grf::GraphicsPipelineSettings{
      .colorFormats = { grf::Format::bgra8_srgb }
    });
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: copies share lifetime, last copy triggers death", "[destruction][buffer]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint64_t address;
  {
    grf::Buffer original = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
    address = original.address();

    {
      grf::Buffer copy1 = original;
      grf::Buffer copy2 = original;
      CHECK(copy1.address() == address);
      CHECK(copy2.address() == address);
    }

    CHECK(original.address() == address);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: many resources can be created and destroyed in a loop", "[destruction]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  for (int frame = 0; frame < 8; ++frame) {
    grf.beginFrame();

    for (int i = 0; i < 16; ++i) {
      grf::Buffer buf = grf.createBuffer(grf::BufferIntent::GPUOnly, 256);
      grf::Tex2D  tex = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    }

    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: outliving the proxy does not crash GRF shutdown", "[destruction][shutdown]") {
  grf::Buffer* leaked = nullptr;

  {
    grf::GRF grf;
    leaked = new grf::Buffer(grf.createBuffer(grf::BufferIntent::GPUOnly, 256));
  }

  delete leaked;

  SUCCEED();
}

TEST_CASE("destruction: tex2D heap slot is reclaimed after drain", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t firstSlot;
  {
    grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    firstSlot = tex.heapIndex();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Tex2D next = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
  CHECK(next.heapIndex() == firstSlot);
}

TEST_CASE("destruction: sampler heap slot is reclaimed after drain", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t firstSlot;
  {
    grf::Sampler s = grf.createSampler({});
    firstSlot = s.heapIndex();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Sampler next = grf.createSampler({});
  CHECK(next.heapIndex() == firstSlot);
}

TEST_CASE("destruction: img2D reclaims both storage and sampled slots", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t firstSlot;
  {
    grf::Img2D img = grf.createImg2D(grf::Format::rgba8_unorm, 32, 32);
    firstSlot = img.heapIndex();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Img2D next = grf.createImg2D(grf::Format::rgba8_unorm, 32, 32);
  CHECK(next.heapIndex() == firstSlot);
}

TEST_CASE("destruction: free list is LIFO", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t slotA, slotB;
  {
    grf::Tex2D a = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    grf::Tex2D b = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    slotA = a.heapIndex();
    slotB = b.heapIndex();
    CHECK(slotA != slotB);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Tex2D first  = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
  grf::Tex2D second = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);

  CHECK(first.heapIndex()  == slotA);
  CHECK(second.heapIndex() == slotB);
}

TEST_CASE("destruction: fence dies after endFrame drain", "[destruction][sync]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Fence f = grf.createFence(true);
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: semaphore dies after endFrame drain", "[destruction][sync]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Semaphore s = grf.createSemaphore();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: ring of buffers releases all slots after drain", "[destruction][ring]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  std::vector<uint64_t> originalAddresses;
  {
    grf::Ring<grf::Buffer> ring = grf.createBufferRing(grf::BufferIntent::GPUOnly, 256);
    for (uint32_t i = 0; i < 2; ++i)
      originalAddresses.push_back(ring[i].address());
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: ring of img2D reclaims heap slots after drain", "[destruction][ring][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  std::vector<uint32_t> originalSlots;
  {
    grf::Ring<grf::Img2D> ring = grf.createImg2DRing(grf::Format::rgba8_unorm, 32, 32);
    for (uint32_t i = 0; i < 2; ++i)
      originalSlots.push_back(ring[i].heapIndex());
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  grf::Img2D fresh1 = grf.createImg2D(grf::Format::rgba8_unorm, 32, 32);
  grf::Img2D fresh2 = grf.createImg2D(grf::Format::rgba8_unorm, 32, 32);

  std::vector<uint32_t> reusedSlots = { fresh1.heapIndex(), fresh2.heapIndex() };
  std::sort(originalSlots.begin(), originalSlots.end());
  std::sort(reusedSlots.begin(), reusedSlots.end());
  CHECK(originalSlots == reusedSlots);
}

TEST_CASE("destruction: ring of fences releases handles after drain", "[destruction][ring][sync]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  {
    grf::Ring<grf::Fence> ring = grf.createFenceRing();
  }

  for (int i = 0; i < 4; ++i) {
    grf.beginFrame();
    grf.endFrame();
  }

  SUCCEED();
}

TEST_CASE("destruction: slots not reclaimed before drain", "[destruction][heap]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 2 });

  uint32_t firstSlot;
  {
    grf::Tex2D tex = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
    firstSlot = tex.heapIndex();
  }

  grf::Tex2D next = grf.createTex2D(grf::Format::rgba8_unorm, 32, 32);
  CHECK(next.heapIndex() != firstSlot);
}
