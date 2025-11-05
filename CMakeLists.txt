cmake_minimum_required(VERSION 3.10)
project(simple_gc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_definitions(_POSIX_C_SOURCE=199309L)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)

# create an INTERFACE library for common includes
add_library(gc_common INTERFACE)
target_include_directories(gc_common INTERFACE include)

# core types library (no dependencies)
add_library(gc_types OBJECT src/gc_types.c)
target_link_libraries(gc_types PUBLIC gc_common)

# pool library (depends only on types)
add_library(gc_pool OBJECT src/gc_pool.c)
target_link_libraries(gc_pool PUBLIC gc_common)

# large blocks library (depends only on types)
add_library(gc_large OBJECT src/gc_large.c)
target_compile_definitions(gc_large PRIVATE _GNU_SOURCE)
target_link_libraries(gc_large PUBLIC gc_common)

# mark library (depends on types, pool, large)
add_library(gc_mark OBJECT src/gc_mark.c)
target_link_libraries(gc_mark PUBLIC gc_common)

# sweep library (depends on types, pool, large)
add_library(gc_sweep OBJECT src/gc_sweep.c)
target_link_libraries(gc_sweep PUBLIC gc_common)

# trace library (depends on types, pool)
add_library(gc_trace OBJECT src/gc_trace.c)
target_link_libraries(gc_trace PUBLIC gc_common)

# debug library (depends on types)
add_library(gc_debug OBJECT src/gc_debug.c)
target_compile_definitions(gc_debug PRIVATE _GNU_SOURCE)
target_link_libraries(gc_debug PUBLIC gc_common)

# platform library
add_library(gc_platform OBJECT src/gc_platform.c)
target_compile_definitions(gc_platform PRIVATE _GNU_SOURCE)
target_link_libraries(gc_platform PUBLIC gc_common)

# visualizer library
add_library(gc_visualizer OBJECT src/gc_visualizer.c)
target_link_libraries(gc_visualizer PUBLIC gc_common)

# main GC library - combines all object libraries
add_library(simple_gc
  src/simple_gc.c
  $<TARGET_OBJECTS:gc_types>
  $<TARGET_OBJECTS:gc_pool>
  $<TARGET_OBJECTS:gc_large>
  $<TARGET_OBJECTS:gc_mark>
  $<TARGET_OBJECTS:gc_sweep>
  $<TARGET_OBJECTS:gc_trace>
  $<TARGET_OBJECTS:gc_debug>
  $<TARGET_OBJECTS:gc_platform>
  $<TARGET_OBJECTS:gc_visualizer>
)
target_link_libraries(simple_gc PUBLIC gc_common Threads::Threads)
target_compile_definitions(simple_gc PRIVATE _GNU_SOURCE)

enable_testing()
add_subdirectory(test)
add_subdirectory(examples)

add_custom_target(example
  COMMAND visualizer_demo
  DEPENDS visualizer_demo
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/examples
  COMMENT "Running visualizer demo..."
)
