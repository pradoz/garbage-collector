# base garbage collector
add_executable(test_simple_gc
  test_simple_gc.c
  munit/munit.c
)
target_link_libraries(test_simple_gc simple_gc)
target_include_directories(test_simple_gc PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_simple_gc COMMAND test_simple_gc)

# visualizer
add_executable(test_visualizer
  test_visualizer.c
  munit/munit.c
)

target_link_libraries(test_visualizer simple_gc)
target_include_directories(test_visualizer PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_visualizer COMMAND test_visualizer)
set_tests_properties(test_visualizer PROPERTIES
  ENVIRONMENT "MUNIT_OUTPUT_CAPTURE=1"
)

# stack scanning
add_executable(test_stack_scan
  test_stack_scan.c
  munit/munit.c
)
target_link_libraries(test_stack_scan simple_gc)
target_include_directories(test_stack_scan PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_stack_scan COMMAND test_stack_scan)

# memory pools
add_executable(test_memory_pools
  test_memory_pools.c
  munit/munit.c
)
target_link_libraries(test_memory_pools simple_gc)
target_include_directories(test_memory_pools PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_memory_pools COMMAND test_memory_pools)
