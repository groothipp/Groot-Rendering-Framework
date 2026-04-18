find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

find_library(GLSLANG_LIB NAMES glslang REQUIRED)
find_library(SPIRV_LIB NAMES SPIRV REQUIRED)
find_library(SPIRV_TOOLS_LIB NAMES SPIRV-Tools REQUIRED)
find_library(SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt REQUIRED)

set(GRF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GRF_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR/include})

set(GRF_HEADERS

)

set(GRF_SOURCES
  ${GRF_SOURCE_DIR}/vma.cpp
)

set_source_files_properties(
  ${GRF_SOURCE_DIR}/vma.cpp
  PROPERTIES COMPILE_FLAGS "-Wno-nullability-completeness"
)

add_library(grf ${GRF_HEADERS} ${GRF_SOURCES})

target_compile_definitions(grf PRIVATE
  "GRF_VERSION=\"${GRF_VERSION_MAJOR}.${GRF_VERSION_MINOR}.${GRF_VERSION_PATCH}\""
  "VULKAN_HPP_NO_CONSTRUCTORS"
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
)