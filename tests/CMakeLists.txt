find_package(Catch2 3 REQUIRED)

set(TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(TEST_SOURCES
  ${TEST_SOURCE_DIR}/test.cpp
)

add_executable(test ${TEST_SOURCES})

target_compile_definitions(test PRIVATE
  "GRF_TEST_DIR=\"${TEST_SOURCE_DIR}\""
)

target_include_directories(test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test PRIVATE
  grf
  Catch2::Catch2WithMain
)