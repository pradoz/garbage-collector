#ifndef GC_LARGE_H
#define GC_LARGE_H

#include <stddef.h>
#include <stdbool.h>
#include "gc_types.h"


#define GC_LARGE_OBJECT_THRESHOLD 256
#define GC_HUGE_OBJECT_THRESHOLD 4096
#define GC_SIZE_MAX 1024 * 1024 * 5  // 5 MB


// large block structure (256 bytes - 4KB objects)
typedef struct large_block {
  void *memory;
  size_t size;
  bool in_use;
  obj_header_t *header;
  struct large_block *next;
} large_block_t;

// huge object structure (>4KB objects, uses mmap)
typedef struct huge_object {
  void *memory;
  size_t size;
  obj_header_t *header;
  struct huge_object *next;
} huge_object_t;


// large block management
large_block_t* gc_large_create_block(obj_type_t type, size_t size);
void gc_large_free_block(large_block_t *block);
large_block_t* gc_large_find_best_fit(large_block_t *blocks, size_t size);
void* gc_large_alloc(large_block_t **blocks, size_t *block_count, obj_type_t type, size_t size);

// huge object management
huge_object_t* gc_huge_create_object(obj_type_t type, size_t size);
void gc_huge_free_object(huge_object_t *huge);
void* gc_huge_alloc(huge_object_t **objects, size_t *object_count, obj_type_t type, size_t size);

obj_header_t* gc_large_find_header(large_block_t *blocks, void *ptr);
obj_header_t* gc_huge_find_header(huge_object_t *objects, void *ptr);

void gc_large_destroy_all(large_block_t *blocks);
void gc_huge_destroy_all(huge_object_t *objects);

// statistics
size_t gc_large_count_blocks(large_block_t *blocks);
size_t gc_large_count_in_use(large_block_t *blocks);
size_t gc_large_total_memory(large_block_t *blocks);
size_t gc_huge_count_objects(huge_object_t *objects);
size_t gc_huge_total_memory(huge_object_t *objects);

#endif /* GC_LARGE_H */
