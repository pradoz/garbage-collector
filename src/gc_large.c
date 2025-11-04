#include "gc_large.h"
#include <stdlib.h>
#include <sys/mman.h>


large_block_t* gc_large_create_block(obj_type_t type, size_t size) {
  if (size <= GC_LARGE_OBJECT_THRESHOLD || size >= GC_HUGE_OBJECT_THRESHOLD) {
    return NULL;
  }

  size_t total_size = sizeof(obj_header_t) + size;
  void* memory = malloc(total_size);
  if (!memory) return NULL;

  large_block_t *block = (large_block_t*) malloc(sizeof(large_block_t));
  if (!block) {
    free(memory);
    return NULL;
  }

  block->memory = memory;
  block->size = size;
  block->in_use = true;
  block->header = (obj_header_t*) memory;
  block->next = NULL;

  if (!gc_init_header(block->header, type, size)) {
    free(memory);
    free(block);
    return NULL;
  }

  return block;
}

void gc_large_free_block(large_block_t *block) {
  if (!block) return;

  if (block->memory) {
    free(block->memory);
  }
  free(block);
}

large_block_t* gc_large_find_best_fit(large_block_t *blocks, size_t size) {
  if (!blocks) return NULL;

  large_block_t *best_fit = NULL;
  size_t best_fit_waste = GC_SIZE_MAX;

  large_block_t *curr = blocks;
  while (curr) {
    if (!curr->in_use && curr->size >= size) {
      size_t waste = curr->size - size;
      if (waste < best_fit_waste) {
        best_fit = curr;
        best_fit_waste = waste;
      }
    }
    curr = curr->next;
  }

  return best_fit;
}

void* gc_large_alloc(large_block_t **blocks, size_t *block_count, obj_type_t type, size_t size) {
  if (!blocks || !block_count) return NULL;
  if (size <= GC_LARGE_OBJECT_THRESHOLD || size >= GC_HUGE_OBJECT_THRESHOLD) {
    return NULL;
  }

  // look for a free block that fits the size
  large_block_t *best_fit = gc_large_find_best_fit(*blocks, size);

  if (best_fit) { // try to reuse existing block
    best_fit->in_use = true;

    obj_header_t *header = best_fit->header;
    if (!gc_init_header(header, type, size)) {
      best_fit->in_use = false;
      return NULL;
    }
    return (void*)(header + 1);
  }

  // otherwise, try to allocate a new block
  large_block_t *new_block = gc_large_create_block(type, size);
  if (!new_block) return NULL;

  // add to list
  new_block->next = *blocks;
  *blocks = new_block;
  (*block_count)++;

  return (void*)(new_block->header + 1);
}

obj_header_t* gc_large_find_header(large_block_t *blocks, void *ptr) {
  if (!blocks || !ptr) return NULL;

  large_block_t *curr = blocks;
  while (curr) {
    void *found = (void*) (curr->header + 1);
    if (found == ptr) return curr->header;
    curr = curr->next;
  }

  return NULL;
}

void gc_large_destroy_all(large_block_t *blocks) {
  large_block_t *curr = blocks;
  while (curr) {
    large_block_t *next = curr->next;
    gc_large_free_block(curr);
    curr = next;
  }
}

size_t gc_large_count_blocks(large_block_t *blocks) {
  size_t count = 0;
  large_block_t *curr = blocks;
  while (curr) {
    count++;
    curr = curr->next;
  }
  return count;
}

size_t gc_large_count_in_use(large_block_t *blocks) {
  size_t count = 0;
  large_block_t *curr = blocks;
  while (curr) {
    if (curr->in_use) count++;
    curr = curr->next;
  }
  return count;
}

size_t gc_large_total_memory(large_block_t *blocks) {
  size_t total = 0;
  large_block_t *curr = blocks;
  while (curr) {
    if (curr->in_use) {
      total += sizeof(obj_header_t) + curr->size;
    }
    curr = curr->next;
  }
  return total;
}

huge_object_t* gc_huge_create_object(obj_type_t type, size_t size) {
  if (size < GC_HUGE_OBJECT_THRESHOLD) return NULL;

  size_t total_size = sizeof(obj_header_t) + size;

  // round up to page size
  size_t page_size = 4096;
  size_t pages = (total_size + page_size - 1) / page_size;
  size_t alloc_size = pages * page_size;

  void *memory = mmap(NULL, alloc_size,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS,
      -1, 0);

  if (memory == MAP_FAILED) return NULL;

  huge_object_t *huge = (huge_object_t*) malloc(sizeof(huge_object_t));
  if (!huge) {
    munmap(memory, alloc_size);
    return NULL;
  }

  huge->memory = memory;
  huge->size = alloc_size;
  huge->header = (obj_header_t*) memory;
  huge->next = NULL;

  if (!gc_init_header(huge->header, type, size)) {
    munmap(memory, alloc_size);
    free(huge);
    return NULL;
  }

  return huge;
}

void gc_huge_free_object(huge_object_t *huge) {
  if (!huge) return;

  if (huge->memory) {
    munmap(huge->memory, huge->size);
  }
  free(huge);
}

void* gc_huge_alloc(huge_object_t **objects, size_t *object_count, obj_type_t type, size_t size) {
  if (!objects || !object_count) return NULL;
  if (size < GC_HUGE_OBJECT_THRESHOLD) return NULL;

  huge_object_t *huge = gc_huge_create_object(type, size);
  if (!huge) return NULL;

  // add to list
  huge->next = *objects;
  *objects = huge;
  (*object_count)++;

  return (void*)(huge->header + 1);
}

obj_header_t* gc_huge_find_header(huge_object_t *objects, void *ptr) {
  if (!objects || !ptr) return NULL;

  huge_object_t *curr = objects;
  while (curr) {
    void *found = (void*) (curr->header + 1);
    if (found == ptr) return curr->header;
    curr = curr->next;
  }

  return NULL;
}

void gc_huge_destroy_all(huge_object_t *objects) {
  huge_object_t *curr = objects;
  while (curr) {
    huge_object_t *next = curr->next;
    gc_huge_free_object(curr);
    curr = next;
  }
}

size_t gc_huge_count_objects(huge_object_t *objects) {
  size_t count = 0;
  huge_object_t *curr = objects;
  while (curr) {
    count++;
    curr = curr->next;
  }
  return count;
}

size_t gc_huge_total_memory(huge_object_t *objects) {
  size_t total = 0;
  huge_object_t *curr = objects;
  while (curr) {
    total += curr->size;
    curr = curr->next;
  }
  return total;
}
