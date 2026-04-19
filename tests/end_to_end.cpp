#include "public/gpu.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE( "end_to_end" ) {
  grf::GPU gpu;
  gpu.run();
}