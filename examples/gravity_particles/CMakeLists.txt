add_executable(gravity_particles ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_compile_definitions(gravity_particles PRIVATE
  "SHADERS=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\""
)

target_include_directories(gravity_particles PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(gravity_particles PRIVATE
  grf
)