#ifndef GC_POOL_H
#define GC_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "gc_types.h"


#define GC_NUM_SIZE_CLASSES 6
#define GC_POOL_BLOCK_SIZE 4096

extern const size_t GC_SIZE_CLASS_SIZES[GC_NUM_SIZE_CLASSES];


typedef struct free_node {
  struct free_node *next;
} free_node_t;

typedef struct pool_block {
  void *memory;
  size_t slot_size;
  size_t capacity;
  size_t used;
  free_node_t *free_list;
  struct pool_block *next;
} pool_block_t;

typedef struct size_class {
  size_t size;           // object size (excluding header)
  size_t slot_size;      // total slot size (header + object)
  pool_block_t *blocks;
  size_t total_capacity;
  size_t total_used;
  size_t total_allocated;
} size_class_t;


// pool management
int gc_pool_size_to_class(size_t size);
size_class_t* gc_pool_get_size_class(size_class_t *classes, size_t size);

// block management
pool_block_t* gc_pool_create_block(size_t slot_size, size_t capacity);
void gc_pool_free_block(pool_block_t *block);
bool gc_pool_pointer_in_block(pool_block_t *block, void *ptr);

// allocation/freeing
void* gc_pool_alloc_from_block(pool_block_t *block, obj_type_t type, size_t size);
void* gc_pool_alloc_from_size_class(size_class_t *sc, obj_type_t type, size_t size);
void gc_pool_free_to_block(pool_block_t *block, size_class_t *sc, obj_header_t *header);

// size class management
bool gc_pool_init_size_class(size_class_t *sc, size_t object_size);
void gc_pool_destroy_size_class(size_class_t *sc);
bool gc_pool_init_all_classes(size_class_t *classes);
void gc_pool_destroy_all_classes(size_class_t *classes);

// statistics
size_t gc_pool_count_blocks(size_class_t *sc);
float gc_pool_utilization(size_class_t *sc);
size_t gc_pool_fragmented_bytes(size_class_t *sc);

#endif /* GC_POOL_H */
