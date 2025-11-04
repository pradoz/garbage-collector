#include "gc_pool.h"
#include "simple_gc.h"
#include <stdlib.h>
#include <string.h>


const size_t GC_SIZE_CLASS_SIZES[GC_NUM_SIZE_CLASSES] = {
  8,    // booleans, small numbers
  16,   // pointers, small structs
  32,   // small-medium
  64,   // medium
  128,  // medium-large
  256   // large
};


int gc_pool_size_to_class(size_t size) {
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    if (size <= GC_SIZE_CLASS_SIZES[i]) {
      return i;
    }
  }
  return -1; // too big for the pool
}

size_class_t* gc_pool_get_size_class(size_class_t *classes, size_t size) {
  if (!classes) return NULL;

  int class_index = gc_pool_size_to_class(size);
  if (class_index < 0) {
    return NULL;
  }
  return &classes[class_index];
}

pool_block_t* gc_pool_create_block(size_t slot_size, size_t capacity) {
  if (slot_size == 0 || capacity == 0) return NULL;

  pool_block_t* block = (pool_block_t*) malloc(sizeof(pool_block_t));
  if (!block) return NULL;

  size_t total_size = slot_size * capacity;
  block->memory = malloc(total_size);
  if (!block->memory) {
    free(block);
    return NULL;
  }

  block->slot_size = slot_size;
  block->capacity = capacity;
  block->used = 0;
  block->next = NULL;

  // initialize free list
  block->free_list = (free_node_t*) block->memory;
  char *slot = (char*) block->memory;
  for (size_t i = 0; i < capacity - 1; ++i) {
    free_node_t *node = (free_node_t*) slot;
    slot += slot_size;
    node->next = (free_node_t*) slot;
  }

  free_node_t *last = (free_node_t*) slot;
  last->next = NULL;

  return block;
}

void gc_pool_free_block(pool_block_t *block) {
  if (!block) return;

  if (block->memory) {
    free(block->memory);
  }
  free(block);
}

bool gc_pool_pointer_in_block(pool_block_t *block, void *ptr) {
  if (!block || !ptr) return false;

  char *start = block->memory;
  char *end = start + (block->slot_size * block->capacity);
  char *p = (char*) ptr;
  return (p >= start && p < end);
}

void* gc_pool_alloc_from_block(pool_block_t *block, obj_type_t type, size_t size) {
  if (!block || !block->free_list) return NULL;

  // pop from free list
  free_node_t *node = block->free_list;
  block->free_list = node->next;
  block->used++;

  // node becomes the new object header
  obj_header_t *header = (obj_header_t*) node;
  if (!gc_init_header(header, type, size)) {
    // rollback allocation
    node->next = block->free_list;
    block->free_list = node;
    block->used--;
    return NULL;
  }

  return (void*)(header + 1);
}

void* gc_pool_alloc_from_size_class(size_class_t *sc, obj_type_t type, size_t size) {
  if (!sc) return NULL;

  // try to allocate from existing blocks
  pool_block_t *block = sc->blocks;
  while (block) {
    if (block->free_list) {
      void *ptr = gc_pool_alloc_from_block(block, type, size);
      if (ptr) {
        sc->total_used++;
        sc->total_allocated++;
        return ptr;
      }
    }
    block = block->next;
  }

  // no space in existing blocks; create a new block
  size_t slots_per_block = GC_POOL_BLOCK_SIZE / sc->slot_size;
  if (slots_per_block < 1) {
    slots_per_block = 1;
  }

  pool_block_t *new_block = gc_pool_create_block(sc->slot_size, slots_per_block);
  if (!new_block) return NULL;

  // add to size class
  new_block->next = sc->blocks;
  sc->blocks = new_block;
  sc->total_capacity += new_block->capacity;

  // allocate from new block
  void *ptr = gc_pool_alloc_from_block(new_block, type, size);
  if (ptr) {
    sc->total_used++;
    sc->total_allocated++;
  }

  return ptr;
}

void gc_pool_free_to_block(pool_block_t *block, size_class_t *sc, obj_header_t *header) {
  if (!block || !sc || !header) return;

  free_node_t *node = (free_node_t*) header;
  node->next = block->free_list;
  block->free_list = node;
  block->used--;
  sc->total_used--;
}

bool gc_pool_init_size_class(size_class_t *sc, size_t object_size) {
  if (!sc) return false;

  sc->size = object_size;
  sc->slot_size = sizeof(obj_header_t) + object_size;
  sc->blocks = NULL;
  sc->total_capacity = 0;
  sc->total_used = 0;
  sc->total_allocated = 0;

  return true;
}

void gc_pool_destroy_size_class(size_class_t *sc) {
  if (!sc) return;

  pool_block_t *block = sc->blocks;
  while (block) {
    pool_block_t *next = block->next;
    gc_pool_free_block(block);
    block = next;
  }

  sc->blocks = NULL;
  sc->total_capacity = 0;
  sc->total_used = 0;
}

bool gc_pool_init_all_classes(size_class_t *classes) {
  if (!classes) return false;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    if (!gc_pool_init_size_class(&classes[i], GC_SIZE_CLASS_SIZES[i])) {
      // cleanup on failure
      for (int j = 0; j < i; ++j) {
        gc_pool_destroy_size_class(&classes[j]);
      }
      return false;
    }
  }

  return true;
}

void gc_pool_destroy_all_classes(size_class_t *classes) {
  if (!classes) return;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    gc_pool_destroy_size_class(&classes[i]);
  }
}

// statistics
size_t gc_pool_count_blocks(size_class_t *sc) {
  if (!sc) return 0;

  size_t count = 0;
  pool_block_t *block = sc->blocks;
  while (block) {
    count++;
    block = block->next;
  }
  return count;
}

float gc_pool_utilization(size_class_t *sc) {
  if (!sc || sc->total_capacity == 0) return 0.0f;
  return (float)sc->total_used / (float)sc->total_capacity;
}

size_t gc_pool_fragmented_bytes(size_class_t *sc) {
  if (!sc) return 0;

  size_t capacity_bytes = sc->total_capacity * sc->slot_size;
  size_t used_bytes = sc->total_used * sc->slot_size;
  return capacity_bytes - used_bytes;
}
