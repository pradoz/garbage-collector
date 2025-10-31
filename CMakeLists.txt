cmake_minimum_required(VERSION 3.10)
project(simple_gc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)

add_library(simple_gc
  src/simple_gc.c
  src/gc_platform.c
  src/gc_visualizer.c
)

target_include_directories(simple_gc PUBLIC include)

target_link_libraries(simple_gc PRIVATE Threads::Threads)

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
