cmake_minimum_required(VERSION 4.3)

option(GRF_DEBUG "compile libgrf with debug flags" ON)
option(GRF_MAKE_TESTS "compile grf tests" ON)

set(GRF_VERSION_MAJOR 0)
set(GRF_VERSION_MINOR 0)
set(GRF_VERSION_PATCH 0)

project(GrootRenderingFramework
  VERSION "${GRF_VERSION_MAJOR}.${GRF_VERSION_MINOR}.${GRF_VERSION_PATCH}"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 26)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)

if(GRF_MAKE_TESTS)
  add_subdirectory(${CMAKE_SOURCE_DIR}/tests)
endif()