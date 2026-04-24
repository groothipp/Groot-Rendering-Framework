#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

TEST_CASE("frame: running returns true on fresh GRF", "[frame]") {
  grf::GRF grf;
  CHECK(grf.running());
}

TEST_CASE("frame: running returns false when endCond is true", "[frame]") {
  grf::GRF grf;
  CHECK_FALSE(grf.running([]{ return true; }));
}

TEST_CASE("frame: running returns true when endCond is false", "[frame]") {
  grf::GRF grf;
  CHECK(grf.running([]{ return false; }));
}

TEST_CASE("frame: beginFrame returns zero time on first call", "[frame]") {
  grf::GRF grf;
  auto [index, time] = grf.beginFrame();
  CHECK(index == 0u);
  CHECK(time == 0.0);
}

TEST_CASE("frame: frame index advances and wraps at flightFrames", "[frame]") {
  grf::GRF grf;

  auto [idx0, _0] = grf.beginFrame();
  CHECK(idx0 == 0u);
  grf.endFrame();

  auto [idx1, _1] = grf.beginFrame();
  CHECK(idx1 == 1u);
  grf.endFrame();

  auto [idx2, _2] = grf.beginFrame();
  CHECK(idx2 == 0u);
}

TEST_CASE("frame: custom flightFrames changes wrap point", "[frame]") {
  grf::GRF grf(grf::Settings{ .flightFrames = 3 });

  std::vector<uint32_t> observed;
  for (int i = 0; i < 4; ++i) {
    auto [idx, _] = grf.beginFrame();
    observed.push_back(idx);
    grf.endFrame();
  }

  REQUIRE(observed.size() == 4);
  CHECK(observed[0] == 0u);
  CHECK(observed[1] == 1u);
  CHECK(observed[2] == 2u);
  CHECK(observed[3] == 0u);
}

TEST_CASE("frame: beginFrame reports non-zero delta after work", "[frame]") {
  grf::GRF grf;

  auto [_, t0] = grf.beginFrame();
  CHECK(t0 == 0.0);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  grf.endFrame();

  auto [_, t1] = grf.beginFrame();
  CHECK(t1 >= 0.005);
}

TEST_CASE("frame: nextSwapchainImage returns without crashing", "[frame]") {
  grf::GRF grf;
  grf.beginFrame();
  grf::SwapchainImage image = grf.nextSwapchainImage();
  SUCCEED();
}

TEST_CASE("sync: createFence produces valid fence", "[sync]") {
  grf::GRF grf;

  SECTION("unsignaled") {
    grf::Fence fence = grf.createFence(false);
    CHECK(fence.valid());
  }

  SECTION("signaled") {
    grf::Fence fence = grf.createFence(true);
    CHECK(fence.valid());
  }

  SECTION("default argument") {
    grf::Fence fence = grf.createFence();
    CHECK(fence.valid());
  }
}

TEST_CASE("sync: waitFences returns immediately for signaled fence", "[sync]") {
  grf::GRF grf;
  grf::Fence fence = grf.createFence(true);

  auto start = std::chrono::steady_clock::now();
  grf.waitFences({ fence });
  auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

  CHECK(elapsed < 0.1);
}

TEST_CASE("sync: createSemaphore produces valid semaphore", "[sync]") {
  grf::GRF grf;
  grf::Semaphore sem = grf.createSemaphore();
  CHECK(sem.valid());
}
