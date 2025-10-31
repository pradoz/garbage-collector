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
