add_executable(glass_bubbles
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_compile_definitions(glass_bubbles PRIVATE
  "SHADERS=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\""
  "ASSETS=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\""
)

target_include_directories(glass_bubbles PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(glass_bubbles PRIVATE
  grf
)
