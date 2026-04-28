cmake_minimum_required(VERSION 4.3)

option(GRF_DEBUG "compile libgrf with debug flags" OFF)
option(GRF_MAKE_TESTS "compile grf tests" OFF)

project(GrootRenderingFramework
  VERSION 0.7.0
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

find_path(GLM_INCLUDE_DIR glm/glm.hpp REQUIRED)

find_library(GLSLANG_LIB NAMES glslang REQUIRED)
find_library(SPIRV_LIB NAMES SPIRV REQUIRED)
find_library(SPIRV_TOOLS_LIB NAMES SPIRV-Tools REQUIRED)
find_library(SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt REQUIRED)

set(GRF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(GRF_PUBLIC_INCL_DIR ${CMAKE_SOURCE_DIR}/public)
set(GRF_INTERNAL_INCL_DIR ${CMAKE_SOURCE_DIR}/internal)

set(GRF_PUBLIC_HEADERS
  ${GRF_PUBLIC_INCL_DIR}/cmd.hpp
  ${GRF_PUBLIC_INCL_DIR}/grf.hpp
  ${GRF_PUBLIC_INCL_DIR}/gui.hpp
  ${GRF_PUBLIC_INCL_DIR}/input.hpp
  ${GRF_PUBLIC_INCL_DIR}/math.hpp
  ${GRF_PUBLIC_INCL_DIR}/pipelines.hpp
  ${GRF_PUBLIC_INCL_DIR}/profiler.hpp
  ${GRF_PUBLIC_INCL_DIR}/resources.hpp
  ${GRF_PUBLIC_INCL_DIR}/ring.hpp
  ${GRF_PUBLIC_INCL_DIR}/shader.hpp
  ${GRF_PUBLIC_INCL_DIR}/sync.hpp
  ${GRF_PUBLIC_INCL_DIR}/types.hpp
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
  ${GRF_INTERNAL_INCL_DIR}/resources/sampler.hpp

  ${GRF_INTERNAL_INCL_DIR}/allocator.hpp
  ${GRF_INTERNAL_INCL_DIR}/cmd.hpp
  ${GRF_INTERNAL_INCL_DIR}/descriptor_heap.hpp
  ${GRF_INTERNAL_INCL_DIR}/graveyard.hpp
  ${GRF_INTERNAL_INCL_DIR}/grf.hpp
  ${GRF_INTERNAL_INCL_DIR}/gui.hpp
  ${GRF_INTERNAL_INCL_DIR}/input.hpp
  ${GRF_INTERNAL_INCL_DIR}/log.hpp
  ${GRF_INTERNAL_INCL_DIR}/pipelines.hpp
  ${GRF_INTERNAL_INCL_DIR}/profiler.hpp
  ${GRF_INTERNAL_INCL_DIR}/swapchain_image.hpp
  ${GRF_INTERNAL_INCL_DIR}/sync.hpp
)

set(GRF_SOURCES
  ${GRF_SOURCE_DIR}/gsl/assembler.cpp
  ${GRF_SOURCE_DIR}/gsl/lexer.cpp
  ${GRF_SOURCE_DIR}/gsl/parser.cpp
  ${GRF_SOURCE_DIR}/gsl/shader.cpp
  ${GRF_SOURCE_DIR}/gsl/shader_manager.cpp

  ${GRF_SOURCE_DIR}/resources/resource_manager.cpp

  ${GRF_SOURCE_DIR}/allocator.cpp
  ${GRF_SOURCE_DIR}/cmd.cpp
  ${GRF_SOURCE_DIR}/descriptor_heap.cpp
  ${GRF_SOURCE_DIR}/grf.cpp
  ${GRF_SOURCE_DIR}/grf_init.cpp
  ${GRF_SOURCE_DIR}/gui.cpp
  ${GRF_SOURCE_DIR}/input.cpp
  ${GRF_SOURCE_DIR}/profiler.cpp
  ${GRF_SOURCE_DIR}/pipelines.cpp
  ${GRF_SOURCE_DIR}/resources.cpp
  ${GRF_SOURCE_DIR}/stb.cpp
  ${GRF_SOURCE_DIR}/swapchain_image.cpp
  ${GRF_SOURCE_DIR}/sync.cpp
  ${GRF_SOURCE_DIR}/vma.cpp
)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

set(IMGUI_PUBLIC_HEADERS
  ${IMGUI_DIR}/imgui.h
  ${IMGUI_DIR}/imconfig.h
)

set_source_files_properties(
  ${GRF_SOURCE_DIR}/vma.cpp
  PROPERTIES COMPILE_FLAGS "-Wno-nullability-completeness"
)

set_source_files_properties(
  ${GRF_SOURCE_DIR}/stb.cpp
  PROPERTIES COMPILE_FLAGS "-Wno-unused-but-set-variable -Wno-unused-function"
)

set_source_files_properties(
  ${IMGUI_SOURCES}
  PROPERTIES COMPILE_FLAGS "-Wno-nullability-completeness -Wno-unused-function"
)

add_library(grf
  ${GRF_PUBLIC_HEADERS}
  ${GRF_INTERNAL_HEADERS}
  ${GRF_SOURCES}
  ${IMGUI_SOURCES}
)

target_compile_definitions(grf PRIVATE
  "GRF_VERSION=\"${PROJECT_VERSION}\""
  "VULKAN_HPP_NO_CONSTRUCTORS"
  "GLFW_INCLUDE_NONE"
)

target_compile_definitions(grf PUBLIC
  "GLM_FORCE_DEPTH_ZERO_TO_ONE"
)

target_include_directories(grf
  PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
  INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(grf PUBLIC cxx_std_26)

target_link_libraries(grf PRIVATE
  Vulkan::Vulkan
  Vulkan::shaderc_combined
  ${GLSLANG_LIB}
  ${SPIRV_LIB}
  ${SPIRV_TOOLS_LIB}
  ${SPIRV_TOOLS_OPT_LIB}
  glfw
  $<BUILD_INTERFACE:glm::glm>
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
    ${TEST_SOURCE_DIR}/cmd.cpp
    ${TEST_SOURCE_DIR}/destruction.cpp
    ${TEST_SOURCE_DIR}/frame.cpp
    ${TEST_SOURCE_DIR}/lexer.cpp
    ${TEST_SOURCE_DIR}/parser.cpp
    ${TEST_SOURCE_DIR}/resource_update.cpp
    ${TEST_SOURCE_DIR}/pipelines.cpp
    ${TEST_SOURCE_DIR}/render.cpp
    ${TEST_SOURCE_DIR}/ring.cpp
    ${TEST_SOURCE_DIR}/shaders.cpp
  )

  add_executable(test ${TEST_SOURCES})

  target_compile_definitions(test PRIVATE
    "GRF_TEST_DIR=\"${TEST_SOURCE_DIR}\""
  )

  target_include_directories(test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${IMGUI_DIR}
  )

  target_link_libraries(test PRIVATE
    grf
    Catch2::Catch2WithMain
  )
endif()

install(TARGETS grf
  EXPORT grfTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${GRF_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/grf
)

install(FILES ${IMGUI_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/grf
)

get_filename_component(GLM_REAL_DIR ${GLM_INCLUDE_DIR}/glm REALPATH)
install(DIRECTORY ${GLM_REAL_DIR}/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glm
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
    PATTERN "*.inl"
)

install(EXPORT grfTargets
  FILE grfTargets.cmake
  NAMESPACE grf::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grf
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/grfConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/grfConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grf
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/grfConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/grfConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/grfConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grf
)