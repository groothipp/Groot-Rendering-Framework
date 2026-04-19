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

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
find_package(glfw3 REQUIRED)

find_library(GLSLANG_LIB NAMES glslang REQUIRED)
find_library(SPIRV_LIB NAMES SPIRV REQUIRED)
find_library(SPIRV_TOOLS_LIB NAMES SPIRV-Tools REQUIRED)
find_library(SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt REQUIRED)

set(GRF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(GRF_PUBLIC_INCL_DIR ${CMAKE_SOURCE_DIR}/public)
set(GRF_INTERNAL_INCL_DIR ${CMAKE_SOURCE_DIR}/internal)

set(GRF_PUBLIC_HEADERS
  ${GRF_PUBLIC_INCL_DIR}/gpu.hpp
  ${GRF_PUBLIC_INCL_DIR}/structs.hpp
)

set(GRF_INTERNAL_HEADERS
  ${GRF_INTERNAL_INCL_DIR}/gpu.hpp
  ${GRF_INTERNAL_INCL_DIR}/log.hpp
)

set(GRF_SOURCES
  ${GRF_SOURCE_DIR}/gpu.cpp
  ${GRF_SOURCE_DIR}/vma.cpp
)

set_source_files_properties(
  ${GRF_SOURCE_DIR}/vma.cpp
  PROPERTIES COMPILE_FLAGS "-Wno-nullability-completeness"
)

add_library(grf ${GRF_PUBLIC_HEADERS} ${GRF_INTERNAL_HEADERS} ${GRF_SOURCES})

target_compile_definitions(grf PRIVATE
  "GRF_VERSION=\"${GRF_VERSION_MAJOR}.${GRF_VERSION_MINOR}.${GRF_VERSION_PATCH}\""
  "VULKAN_HPP_NO_CONSTRUCTORS"
  "GLFW_INCLUDE_NONE"
)

target_include_directories(grf PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(grf PRIVATE
  Vulkan::Vulkan
  Vulkan::shaderc_combined
  ${GLSLANG_LIB}
  ${SPIRV_LIB}
  ${SPIRV_TOOLS_LIB}
  ${SPIRV_TOOLS_OPT_LIB}
  glfw
)

if(GRF_DEBUG)
  target_compile_options(grf PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O0 -g3>
    $<$<CXX_COMPILER_ID:MSVC>:/Od /Zi>
  )
  target_compile_definitions(grf PRIVATE "GRF_DEBUG=1")
else()
  target_compile_options(grf PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3>
    $<$<CXX_COMPILER_ID:MSVC>:/O2>
  )
  target_compile_definitions(grf PRIVATE NDEBUG)
endif()

if(GRF_MAKE_TESTS)
  find_package(Catch2 3 REQUIRED)

  set(TEST_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests)

  set(TEST_SOURCES
    ${TEST_SOURCE_DIR}/end_to_end.cpp
  )

  add_executable(test ${TEST_SOURCES})

  target_compile_definitions(test PRIVATE
    "GRF_TEST_DIR=\"${TEST_SOURCE_DIR}\""
  )

  target_include_directories(test PRIVATE
    ${CMAKE_SOURCE_DIR}
  )

  target_link_libraries(test PRIVATE
    grf
    Catch2::Catch2WithMain
  )
endif()