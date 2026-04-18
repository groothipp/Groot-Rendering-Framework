#include <catch2/catch_test_macros.hpp>

#include <print>

TEST_CASE( "test" ) {
  std::println(GRF_TEST_DIR);
  CHECK( true );
}