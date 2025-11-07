#include <sys/mman.h>
#include <stdlib.h>

#include "gc_sweep.h"
#include "simple_gc.h"
#include "gc_pool.h"
#include "gc_large.h"
#include "gc_debug.h"


void gc_sweep_pools(gc_t *gc) {
  if (!gc) return;

  // sweep size classes
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gc->size_classes[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      char *slot = (char*) block->memory;

      // collect unmarked objects first, then free them
      // this avoids corrupting the free list during iteration
      obj_header_t *to_free[block->capacity];
      size_t free_count = 0;

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
            // unmarked, collect for freeing
            to_free[free_count++] = header;
          } else {
            // marked, unmark for next cycle
            header->marked = false;
          }
        }

        slot += block->slot_size;
      }

      // free all the collected objects
      for (size_t j = 0; j < free_count; ++j) {
        obj_header_t *header = to_free[j];

        if (gc->debug) {
          void *data_ptr = (void*)(header + 1);
          gc_debug_track_free(gc, data_ptr);
        }

        size_t bytes_changed = (sizeof(obj_header_t) + header->size);
        gc_pool_free_to_block(block, sc, header);
        gc->object_count--;
        gc->heap_used -= bytes_changed;
        gc->total_bytes_freed += bytes_changed;
      }

      block = block->next;
    }
  }
}

void gc_sweep_large_blocks(gc_t *gc) {
  if (!gc) return;

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

      block = block->next;
    } else {
      // block is not in use - could be freed if we want aggressive cleanup
      // for now, keep it for reuse
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

void gc_sweep_legacy(gc_t *gc) {
  if (!gc) return;

  // legacy sweep for non-pool mode
  obj_header_t** curr = &gc->objects;
  while (*curr) {
    if (!(*curr)->marked) { // unreachable
      obj_header_t* tmp = *curr;

      if (gc->debug) {
        void *data_ptr = (void*)(tmp + 1);
        gc_debug_track_free(gc, data_ptr);
      }

      if (gc->trace) {
        void *data_ptr = (void*)(tmp + 1);
        size_t size = tmp->size;
        gc_trace_event_t event = {
          .type = GC_EVENT_FREE,
          .data.free = {data_ptr, size}
        };
        gc_trace_event(gc, &event);
      }

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

void gc_sweep_all(gc_t *gc) {
  if (!gc) return;

  if (gc->use_pools) {
    gc_sweep_pools(gc);
    gc_sweep_large_blocks(gc);
    gc_sweep_huge_objects(gc);
    return;  // exit early
  }

  // legacy sweep for non-pool mode
  gc_sweep_legacy(gc);
}

size_t gc_count_swept(gc_t *gc) {
  if (!gc) return 0;

  // TODO
  return 0;
}

size_t gc_bytes_freed_last_sweep(gc_t *gc) {
  if (!gc) return 0;

  // TODO
  return gc->total_bytes_freed;
}
