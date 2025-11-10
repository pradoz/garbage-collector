#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "simple_gc.h"
#include "gc_platform.h"
#include "gc_pool.h"
#include "gc_large.h"
#include "gc_mark.h"
#include "gc_sweep.h"
#include "gc_generation.h"
#include "gc_cardtable.h"
#include "gc_barrier.h"
#include "gc_trace.h"
#include "gc_debug.h"


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
  return gc_init_header(header, type, size);
}

bool simple_gc_is_valid_header(const obj_header_t *header) {
  return gc_is_valid_header(header);
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

static gc_config_t gc_default_config(void) {
  gc_config_t config;
  config.auto_collect = true;
  config.collect_threshold = 0.75f;
  config.auto_expand_pools = true;
  config.auto_shrink_pools = true;
  config.expansion_trigger = 100;
  return config;
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

  gc->gen_context = NULL;
  gc->barrier_context = NULL;

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
  if (!gc_pool_init_all_classes(gc->size_classes)) {
    free(gc->roots);
    return false;
  }

  gc->use_pools = true;
  gc->large_blocks = NULL;
  gc->large_block_count = 0;
  gc->huge_objects = NULL;
  gc->huge_object_count = 0;

  // memory pressure
  gc->config = gc_default_config();
  gc->pressure = GC_PRESSURE_NONE;
  gc->allocs_since_collect = 0;
  gc->alloc_rate = 0;
  gc->last_collect_time = 0;
  gc->last_alloc_time = 0;
  gc->last_collection_duration = 0;

  // stats
  gc->total_allocations = 0;
  gc->total_collections = 0;
  gc->total_bytes_allocated = 0;
  gc->total_bytes_freed = 0;
  gc->total_compactions = 0;
  gc->bytes_reclaimed = 0;

  // tracing/debugging
  gc->trace = NULL;
  gc->debug = NULL;

  // generational
  gc->gen_context = NULL;

  return true;
}

void simple_gc_destroy(gc_t* gc) {
  if (!gc) {
    return;
  }

  if (gc->barrier_context) gc_barrier_destroy(gc);
  if (gc->gen_context) gc_gen_destroy(gc);

  // end tracing if active
  if (gc->trace) gc_trace_end(gc);

  // free memory pools
  if (gc->use_pools) {
    gc_pool_destroy_all_classes(gc->size_classes);

    // free large blocks
    gc_large_destroy_all(gc->large_blocks);
    gc->large_blocks = NULL;
    gc->large_block_count = 0;

    // free huge objects
    gc_huge_destroy_all(gc->huge_objects);
    gc->huge_objects = NULL;
    gc->huge_object_count = 0;
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

size_t simple_gc_total_object_count(const gc_t *gc) {
  if (!gc) return 0;
  if (gc->gen_context && gc_gen_enabled(gc)) {
    gc_gen_t *gen = gc->gen_context;
    return gen->stats[GC_GEN_YOUNG].objects + gen->stats[GC_GEN_OLD].objects;
  }
  return gc->object_count;
}

size_t simple_gc_heap_capacity(const gc_t* gc) {
  return gc ? gc->heap_capacity : 0;
}

size_t simple_gc_heap_used(const gc_t *gc) {
  return gc ? gc->heap_used : 0;
}

static void gc_update_pressure(gc_t *gc) {
  if (!gc) return;
  gc->pressure = simple_gc_check_pressure(gc);
}

static bool gc_should_auto_collect(gc_t *gc) {
  if (!gc) return false;

  float utilization = (float) gc->heap_used / (float) gc->heap_capacity;

  // Check against configured threshold
  if (utilization >= gc->config.collect_threshold) return true;

  // Use actual pressure calculation
  gc_pressure_t pressure = simple_gc_check_pressure(gc);
  if (pressure >= GC_PRESSURE_HIGH) return true;

  // Allocation rate heuristic with config
  if (gc->allocs_since_collect > gc->config.expansion_trigger) {
    if (utilization > 0.60f) return true;
  }

  return false;
}

void *simple_gc_alloc(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || size == 0) return NULL;

  if (gc->gen_context && gc_gen_enabled(gc)) {
    void *result = gc_gen_alloc(gc, type, size);
    if (result) {
      // bookkeeping for legacy mode
      gc->allocs_since_collect++;
      gc->total_allocations++;
      size_t total_size = sizeof(obj_header_t) + size;
      gc->total_bytes_allocated += total_size;
      update_heap_bounds(gc, result, size);
      if (gc_gen_should_collect_minor(gc)) {
        gc_gen_collect_minor(gc);
      }
      return result;
    }
    return NULL;
  }

  clock_t now = clock();
  if (gc->last_alloc_time > 0) {
    clock_t time_since_last = now - gc->last_alloc_time;
    // update running average of allocation rate
    // rate = allocations per second
    if (time_since_last > 0) {
      double seconds = (double)time_since_last / CLOCKS_PER_SEC;
      gc->alloc_rate = 1.0 / seconds;  // Simple rate calculation
    }
  }
  gc->last_alloc_time = now;

  // auto-collect if pressure indicates to do so
  gc_update_pressure(gc);
  if (gc_should_auto_collect(gc)) simple_gc_collect(gc);

  // capacity check
  size_t total_size = sizeof(obj_header_t) + size;
  if (total_size + gc->heap_used > gc->heap_capacity) return NULL;

  void *result = NULL;

  if (gc->use_pools) {
    size_class_t *sc = gc_pool_get_size_class(gc->size_classes, size);
    if (sc) {
      result = gc_pool_alloc_from_size_class(sc, type, size);
    } else {
      if (size >= GC_HUGE_OBJECT_THRESHOLD) {
        result = gc_huge_alloc(&gc->huge_objects, &gc->huge_object_count, type, size);
      } else {
        result = gc_large_alloc(&gc->large_blocks, &gc->large_block_count, type, size);
      }
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


  // bookkeeping for legacy mode
  if (result) {
    GC_TRACE_ALLOC(gc, result, size, type, __FILE__, __LINE__);
    gc->allocs_since_collect++;
    gc->object_count++;
    gc->heap_used += total_size;
    update_heap_bounds(gc, result, size);
    gc->total_allocations++;
    gc->total_bytes_allocated += total_size;
  }
  return result;
}

void *simple_gc_alloc_debug(gc_t *gc, obj_type_t type, size_t size,
    const char *file, int line, const char *func) {
  void *result = simple_gc_alloc(gc, type, size);

  if (result && gc->debug) {
    gc_debug_track_alloc(gc, result, size, type, file, line, func);
  }

  return result;
}

obj_header_t* gc_find_header_in_pools(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return NULL;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      if (gc_pool_pointer_in_block(block, ptr)) {
        // ptr found in block, find header
        char *base = (char*) block->memory;
        char *mem_start = (char*) ptr;
        ptrdiff_t offset = mem_start - base;
        size_t slot_index = offset / block->slot_size;
        char *slot_start = base + (slot_index * block->slot_size);

        obj_header_t *header = (obj_header_t*) slot_start;
        void *expected_data_ptr = (void*)(header + 1);

        if (expected_data_ptr == ptr) {
          // check if this slot is actually in use (not in free list)
          bool is_free = false;
          free_node_t *free_node = block->free_list;
          while (free_node) {
            if ((void*)free_node == (void*)header) {
              is_free = true;
              break;
            }
            free_node = free_node->next;
          }

          if (!is_free && simple_gc_is_valid_header(header)) {
            return header;
          }
        }

        // ptr is in the block but not a valid data pointer
        return NULL;
      }
      block = block->next;
    }
  }

  return NULL;
}

obj_header_t *simple_gc_find_header(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return NULL;

  // check young generation
  if (gc->gen_context && gc_gen_enabled(gc)) {
    gc_gen_t *gen = gc->gen_context;

    // search young pools
    for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
      size_class_t *sc = &gen->young_pools[i];
      pool_block_t *block = sc->blocks;

      while (block) {
        if (gc_pool_pointer_in_block(block, ptr)) {
          char *base = (char*) block->memory;
          char *mem_start = (char*) ptr;
          ptrdiff_t offset = mem_start - base;
          size_t slot_index = offset / block->slot_size;
          char *slot_start = base + (slot_index * block->slot_size);

          obj_header_t *header = (obj_header_t*) slot_start;
          void *expected_data_ptr = (void*)(header + 1);

          if (expected_data_ptr == ptr) { // check if slot in use
            bool is_free = false;
            free_node_t *free_node = block->free_list;

            while (free_node) {
              if ((void*)free_node == (void*)header) {
                is_free = true;
                break;
              }
              free_node = free_node->next;
            }
            if (!is_free && simple_gc_is_valid_header(header)) {
              return header;
            }
          }

          return NULL;
        }

        block = block->next;
      }
    }


    // search young large blocks
    large_block_t *large = gen->young_large;
    while (large) {
      void *data = (void*)(large->header + 1);
      if (data == ptr) {
        return large->header;
      }
      large = large->next;
    }
  }

  // check old gen pools
  if (gc->use_pools) {
    obj_header_t* header = gc_find_header_in_pools(gc, ptr);
    if (header) return header;

    header = gc_large_find_header(gc->large_blocks, ptr);
    if (header) return header;

    header = gc_huge_find_header(gc->huge_objects, ptr);
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

  if (gc->trace) {
    gc_trace_event_t event = {.type = GC_EVENT_ROOT_ADD};
    gc_trace_event(gc, &event);
  }

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

      if (gc->trace) {
        gc_trace_event_t event = {.type = GC_EVENT_ROOT_REMOVE};
        gc_trace_event(gc, &event);
      }
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


void simple_gc_collect(gc_t *gc) {
  if (!gc) {
    return;
  }

  clock_t start = clock();
  size_t objects_before = gc->object_count;
  size_t bytes_before = gc->heap_used;
  GC_TRACE_COLLECT_START(gc, "full", objects_before, bytes_before);

  gc->total_collections++;

  // trace mark phase
  if (gc->trace) {
    gc_trace_event_t event = {.type = GC_EVENT_MARK_START};
    gc_trace_event(gc, &event);
  }

  gc_mark_all_roots(gc);

  // automated root scanning
  if (gc->auto_root_scan_enabled) {
    simple_gc_scan_stack(gc);
  }

  if (gc->trace) {
    gc_trace_event_t event = {.type = GC_EVENT_MARK_END};
    gc_trace_event(gc, &event);
  }

  // sweep phase
  if (gc->trace) {
    gc_trace_event_t event = {.type = GC_EVENT_SWEEP_START};
    gc_trace_event(gc, &event);
  }

  gc_sweep_all(gc);

  if (gc->trace) {
    gc_trace_event_t event = {.type = GC_EVENT_SWEEP_END};
    gc_trace_event(gc, &event);
  }

  // auto-compact if fragmented
  if (simple_gc_should_compact(gc)) {
    if (gc->trace) {
      gc_trace_event_t event = {.type = GC_EVENT_COMPACT_START};
      gc_trace_event(gc, &event);

    }
    simple_gc_compact(gc);

    if (gc->trace) {
      gc_trace_event_t event = {.type = GC_EVENT_COMPACT_END};
      gc_trace_event(gc, &event);
    }
  }

  simple_gc_auto_tune(gc);

  clock_t end = clock();

  gc->allocs_since_collect = 0;
  gc->last_collection_duration = (double) (end - start) / CLOCKS_PER_SEC;

  double duration = (double) (end - start) / CLOCKS_PER_SEC * 1000.0;
  size_t collected = objects_before - gc->object_count;

  GC_TRACE_COLLECT_END(gc, gc->object_count, gc->heap_used, collected, 0, duration);
}

bool simple_gc_add_reference(gc_t *gc, void *from_ptr, void *to_ptr) {
  if (!gc || !from_ptr || !to_ptr) {
    return false;
  }

  if (!simple_gc_find_header(gc, from_ptr) || !simple_gc_find_header(gc, to_ptr)) {
    return false;
  }

  if (gc->barrier_context) gc_barrier_write(gc, from_ptr, to_ptr);

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
        gc_mark_object(gc, check);
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

static bool gc_add_relocation(compaction_ctx_t *ctx, void *old_addr, void *new_addr) {
  if (!ctx || !old_addr || !new_addr) return false;

  relocation_entry_t *entry = (relocation_entry_t*) malloc(sizeof(relocation_entry_t));
  if (!entry) return false;

  entry->old_addr = old_addr;
  entry->new_addr = new_addr;
  entry->next = ctx->relocations;

  ctx->relocations = entry;
  ctx->relocation_count++;

  return true;
}

static void *gc_find_new_address(compaction_ctx_t *ctx, void *old_addr) {
  if (!ctx || !old_addr) return NULL;

  relocation_entry_t *entry = ctx->relocations;
  while (entry) {
    if (entry->old_addr == old_addr) {
      return entry->new_addr;
    }
    entry = entry->next;
  }

  // did not relocate
  return old_addr;
}

static void gc_clear_relocations(compaction_ctx_t *ctx) {
  if (!ctx) return;

  relocation_entry_t *entry = ctx->relocations;
  while (entry) {
    relocation_entry_t* next = entry->next;
    free(entry);
    entry = next;
  }

  ctx->relocations = NULL;
  ctx->relocation_count = 0;
}

static void gc_compact_size_class(gc_t *gc, size_class_t *sc) {
  if (!gc || !sc || sc->total_used == 0) return;

  float util = gc_pool_utilization(sc);
  // good utilization, don't compact
  if (util > 0.7f) return;

  live_obj_t *live_objects = (live_obj_t*) malloc(sizeof(live_obj_t) * sc->total_used);
  if (!live_objects) return;

  // collect live objects
  size_t live_count = 0;
  pool_block_t *block = sc->blocks;
  while (block) {
    char *slot = (char*) block->memory;

    for (size_t i = 0; i < block->capacity; ++i) {
      bool in_use = true;
      obj_header_t *header = (obj_header_t*) slot;
      free_node_t *free_node = block->free_list;

      while (free_node) {
        if ((void*) free_node == (void*) header) {
          in_use = false;
          break;
        }
        free_node = free_node->next;
      }

      if (in_use) {
        live_objects[live_count].header = header;
        live_objects[live_count].data = (void*)(header + 1);
        live_objects[live_count].block = block;
        ++live_count;
      }

      slot += block->slot_size;
    }

    block = block->next;
  }

  // find new addresses and register relocations
  block = sc->blocks;
  char *dest = (char*) block->memory;

  for (size_t i = 0; i < live_count; ++i) {
    // obj_header_t *old_header = live_objects[i].header;
    void *old_data = live_objects[i].data;

    obj_header_t *new_header = (obj_header_t*) dest;
    void *new_data = (void*)(new_header + 1);

    // register relocation before moving
    if (old_data != new_data) {
      gc_add_relocation(&gc->compaction, old_data, new_data);
    }

    dest += block->slot_size;

    // move to next block if the current block is full
    if (dest >= (char*) block->memory + (block->slot_size * block->capacity)) {
      block = block->next;
      if (!block) break;
      dest = (char*) block->memory;
    }
  }

  // shift objects in the block
  block = sc->blocks;
  dest = (char*) block->memory;

  for (size_t i = 0; i < live_count; ++i) {
    obj_header_t *old_header = live_objects[i].header;
    obj_header_t *new_header = (obj_header_t*) dest;

    if (old_header != new_header) {
      memmove(new_header, old_header, sizeof(obj_header_t) + old_header->size);
    }

    dest += block->slot_size;

    if (dest >= (char*) block->memory + (block->slot_size * block->capacity)) {
      block = block->next;
      if (!block) break;
      dest = (char*) block->memory;
    }
  }

  free(live_objects);

  // rebuild free lists
  block = sc->blocks;
  size_t objects_placed = 0;

  while (block) {
    block->free_list = NULL;
    block->used = 0;

    char *slot = (char*) block->memory;

    for (size_t i = 0; i < block->capacity; ++i) {
      if (objects_placed < live_count) { // slot is used
        ++block->used;
        ++objects_placed;
      } else { // slot is available, add to free list
        free_node_t *free_node = (free_node_t*) slot;
        free_node->next = block->free_list;
        block->free_list = free_node;
      }

      slot += block->slot_size;
    }

    block = block->next;
  }
}

static void gc_update_pointer(compaction_ctx_t *ctx, void **ptr_ref) {
  if (!ctx || !ptr_ref || !*ptr_ref) return;

  void *new_addr = gc_find_new_address(ctx, *ptr_ref);
  if (new_addr != *ptr_ref) *ptr_ref = new_addr;
}

static void gc_update_all_references(gc_t *gc) {
  if (!gc) return;

  compaction_ctx_t *ctx = &gc->compaction;

  // update roots
  for (size_t i = 0; i < gc->root_count; ++i) {
    gc_update_pointer(ctx, &gc->roots[i]);
  }

  // update reference graph
  ref_node_t *ref = gc->references;
  while (ref) {
    gc_update_pointer(ctx, &ref->from_obj);
    gc_update_pointer(ctx, &ref->to_obj);
    ref = ref->next;
  }

  // update heap bounds
  gc_update_pointer(ctx, &gc->heap_start);
  gc_update_pointer(ctx, &gc->heap_end);
}

bool simple_gc_should_compact(gc_t *gc) {
  if (!gc || !gc->use_pools) return false;

  // stats
  size_t total_capacity = 0;
  size_t total_used = 0;
  size_t fragmented_classes = 0;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    total_capacity += sc->total_capacity;
    total_used += sc->total_used;

    if (sc->total_capacity > 0) {
      float util = gc_pool_utilization(sc);
      if (util < 0.5f && sc->total_used > 0) {
        ++fragmented_classes;
      }
    }
  }

  // compact if overall utilization <50% and multiple classes fragmented
  if (total_capacity > 0) {
    float overall_util = (float) total_used / (float) total_capacity;
    return (overall_util < 0.5 && fragmented_classes >= 1);
  }
  return false;
}

void simple_gc_compact(gc_t *gc) {
  if (!gc || !gc->use_pools) return;

  gc->compaction.in_progress = true;
  gc->compaction.relocations = NULL;
  gc->compaction.relocation_count = 0;

  // Track fragmentation reduction instead of heap usage
  size_t fragmented_before = 0;
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    fragmented_before += gc_pool_fragmented_bytes(sc);
  }

  // compact each size class
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    gc_compact_size_class(gc, &gc->size_classes[i]);
  }
  gc_update_all_references(gc);
  gc_clear_relocations(&gc->compaction);

  gc->compaction.in_progress = false;
  gc->total_compactions++;

  // Calculate fragmentation after compaction
  size_t fragmented_after = 0;
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    fragmented_after += gc_pool_fragmented_bytes(sc);
  }

  // Track how much fragmentation was reduced
  if (fragmented_before > fragmented_after) {
    gc->bytes_reclaimed += (fragmented_before - fragmented_after);
  }
}

// memory pressure
gc_pressure_t simple_gc_check_pressure(gc_t *gc) {
  if (!gc) return GC_PRESSURE_NONE;

  float utilization = (float) gc->heap_used / (float) gc->heap_capacity;
  if (utilization >= 0.95f) return GC_PRESSURE_CRITICAL;
  if (utilization >= 0.85f) return GC_PRESSURE_HIGH;
  if (utilization >= 0.70f) return GC_PRESSURE_MEDIUM;
  if (utilization >= 0.50f) return GC_PRESSURE_LOW;
  return GC_PRESSURE_NONE;
}

static bool gc_expand_pool(gc_t *gc, size_class_t *sc) {
  if (!gc || !sc) return false;

  size_t current_capacity = sc->total_capacity;
  size_t new_capacity = current_capacity > 0 ? current_capacity : 64;

  pool_block_t *new_block = gc_pool_create_block(sc->slot_size, new_capacity);
  if (!new_block) return false;

  // add new block to size class
  new_block->next = sc->blocks;
  sc->blocks = new_block;
  sc->total_capacity += new_block->capacity;

  return true;
}

static void gc_shrink_pool(gc_t *gc, size_class_t *sc) {
  if (!gc || !sc) return;

  pool_block_t **curr = &sc->blocks;
  while (*curr) {
    pool_block_t *block = *curr;

    // keep >=1 block
    if (block->next && block->used == 0) {
      // block is empty, remove it
      *curr = block->next;
      sc->total_capacity -= block->capacity;
      gc_pool_free_block(block);
    } else {
      curr = &block->next;
    }
  }
}

void simple_gc_set_config(gc_t *gc, gc_config_t *config) {
  if (!gc || !config) return;
  gc->config = *config;
}

void simple_gc_auto_tune(gc_t *gc) {
  if (!gc || !gc->use_pools) return;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; i++) {
    size_class_t *sc = &gc->size_classes[i];
    if (sc->total_capacity == 0) continue;

    float utilization = gc_pool_utilization(sc);
    if (gc->config.auto_expand_pools && utilization > 0.9f) gc_expand_pool(gc, sc);
    if (gc->config.auto_shrink_pools && utilization < 0.2f) gc_shrink_pool(gc, sc);
  }
}

bool simple_gc_enable_generations(gc_t *gc, size_t young_size) {
  if (!gc) return false;
  if (gc->gen_context) return true; // don't re-initialize

  // default to 20% of heap for young gen
  if (young_size == 0) young_size = gc->heap_capacity / 5;

  return gc_gen_init(gc, young_size);
}

void simple_gc_disable_generations(gc_t *gc) {
  if (!gc) return;
  gc_gen_destroy(gc);
}

bool simple_gc_is_generational(gc_t *gc) {
  return gc_gen_enabled(gc);
}

void simple_gc_collect_minor(gc_t *gc) {
  if (!gc) return;

  if (gc_gen_enabled(gc)) {
    gc_gen_collect_minor(gc);
  } else {
    simple_gc_collect(gc);
  }
}

void simple_gc_collect_major(gc_t *gc) {
  if (!gc) return;

  if (gc_gen_enabled(gc)) {
    gc_gen_collect_major(gc);
  } else {
    simple_gc_collect(gc);
  }
}

void simple_gc_print_gen_stats(gc_t *gc) {
  if (!gc) return;

  if (gc_gen_enabled(gc)) {
    gc_gen_print_stats(gc);
  } else {
    printf("Generational GC not enabled\n");
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
  size_t total_capacity = 0;
  size_t total_fragmented = 0;

  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    stats->size_class_stats[i] = sc->total_allocated;
    stats->pool_blocks_allocated += gc_pool_count_blocks(sc);

    // Calculate fragmentation
    size_t capacity_bytes = sc->total_capacity * sc->slot_size;
    size_t used_bytes = sc->total_used * sc->slot_size;
    total_capacity += capacity_bytes;
    total_fragmented += (capacity_bytes - used_bytes);
  }

  stats->total_fragmented_bytes = total_fragmented;
  stats->fragmentation_ratio = total_capacity > 0
    ? (float)total_fragmented / (float)total_capacity
    : 0.0f;
}

bool simple_gc_enable_write_barrier(gc_t *gc) {
  if (!gc) return false;
  return gc_barrier_init(gc, GC_BARRIER_CARD_MARKING);
}

void simple_gc_disable_write_barrier(gc_t *gc) {
  if (!gc) return;
  gc_barrier_destroy(gc);
}

void simple_gc_write(gc_t *gc, void *from, void *to) {
  if (!gc) return;
  gc_barrier_write(gc, from, to);
}

void simple_gc_print_barrier_stats(gc_t *gc) {
  if (!gc) return;
  gc_barrier_print_stats(gc);
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
  printf("Fragmented:       %zu bytes (%.1f%%)\n",
         stats.total_fragmented_bytes,
         stats.fragmentation_ratio * 100.0f);

  printf("\nSize class allocations:\n");
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; i++) {
    printf("  %3zu bytes: %zu\n",
           GC_SIZE_CLASS_SIZES[i],
           stats.size_class_stats[i]);
  }
  printf("====================\n\n");
}
