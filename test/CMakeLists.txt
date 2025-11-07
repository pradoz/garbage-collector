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

# compaction
add_executable(test_compaction
  test_compaction.c
  munit/munit.c
)
target_link_libraries(test_compaction simple_gc gc_types)
target_include_directories(test_compaction PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_compaction COMMAND test_compaction)

# memory pressure
add_executable(test_memory_pressure
  test_memory_pressure.c
  munit/munit.c
)
target_link_libraries(test_memory_pressure simple_gc)
target_include_directories(test_memory_pressure PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_memory_pressure COMMAND test_memory_pressure)

# gc_large tests
add_executable(test_gc_large
  test_gc_large.c
  munit/munit.c
)
target_link_libraries(test_gc_large simple_gc gc_large gc_types)
target_include_directories(test_gc_large PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_large COMMAND test_gc_large)

# gc_mark tests
add_executable(test_gc_mark
  test_gc_mark.c
  munit/munit.c
)
target_link_libraries(test_gc_mark simple_gc gc_mark gc_types)
target_include_directories(test_gc_mark PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_mark COMMAND test_gc_mark)

# gc_sweep tests
add_executable(test_gc_sweep
  test_gc_sweep.c
  munit/munit.c
)
target_link_libraries(test_gc_sweep simple_gc gc_sweep gc_mark gc_types)
target_include_directories(test_gc_sweep PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_sweep COMMAND test_gc_sweep)

# gc_generational tests
add_executable(test_gc_generational
  test_generational.c
  munit/munit.c
)
target_link_libraries(test_gc_generational simple_gc gc_sweep gc_mark gc_types)
target_include_directories(test_gc_generational PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_generational COMMAND test_gc_generational)

# gc_trace tests
add_executable(test_gc_trace
  test_trace.c
  munit/munit.c
)
target_link_libraries(test_gc_trace simple_gc gc_sweep gc_mark gc_types)
target_include_directories(test_gc_trace PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_trace COMMAND test_gc_trace)

# gc_debug tests
add_executable(test_gc_debug
  test_debug.c
  munit/munit.c
)
target_link_libraries(test_gc_debug simple_gc gc_sweep gc_mark gc_types)
target_include_directories(test_gc_debug PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  munit
)
add_test(NAME test_gc_debug COMMAND test_gc_debug)
