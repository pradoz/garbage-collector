add_executable(visualizer_demo
  visualizer_demo.c
)
target_link_libraries(visualizer_demo simple_gc)
target_include_directories(visualizer_demo PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

add_executable(stack_scan_demo
  stack_scan_demo.c
)

target_link_libraries(stack_scan_demo simple_gc)
target_include_directories(stack_scan_demo PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(visualizer_demo PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
