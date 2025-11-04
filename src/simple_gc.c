#include "simple_gc.h"
#include "gc_platform.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>


// object sizes (excluding header)
const size_t GC_SIZE_CLASS_SIZES[GC_NUM_SIZE_CLASSES] = {
  8,    // booleans, small numbers
  16,   // pointers, small structs
  32,   // sh-medium
  64,   // medium
  128,  // medium-large
  256   // large (beyond this = large object)
};

int gc_size_to_class(size_t size) {
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    if (size <= GC_SIZE_CLASS_SIZES[i]) {
      return i;
    }
  }
  return -1; // too big for the pool
}

size_class_t* gc_get_size_class(gc_t *gc, size_t size) {
  int class_index = gc_size_to_class(size);
  if (class_index < 0) {
    return NULL;
  }
  return &gc->size_classes[class_index];
}

pool_block_t* gc_create_pool_block(size_t slot_size, size_t capacity) {
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

  /* Memory layout (e.g., 4 slots of 32 bytes each):
    when slot_size=32, capacity=4; block->memory:
    addr:   0x1000    0x1020    0x1040    0x1060
          |─────────┬─────────┬─────────┬──────────┐
          | Slot 0  │ Slot 1  │ Slot 2  │ Slot 3   │
          | next─┐  │ next─┐  │ next─┐  │ next=NULL│
          |──────v──┴─|────v──┴─|────v──┴─|────────┘
                 └────^    └────^    └────^
  */
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

void* gc_alloc_from_block(pool_block_t *block, obj_type_t type, size_t size) {
  if (!block || !block->free_list) return NULL;

  // pop from free list
  free_node_t *node = block->free_list;
  block->free_list = node->next;
  block->used++;

  // node becomes the new object header
  obj_header_t *header = (obj_header_t*) node;
  if (!simple_gc_init_header(header, type, size)) {
    node->next = block->free_list;
    block->free_list = node;
    block->used--;
    return NULL;
  }

  return (void*)(header + 1);
}

void* gc_alloc_from_size_class(gc_t *gc, size_class_t* sc, obj_type_t type, size_t size) {
  if (!gc || !sc) return NULL;

  // try to allocate from existing blocks
  pool_block_t *block = sc->blocks;
  while (block) {
    if (block->free_list) {
      void *ptr = gc_alloc_from_block(block, type, size);
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

  pool_block_t *new_block = gc_create_pool_block(sc->slot_size, slots_per_block);
  if (!new_block) return NULL;

  // add to size class and allocate from new block
  new_block->next = sc->blocks;
  sc->blocks = new_block;
  sc->total_capacity += new_block->capacity;

  void *ptr = gc_alloc_from_block(new_block, type, size);
  if (ptr) {
    sc->total_used++;
    sc->total_allocated++;
  }

  return ptr;
}

void* gc_alloc_large_object(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc
      || size <= GC_LARGE_OBJECT_THRESHOLD
      || size >= GC_HUGE_OBJECT_THRESHOLD) return NULL;

  // look for a free block that fits the size
  large_block_t *block = gc->large_blocks;
  large_block_t *best_fit = NULL;
  size_t best_fit_waste = GC_SIZE_MAX;

  while (block) {
    if (!block->in_use && block->size >= size) {
      size_t waste = block->size - size;
      if (waste < best_fit_waste) {
        best_fit = block;
        best_fit_waste = waste;
      }
    }
    block = block->next;
  }

  if (best_fit) { // try to reuse existing block
    best_fit->in_use = true;

    obj_header_t *header = best_fit->header;
    if (!simple_gc_init_header(header, type, size)) {
      best_fit->in_use = false;
      return NULL;
    }
    return (void*)(header + 1);
  }

  // otherwise, try to allocate a new block
  size_t total_size = sizeof(obj_header_t) + size;
  void* memory = malloc(total_size);
  if (!memory) return NULL;

  large_block_t *new_block = (large_block_t*) malloc(sizeof(large_block_t));
  if (!new_block) {
    free(memory);
    return NULL;
  }

  new_block->memory = memory;
  new_block->size = size;
  new_block->in_use = true;
  new_block->header = (obj_header_t*) memory;
  new_block->next = gc->large_blocks;

  gc->large_blocks = new_block;
  gc->large_block_count++;

  if (!simple_gc_init_header(new_block->header, type, size)) {
    free(memory);
    free(new_block);
    return NULL;
  }

  return (void*)(new_block->header + 1);
}

void* gc_alloc_huge_object(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || size < GC_HUGE_OBJECT_THRESHOLD) return NULL;

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
  huge->next = gc->huge_objects;

  gc->huge_objects = huge;
  gc->huge_object_count++;

  if (!simple_gc_init_header(huge->header, type, size)) {
    munmap(memory, alloc_size);
    free(huge);
    return NULL;
  }

  return (void*)(huge->header + 1);
}

void* gc_alloc_large(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || size <= GC_LARGE_OBJECT_THRESHOLD) return NULL;

  if (size >= GC_HUGE_OBJECT_THRESHOLD) {
    return gc_alloc_huge_object(gc, type, size);
  }

  // if we get here, its a large object
  return gc_alloc_large_object(gc, type, size);
}


static void update_heap_bounds(gc_t *gc, void *ptr, size_t size) {
  if (!gc || !ptr) return;

  void *start = ptr;
  void *end = (void*) ((char*) ptr + size);

  if (!gc->heap_start || start < gc->heap_start) {
    gc->heap_start = start;
  }
  if (!gc->heap_end || end > gc->heap_end) {
    gc->heap_end = end;
  }
}


const char *simple_gc_version(void) {
  static char version_string[32]; // Buffer size large enough for "M.m.p"

  snprintf(version_string, sizeof(version_string), "%d.%d.%d",
      SIMPLE_GC_VERSION_MAJOR, SIMPLE_GC_VERSION_MINOR, SIMPLE_GC_VERSION_PATCH);

  return version_string;
}

bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size) {
  if (!header || size == 0) {
    return false;
  }

  header->type = type;
  header->size = size;
  header->marked = false;
  header->next = NULL;
  return true;
}

bool simple_gc_is_valid_header(const obj_header_t *header) {
  if (!header) {
    return false;
  }

  if (header->type < OBJ_TYPE_UNKNOWN || header->type > OBJ_TYPE_STRUCT) {
    return false;
  }

  if (header->size == 0) {
    return false;
  }
  return true;
}

gc_t* simple_gc_new(size_t init_capacity) {
  if (init_capacity == 0) {
    return NULL;
  }

  gc_t* gc = (gc_t*) malloc(sizeof(gc_t));
  if (!gc) {
    // free(gc);
    return NULL;
  }

  if (!simple_gc_init(gc, init_capacity)) {
    free(gc);
    return NULL;
  }

  return gc;
}

gc_t *simple_gc_new_auto(size_t init_capacity) {
  gc_t *gc = simple_gc_new(init_capacity);
  if (!gc) {
    return NULL;
  }

  if (!simple_gc_auto_init_stack(gc)) {
    simple_gc_destroy(gc);
    free(gc);
    return NULL;
  }

  return gc;
}

bool gc_init_size_class(size_class_t *sc, size_t object_size) {
  if (!sc) return false;

  // initialize a single size class
  sc->size = object_size;
  sc->slot_size = sizeof(obj_header_t) + object_size;
  sc->blocks = NULL;
  sc->total_capacity = 0;
  sc->total_used = 0;
  sc->total_allocated = 0;

  return true;
}

bool gc_init_pools(gc_t *gc) {
  if (!gc) return false;

  // initialize all size classes
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    if (!gc_init_size_class(&gc->size_classes[i], GC_SIZE_CLASS_SIZES[i])) {
      return false;
    }
  }

  gc->use_pools = true;
  gc->large_blocks = NULL;
  gc->large_block_count = 0;
  gc->huge_objects = NULL;
  gc->huge_object_count = 0;

  return true;
}

bool simple_gc_init(gc_t* gc, size_t init_capacity) {
  if (!gc || init_capacity == 0) {
    return false;
  }

  // GC context
  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
  gc->heap_capacity = init_capacity;

  // roots
  gc->root_capacity = 16;
  gc->root_count = 0;
  gc->roots = (void**) malloc(sizeof(void*) * gc->root_capacity);

  if (!gc->roots) {
    return false;
  }

  // refs
  gc->references = NULL;

  // stack scanning
  gc->stack_bottom = NULL;
  gc->auto_root_scan_enabled = false;

  // heap bounds
  gc->heap_start = NULL;
  gc->heap_end = NULL;

  // memory pools
  if (!gc_init_pools(gc)) {
    free(gc->roots);
    return false;
  }

  // stats
  gc->total_allocations = 0;
  gc->total_collections = 0;
  gc->total_bytes_allocated = 0;
  gc->total_bytes_freed = 0;

  return true;
}

void gc_free_pool_block(pool_block_t *block) {
  if (!block) return;

  if (block->memory) {
    free(block->memory);
  }
  free(block);
}

void gc_destroy_size_class(size_class_t *sc) {
  if (!sc) return;

  pool_block_t *block = sc->blocks;
  while (block) {
    pool_block_t *old = block;
    block = block->next;
    gc_free_pool_block(old);
    // pool_block_t *next = block->next;
    // gc_free_pool_block(block);
    // block = next;
  }

  sc->blocks = NULL;
  sc->total_capacity = 0;
  sc->total_used = 0;
}

void gc_destroy_pools(gc_t* gc) {
  if (!gc) return;
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    gc_destroy_size_class(&gc->size_classes[i]);
  }

  // free large blocks
  large_block_t *block = gc->large_blocks;
  while (block) {
    large_block_t *next = block->next;
    if (block->memory) free(block->memory);
    free(block);
    block = next;
  }

  // free huge objects
  huge_object_t *huge = gc->huge_objects;
  while (huge) {
    huge_object_t  *next = huge->next;
    if (huge->memory) munmap(huge->memory, huge->size);
    free(huge);
    huge = next;
  }

  // reset bookkeeping
  gc->large_blocks = NULL;
  gc->large_block_count = 0;
  gc->huge_objects = NULL;
  gc->huge_object_count = 0;
}

void simple_gc_destroy(gc_t* gc) {
  if (!gc) {
    return;
  }

  // free memory pools
  if (gc->use_pools) {
    gc_destroy_pools(gc);
  }

  // free gc objects
  obj_header_t* obj = gc->objects;
  while (obj) {
    obj_header_t* tmp = obj;
    obj = obj->next;
    free(tmp);
  }

  // free references
  ref_node_t* ref = gc->references;
  while (ref) {
    ref_node_t* tmp = ref;
    ref = ref->next;
    free(tmp);
  }

  // reset context
  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
  free(gc->roots);
  gc->roots = NULL;
  gc->root_count = 0;
  gc->root_capacity = 0;
  gc->references = NULL;
}

size_t simple_gc_object_count(const gc_t* gc) {
  return gc ? gc->object_count : 0;
}

size_t simple_gc_heap_capacity(const gc_t* gc) {
  return gc ? gc->heap_capacity : 0;
}

size_t simple_gc_heap_used(const gc_t *gc) {
  return gc ? gc->heap_used : 0;
}

void *simple_gc_alloc(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || size == 0) {
    return NULL;
  }

  // capacity check
  size_t total_size = sizeof(obj_header_t) + size;
  if (total_size + gc->heap_used > gc->heap_capacity) {
    return NULL;
  }

  void *result = NULL;

  if (gc->use_pools) {
    size_class_t *sc = gc_get_size_class(gc, size);
    if (sc) {
      result = gc_alloc_from_size_class(gc, sc, type, size);
    } else {
      result = gc_alloc_large(gc, type, size);
    }
  } else {  // fall back to malloc-based allocation
    // allocate total memory for object+header
    obj_header_t* header = (obj_header_t*) malloc(total_size);
    if (!header) {
      return NULL;

    }
    // verify we can initialize the header
    if (!simple_gc_init_header(header, type, size)) {
      free(header);
      return NULL;
    }

    header->next = gc->objects;
    gc->objects = header;
    result = (void*)(header + 1);
  }


  // bookkeeping
  if (result) {
    gc->object_count++;
    gc->heap_used += total_size;
    update_heap_bounds(gc, result, size);

    // stats
    gc->total_allocations++;
    gc->total_bytes_allocated += total_size;
  }
  return result;
}

static obj_header_t *gc_find_header_in_large_objects(gc_t *gc, void* ptr) {
  if (!gc || !ptr) return NULL;

  large_block_t *block = gc->large_blocks;
  while (block) {
    void *found = (void*) (block->header + 1);
    if (found == ptr) return block->header;
    block = block->next;
  }

  return NULL;
}

static obj_header_t *gc_find_header_in_huge_objects(gc_t *gc, void* ptr) {
  if (!gc || !ptr) return NULL;

  huge_object_t *huge = gc->huge_objects;
  while (huge) {
    void *found = (void*) (huge->header + 1);
    if (found == ptr) return huge->header;
    huge = huge->next;
  }

  return NULL;
}

obj_header_t *simple_gc_find_header(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return NULL;

  // check pools first
  if (gc->use_pools) {
    obj_header_t* header = gc_find_header_in_pools(gc, ptr);
    if (header) return header;

    header = gc_find_header_in_large_objects(gc, ptr);
    if (header) return header;

    header = gc_find_header_in_huge_objects(gc, ptr);
    if (header) return header;
  }

  // fallback to legacy (non-pool) object management
  obj_header_t* curr = gc->objects;
  while (curr) {
    void* found = (void*)(curr + 1);
    if (found == ptr) {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}

bool simple_gc_add_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  obj_header_t* header = simple_gc_find_header(gc, ptr);
  if (!header) {
    return false;
  }

  // check if we need to resize
  if (gc->root_count >= gc->root_capacity) {
    size_t new_capacity = gc->root_capacity * 2 + 1;
    void** new_roots = (void**) realloc(gc->roots, new_capacity * sizeof(void*));
    if (!new_roots) {
      return false;
    }

    gc->roots = new_roots;
    gc->root_capacity = new_capacity;
  }

  gc->roots[gc->root_count++] = ptr;

  return true;
}

bool simple_gc_remove_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  // find the root
  for (size_t i = 0; i < gc->root_count; ++i) {
    if (gc->roots[i] == ptr) {
      // shift all elements after, overwriting what we want to remove
      for (size_t j = i; j < gc->root_count - 1; ++j) {
        gc->roots[i] = gc->roots[j + 1];
      }
      // update and return found
      gc->root_count--;
      return true;
    }
  }
  return false;  // not found
}

bool simple_gc_is_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  for (size_t i = 0; i < gc->root_count; ++i) {
    if (gc->roots[i] == ptr) {
      return true;
    }
  }

  return false;
}

void simple_gc_mark(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return;
  }

  obj_header_t* header = simple_gc_find_header(gc, ptr);
  if (!header || header->marked) {
    return;
  }

  // object is reachable
  header->marked = true;

  ref_node_t* ref = gc->references;
  while (ref) {
    if (ref->from_obj == ptr) {
      simple_gc_mark(gc, ref->to_obj);
    }
    ref = ref->next;
  }
}

void simple_gc_mark_roots(gc_t *gc) {
  if (!gc) {
    return;
  }

  for (size_t i = 0; i < gc->root_count; ++i) {
    simple_gc_mark(gc, gc->roots[i]);
  }
}

void simple_gc_sweep(gc_t *gc) {
  if (!gc) return;

  if (gc->use_pools) {
    gc_sweep_pools(gc);
    gc_sweep_large_objects(gc);
    gc_sweep_huge_objects(gc);
    return;  // exit early
  }

  // legacy sweep for non-pool mode
  obj_header_t** curr = &gc->objects;
  while (*curr) {
    if (!(*curr)->marked) { // unreachable
      obj_header_t* tmp = *curr;
      *curr = (*curr)->next;

      gc->object_count--;
      gc->heap_used -= (sizeof(obj_header_t) + tmp->size);

      free(tmp);
    } else {
      (*curr)->marked = false;
      curr = &(*curr)->next;
    }
  }
}

void simple_gc_collect(gc_t *gc) {
  if (!gc) {
    return;
  }

  // stats
  gc->total_collections++;

  simple_gc_mark_roots(gc);

  // automated root scanning
  if (gc->auto_root_scan_enabled) {
    simple_gc_scan_stack(gc);
  }

  simple_gc_sweep(gc);
}

bool simple_gc_add_reference(gc_t *gc, void *from_ptr, void *to_ptr) {
  if (!gc || !from_ptr || !to_ptr) {
    return false;
  }

  if (!simple_gc_find_header(gc, from_ptr) || !simple_gc_find_header(gc, to_ptr)) {
    return false;
  }

  ref_node_t* ref = (ref_node_t*) malloc(sizeof(ref_node_t));
  if (!ref) {
    return false;
  }

  ref->from_obj = from_ptr;
  ref->to_obj = to_ptr;

  ref->next = gc->references;
  gc->references = ref;

  return true;
}

bool simple_gc_remove_reference(gc_t *gc, void *from_ptr, void *to_ptr) {
  if (!gc || !from_ptr || !to_ptr) {
    return false;
  }

  ref_node_t** curr = &gc->references;
  while (*curr) {
    ref_node_t* ref = *curr;
    if (ref->from_obj == from_ptr && ref->to_obj == to_ptr) {
      *curr = ref->next;
      free(ref);
      return true;
    }
    curr = &(*curr)->next;
  }

  return false;
}

bool simple_gc_set_stack_bottom(gc_t *gc, void *hint) {
  if (!gc) return false;

  // where hint points to a local variable
  gc->stack_bottom = hint;
  return true;
}

void *simple_gc_get_stack_bottom(gc_t *gc) {
  return gc ? gc->stack_bottom : NULL;
}

bool simple_gc_enable_auto_roots(gc_t *gc, bool enable) {
  if (!gc) return false;
  gc->auto_root_scan_enabled = enable;
  return true;
}

bool simple_gc_is_heap_pointer(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return false;

  // fast boundary check
  void *heap_start = gc->heap_start;
  void *heap_end = gc->heap_end;
  if (!heap_start || !heap_end) {
    return false;
  }
  if (ptr < heap_start || ptr >= heap_end) {
    return false;
  }

  obj_header_t *curr = gc->objects;
  while (curr) {
    void *start = (void*)(curr + 1);
    void *end = (void*)((char*) start + curr->size);
    if (ptr >= start && ptr < end) {
      return true;
    }
    curr = curr->next;
  }
  return false;
}

void simple_gc_scan_stack(gc_t* gc) {
  if (!gc || !gc->stack_bottom || !gc->auto_root_scan_enabled) return;

  // save registers to the stack
  gc_platform_save_registers();

  // new local to approximate the current stack pointer
  void *stack_top;
  void *stack_ptr = &stack_top;

  uintptr_t align_mask = sizeof(void *) - 1;
  char *scan_start = (char*) (((uintptr_t) stack_ptr) & ~align_mask);
  char *scan_end = (char*) (((uintptr_t) gc->stack_bottom) & ~align_mask);

  if (scan_start > scan_end) {
    char* tmp = scan_start;
    scan_start = scan_end;
    scan_end = tmp;
  }

  // scan by word (pointer-sized) chunks
  uintptr_t *curr_word = (uintptr_t*) scan_start;
  uintptr_t *last_word = (uintptr_t*) scan_end;
  while (curr_word < last_word) {
    void *check = (void*)(*curr_word);
    if (simple_gc_is_heap_pointer(gc, check)) {
      obj_header_t * header = simple_gc_find_header(gc, check);
      if (header && !header->marked) {
        simple_gc_mark(gc, check);
      }
    }
    // for now, accept false positives (integers mistaken for pointers)
    // but never false negatives (missing real pointers)
    curr_word++;
  }

}

bool simple_gc_auto_init_stack(gc_t *gc) {
  if (!gc) return false;

  void *stack_bottom = gc_platform_get_stack_bottom();
  if (!stack_bottom) return false;

  gc->stack_bottom = stack_bottom;
  gc->auto_root_scan_enabled = true;
  return true;
}

bool gc_pointer_in_block(pool_block_t *block, void *ptr) {
  if (!block || !ptr) return false;

  char *start = block->memory;
  char *end = start + (block->slot_size * block->capacity);
  char *p = (char*) ptr;
  return (p >= start && p < end);
}

// TODO: debug
obj_header_t* gc_find_header_in_pools(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return NULL;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      if (gc_pointer_in_block(block, ptr)) {
        // ptr found in block, find header
        char *base = (char*) block->memory;
        char *mem_start = (char*) ptr;
        ptrdiff_t offset = mem_start - base;
        size_t slot_index = offset / block->slot_size;
        char *slot_start = base + (slot_index * block->slot_size);

        obj_header_t *header = (obj_header_t*) slot_start;
        void *expected_data_ptr = (void*)(header + 1);
        if (expected_data_ptr == ptr && simple_gc_is_valid_header(header)) {
          return header;
        }

        // ptr is in the block but not a valid data pointer
        return NULL;
      }
      block = block->next;
    }
  }

  return NULL;
}

void gc_free_to_pool(gc_t *gc, obj_header_t *header) {
  if (!gc || !header) return;

  // check if the object is from a pool
  size_t size = header->size;
  size_class_t *sc = gc_get_size_class(gc, size);
  if (!sc) return;

  // find which block the object belongs to
  void *ptr = (void*)(header + 1);
  pool_block_t *block = sc->blocks;

  while (block) {
    if (gc_pointer_in_block(block, ptr)) {
      free_node_t *node = (free_node_t*) header;
      node->next = block->free_list;
      block->free_list = node;
      block->used--;
      sc->total_used--;
      return;
    }
    block = block->next;
  }
}

void gc_sweep_pools(gc_t *gc) {
  if (!gc) return;

  // sweep size classes
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      char *slot = (char*) block->memory;

      for (size_t j = 0; j < block->capacity; ++j) {
        obj_header_t *header = (obj_header_t*) slot;
        // slot is in use if not in the free list
        bool in_use = true;
        free_node_t *free_node = block->free_list;

        while (free_node) {
          if ((void*) free_node == (void*) header) {
            in_use = false;
            break;
          }
          free_node = free_node->next;
        }

        if (in_use) {
          // slot is used - check if marked
          if (!header->marked) {
            // unmarked, return to pool
            gc_free_to_pool(gc, header);
            gc->object_count--;
            size_t bytes_changed = (sizeof(obj_header_t) + header->size);
            gc->heap_used -= bytes_changed;
            gc->total_bytes_freed += bytes_changed;
          } else {
            // marked, unmark for next cycle
            header->marked = false;
          }
        }

        slot += block->slot_size;
      }

      block = block->next;
    }
  }
}

void gc_sweep_large_objects(gc_t *gc) {
  if (!gc) return;

  // large_block_t *prev = NULL;
  large_block_t *block = gc->large_blocks;
  while (block) {
    if (block->in_use) {
      obj_header_t *header = block->header;

      if (!header->marked) {
        // unmarked, mark as free so we can reuse it
        block->in_use = false;
        gc->object_count--;
        gc->heap_used -= (sizeof(obj_header_t) + header->size);
      } else {
        // marked, unmark for next cycle
        header->marked = false;
      }

      // prev = block;
      block = block->next;
    } else {
      // block is not in use - could be freed if we want aggressive cleanup
      // for now, keep it for reuse
      // prev = block;
      block = block->next;
    }
  }
}

void gc_sweep_huge_objects(gc_t *gc) {
  if (!gc) return;

  huge_object_t *prev = NULL;
  huge_object_t *object = gc->huge_objects;
  while (object) {
    obj_header_t *header = object->header;

    if (!header->marked) {
      // unmarked, free it
      huge_object_t *to_free = object;
      object = object->next;

      if (!prev) {
        gc->huge_objects = object;
      } else {
        prev->next = object;
      }

      gc->object_count--;
      gc->huge_object_count--;
      gc->heap_used -= to_free->size;
      // gc->heap_used -= (sizeof(obj_header_t) + to_free->size);
      munmap(to_free->memory, to_free->size);
      free(to_free);
    } else {
      // marked, unmark for next cycle
      header->marked = false;
      prev = object;
      object = object->next;
    }
  }
}

void simple_gc_get_stats(gc_t *gc, gc_stats_t *stats) {
  if (!gc || !stats) return;
  memset(stats, 0, sizeof(gc_stats_t));

  stats->object_count = gc->object_count;
  stats->heap_used = gc->heap_used;
  stats->heap_capacity = gc->heap_capacity;
  stats->total_allocations = gc->total_allocations;
  stats->total_collections = gc->total_collections;
  stats->large_block_count = gc->large_block_count;
  stats->huge_object_count = gc->huge_object_count;

  stats->pool_blocks_allocated = 0;
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    stats->size_class_stats[i] = sc->total_allocated;

    pool_block_t *block = sc->blocks;
    while (block) {
      stats->pool_blocks_allocated++;
      block = block->next;
    }
  }
}

void simple_gc_print_stats(gc_t *gc) {
  if (!gc) return;

  gc_stats_t stats;
  simple_gc_get_stats(gc, &stats);

  printf("\n=== GC Statistics ===\n");
  printf("Objects:          %zu\n", stats.object_count);
  printf("Heap used:        %zu bytes\n", stats.heap_used);
  printf("Heap capacity:    %zu bytes\n", stats.heap_capacity);
  printf("Total allocs:     %zu\n", stats.total_allocations);
  printf("Total collections:%zu\n", stats.total_collections);
  printf("Large blocks:     %zu\n", stats.large_block_count);
  printf("Huge objects:     %zu\n", stats.huge_object_count);
  printf("Pool blocks:      %zu\n", stats.pool_blocks_allocated);

  printf("\nSize class allocations:\n");
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; i++) {
    printf("  %3zu bytes: %zu\n",
           GC_SIZE_CLASS_SIZES[i],
           stats.size_class_stats[i]);
  }
  printf("====================\n\n");
}
