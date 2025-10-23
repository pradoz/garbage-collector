add_executable(test_simple_gc
  munit/munit.c
  test_simple_gc.c
)

target_include_directories(test_simple_gc PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/munit
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_simple_gc simple_gc)
add_test(NAME test_simple_gc COMMAND test_simple_gc)
