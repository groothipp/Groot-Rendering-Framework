add_executable(nbody
  ${CMAKE_CURRENT_SOURCE_DIR}/nbody.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aabb_debug.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/brush.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/lvbh_tree.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/particles.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/physics.cpp
)

target_compile_definitions(nbody PRIVATE
  "SHADERS=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\""
)

target_include_directories(nbody PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(nbody PRIVATE
  grf
)
