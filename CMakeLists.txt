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
  ${GRF_PUBLIC_INCL_DIR}/buffer.hpp
  ${GRF_PUBLIC_INCL_DIR}/enums.hpp
  ${GRF_PUBLIC_INCL_DIR}/grf.hpp
  ${GRF_PUBLIC_INCL_DIR}/image.hpp
  ${GRF_PUBLIC_INCL_DIR}/shader.hpp
  ${GRF_PUBLIC_INCL_DIR}/structs.hpp
)

set(GRF_INTERNAL_HEADERS
  ${GRF_INTERNAL_INCL_DIR}/gsl/assembler.hpp
  ${GRF_INTERNAL_INCL_DIR}/gsl/lexer.hpp
  ${GRF_INTERNAL_INCL_DIR}/gsl/parser.hpp
  ${GRF_INTERNAL_INCL_DIR}/gsl/shader_manager.hpp
  ${GRF_INTERNAL_INCL_DIR}/gsl/tokens.hpp
  ${GRF_INTERNAL_INCL_DIR}/resources/buffer.hpp
  ${GRF_INTERNAL_INCL_DIR}/resources/image.hpp
  ${GRF_INTERNAL_INCL_DIR}/resources/resource_manager.hpp
  ${GRF_INTERNAL_INCL_DIR}/allocator.hpp
  ${GRF_INTERNAL_INCL_DIR}/descriptor_heap.hpp
  ${GRF_INTERNAL_INCL_DIR}/grf.hpp
  ${GRF_INTERNAL_INCL_DIR}/log.hpp
)

set(GRF_SOURCES
  ${GRF_SOURCE_DIR}/gsl/assembler.cpp
  ${GRF_SOURCE_DIR}/gsl/lexer.cpp
  ${GRF_SOURCE_DIR}/gsl/parser.cpp
  ${GRF_SOURCE_DIR}/gsl/shader.cpp
  ${GRF_SOURCE_DIR}/gsl/shader_manager.cpp
  ${GRF_SOURCE_DIR}/resources/buffer.cpp
  ${GRF_SOURCE_DIR}/resources/image.cpp
  ${GRF_SOURCE_DIR}/resources/resource_manager.cpp
  ${GRF_SOURCE_DIR}/allocator.cpp
  ${GRF_SOURCE_DIR}/descriptor_heap.cpp
  ${GRF_SOURCE_DIR}/grf.cpp
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
    ${TEST_SOURCE_DIR}/allocation.cpp
    ${TEST_SOURCE_DIR}/assembler.cpp
    ${TEST_SOURCE_DIR}/lexer.cpp
    ${TEST_SOURCE_DIR}/parser.cpp
    ${TEST_SOURCE_DIR}/resource_update.cpp
    ${TEST_SOURCE_DIR}/shaders.cpp
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