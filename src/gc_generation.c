#include "gc_generation.h"
#include "simple_gc.h"
#include "gc_types.h"
#include "gc_pool.h"
#include "gc_large.h"
// probably delete these check after compiling
#include "gc_mark.h"
#include "gc_sweep.h"
#include "gc_trace.h"
#include "gc_debug.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>


bool gc_gen_init(gc_t *gc, size_t young_size) {
  if (!gc || young_size == 0) return false;

  gc_gen_t *gen = (gc_gen_t*) calloc(1, sizeof(gc_gen_t));
  if (!gen) return false;

  if (!gc_pool_init_all_classes(gen->young_pools)) {
    free(gen);
    return false;
  }

  gen->enabled = true;
  gen->young_large = NULL;
  gen->young_large_count = 0;
  gen->young_capacity = young_size;
  gen->young_used = 0;
  gen->minor_count= 0;
  gen->major_count= 0;

  memset(gen->stats, 0, sizeof(gen->stats));
  gc->gen_context = gen;

  return true;
}

void gc_gen_destroy(gc_t *gc) {
  if (!gc || !gc->gen_context) return;

  gc_gen_t *gen = gc->gen_context;

  gc_pool_destroy_all_classes(gen->young_pools);
  gc_large_destroy_all(gen->young_large);

  free(gen);
  gc->gen_context = NULL;
}

bool gc_gen_enabled(gc_t *gc) {
  return (gc && gc->gen_context && gc->gen_context->enabled);
}

static obj_header_t* gc_gen_find_header_young(gc_t *gc, void *ptr) {
  if (!gc || !gc->gen_context || !ptr) return NULL;

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
        if (expected_data_ptr == ptr) {
          return header;
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

  return NULL;
}

void *gc_gen_alloc(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || !gc->gen_context || size == 0) return NULL;

  gc_gen_t *gen = gc->gen_context;
  void *result = NULL;

  size_class_t *sc = gc_pool_get_size_class(gen->young_pools, size);
  if (sc) { // young small
    result = gc_pool_alloc_from_size_class(sc, type, size);
    if (result) {
      gen->young_used += sizeof(obj_header_t) + size;
      gen->stats[GC_GEN_YOUNG].objects++;
      gen->stats[GC_GEN_YOUNG].bytes_used += size;

      obj_header_t *header = gc_gen_find_header_young(gc, result);
      if (header) {
        header->generation = GC_GEN_YOUNG;
        header->age = 0;
      }

      if (gc->trace) {
        gc_trace_event_t event = {
          .type = GC_EVENT_ALLOC,
          .data.alloc = {result, size, type, NULL, 0},
        };
        gc_trace_event(gc, &event);
      }
    }
  } else if (size >= GC_LARGE_OBJECT_THRESHOLD && size < GC_HUGE_OBJECT_THRESHOLD) { // young large
    result = gc_large_alloc(&gen->young_large, &gen->young_large_count, type, size);
    if (result) {
      gen->young_used += sizeof(obj_header_t) + size;
      gen->stats[GC_GEN_YOUNG].objects++;
      gen->stats[GC_GEN_YOUNG].bytes_used += size;

      obj_header_t *header = gc_gen_find_header_young(gc, result);
      if (header) {
        header->generation = GC_GEN_YOUNG;
        header->age = 0;
      }

      if (gc->trace) {
        gc_trace_event_t event = {
          .type = GC_EVENT_ALLOC,
          .data.alloc = {result, size, type, NULL, 0},
        };
        gc_trace_event(gc, &event);
      }
    }

  } else { // huge object - allocate in old gen
    result = gc_huge_alloc(&gc->huge_objects, &gc->huge_object_count, type, size);
    if (result) {
      obj_header_t *header = simple_gc_find_header(gc, result);

      if (header) {
        header->generation = GC_GEN_OLD;
        header->age = GC_PROMOTION_AGE;
      }
      gen->stats[GC_GEN_OLD].objects++;
      gen->stats[GC_GEN_OLD].bytes_used += size;

      if (gc->trace) {
        gc_trace_event_t event = {
          .type = GC_EVENT_ALLOC,
          .data.alloc = {result, size, type, NULL, 0},
        };
        gc_trace_event(gc, &event);
      }
    }
  }

  return result;
}

bool gc_gen_should_collect_minor(gc_t *gc) {
  if (!gc || !gc->gen_context) return false;

  gc_gen_t *gen = gc->gen_context;
  float util = (float) gen->young_used / (float) gen->young_capacity;
  return util >= 0.8f;
}

bool gc_gen_should_collect_major(gc_t *gc) {
  if (!gc || !gc->gen_context) return false;

  gc_gen_t *gen = gc->gen_context;
  // major collections every 10 minor collections
  return gen->minor_count > 0 && gen->minor_count % 10 == 0;
}

static bool gc_gen_promote_object(gc_t *gc, obj_header_t *header, void *data) {
  if (!gc || !header || !data) return false;

  // allocate directly in old generation
  void *promoted = NULL;
  if (header->size <= GC_SIZE_CLASS_SIZES[GC_NUM_SIZE_CLASSES - 1]) { // small object
    size_class_t *sc = gc_pool_get_size_class(gc->size_classes, header->size);
    if (sc) {
      promoted = gc_pool_alloc_from_size_class(sc, header->type, header->size);
    }
  } else if (header->size >= GC_LARGE_OBJECT_THRESHOLD && header->size < GC_HUGE_OBJECT_THRESHOLD) { // large object
    promoted = gc_large_alloc(&gc->large_blocks, &gc->large_block_count, header->type, header->size);
  } else { // huge object
    promoted = gc_huge_alloc(&gc->huge_objects, &gc->huge_object_count, header->type, header->size);
  }

  if (!promoted) return false;

  memcpy(promoted, data, header->size);
  obj_header_t *new_header = simple_gc_find_header(gc, promoted);
  if (new_header) {
    new_header->generation = GC_GEN_OLD;
    new_header->age = GC_PROMOTION_AGE;
    new_header->marked = false;
  }

  // update references to new location
  ref_node_t *ref = gc->references;
  while (ref) {
    if (ref->to_obj == data) {
      ref->to_obj = promoted;
    }
    if (ref->from_obj == data) {
      ref->from_obj = promoted;
    }
    ref = ref->next;
  }

  // update roots
  for (size_t i = 0; i < gc->root_count; ++i) {
    if (gc->roots[i] == data) {
      gc->roots[i] = promoted;
    }
  }

  gc->object_count++;
  gc->heap_used += sizeof(obj_header_t) + header->size;

  return true;
}

static void gc_gen_record_promotion(gc_t *gc, gc_gen_t *gen, void *data, size_t size) {
  gen->stats[GC_GEN_YOUNG].promotions++;
  gen->stats[GC_GEN_OLD].objects++;
  gen->stats[GC_GEN_OLD].bytes_used += size;

  if (gc->trace) {
    gc_trace_event_t event = {
      .type = GC_EVENT_PROMOTION,
      .data.promotion = {data, GC_GEN_YOUNG, GC_GEN_OLD},
    };
    gc_trace_event(gc, &event);
  }
}

static bool gc_gen_try_promote(gc_t *gc, gc_gen_t *gen, obj_header_t *header, size_t *promoted_count) {
  void *data = (void*)(header + 1);
  void *old_data = data;  // memo old pointer for trace

  if (gc_gen_promote_object(gc, header, data)) {
    (*promoted_count)++;

    // get new pointer, may have changed due to reference updates
    void *new_data = old_data;
    for (size_t i = 0; i < gc->root_count; ++i) {
      if (gc->roots[i] != old_data) {

        // root was updated, find the new location
        obj_header_t *check = simple_gc_find_header(gc, gc->roots[i]);
        if (check && check->generation == GC_GEN_OLD && check->size == header->size) {
          new_data = gc->roots[i];
          break;
        }
      }
    }

    gc_gen_record_promotion(gc, gen, new_data, header->size);
    return true;
  }
  return false;
}

void gc_gen_collect_minor(gc_t *gc) {
  if (!gc || !gc->gen_context) return;

  gc_gen_t *gen = gc->gen_context;

  clock_t start = clock();
  size_t objects_before = gen->stats[GC_GEN_YOUNG].objects;
  size_t bytes_before = gen->stats[GC_GEN_YOUNG].bytes_used;
  size_t promoted_count = 0;
  size_t collected_count = 0;

  if (gc->trace) {
    GC_TRACE_COLLECT_START(gc, "minor", objects_before, bytes_before);
  }

  // mark roots that point to young generation
  for (size_t i = 0; i < gc->root_count; ++i) {
    obj_header_t *header = gc_gen_find_header_young(gc, gc->roots[i]);
    if (header && header->generation == GC_GEN_YOUNG) {
      header->marked = true;
    }
  }

  // mark young objects referenced by old generation
  ref_node_t *ref = gc->references;
  while (ref) {
    obj_header_t *from_header = simple_gc_find_header(gc, ref->from_obj);

    if (from_header && from_header->generation == GC_GEN_OLD) {
      obj_header_t *to_header = gc_gen_find_header_young(gc, ref->to_obj);

      if (to_header && to_header->generation == GC_GEN_YOUNG) {
        to_header->marked = true;
      }
    }
    ref = ref->next;
  }

  // transitive marking within young generation
  bool marked_something;
  do {
    marked_something = false;
    ref = gc->references;
    while (ref) {
      obj_header_t *from_header = gc_gen_find_header_young(gc, ref->from_obj);
      obj_header_t *to_header = gc_gen_find_header_young(gc, ref->to_obj);

      if (from_header && to_header &&
          from_header->generation == GC_GEN_YOUNG &&
          to_header->generation == GC_GEN_YOUNG &&
          from_header->marked && !to_header->marked) {
        to_header->marked = true;
        marked_something = true;
      }
      ref = ref->next;
    }
  } while (marked_something);

  // sweep young pools - TWO PHASE to avoid corrupting during iteration
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gen->young_pools[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      // Phase 1: Collect objects to free/promote (don't modify free list yet)
      typedef struct {
        obj_header_t *header;
        bool should_promote;
        size_t size;
      } young_action_t;

      young_action_t actions[block->capacity];
      size_t action_count = 0;

      char *slot = (char*) block->memory;
      for (size_t j = 0; j < block->capacity; ++j) {
        obj_header_t *header = (obj_header_t*) slot;

        // check if slot is in use
        bool in_use = true;
        free_node_t *free_node = block->free_list;
        while (free_node) {
          if ((void*) free_node == (void*) header) {
            in_use = false;
            break;
          }
          free_node = free_node->next;
        }

        if (in_use && header->generation == GC_GEN_YOUNG) {
          if (!header->marked) {
            // died young - will free it
            actions[action_count].header = header;
            actions[action_count].should_promote = false;
            actions[action_count].size = header->size;
            action_count++;
          } else {
            // survived - increment age
            header->age++;

            if (header->age >= GC_PROMOTION_AGE) {
              // will try to promote
              actions[action_count].header = header;
              actions[action_count].should_promote = true;
              actions[action_count].size = header->size;
              action_count++;
            } else {
              // not old enough - unmark for next cycle
              header->marked = false;
            }
          }
        }

        slot += block->slot_size;
      }

      // Phase 2: Execute actions (safe to modify free list now)
      for (size_t k = 0; k < action_count; ++k) {
        obj_header_t *header = actions[k].header;
        size_t size = actions[k].size;

        if (actions[k].should_promote) {
          // try to promote
          if (gc_gen_try_promote(gc, gen, header, &promoted_count)) {
            // promotion succeeded
            gen->young_used -= sizeof(obj_header_t) + size;
            gen->stats[GC_GEN_YOUNG].objects--;
            gen->stats[GC_GEN_YOUNG].bytes_used -= size;

            // free from young gen
            gc_pool_free_to_block(block, sc, header);
          } else {
            // promotion failed - unmark for next cycle
            header->marked = false;
          }
        } else {
          // died young - free it
          if (gc->debug) {
            void *data_ptr = (void*)(header + 1);
            gc_debug_track_free(gc, data_ptr);
          }

          // update stats before freeing
          gen->young_used -= sizeof(obj_header_t) + size;
          gen->stats[GC_GEN_YOUNG].objects--;
          gen->stats[GC_GEN_YOUNG].bytes_used -= size;
          collected_count++;

          // now safe to free
          gc_pool_free_to_block(block, sc, header);
        }
      }

      block = block->next;
    }
  }

  // sweep young large blocks (this part looks OK)
  large_block_t *large = gen->young_large;
  large_block_t *prev_large = NULL;

  while (large) {
    if (large->in_use && large->header && large->header->generation == GC_GEN_YOUNG) {
      if (!large->header->marked) {
        // died young
        if (gc->debug) {
          void *data_ptr = (void*)(large->header + 1);
          gc_debug_track_free(gc, data_ptr);
        }

        large->in_use = false;
        collected_count++;
        gen->young_used -= sizeof(obj_header_t) + large->header->size;
        gen->stats[GC_GEN_YOUNG].objects--;
        gen->stats[GC_GEN_YOUNG].bytes_used -= large->header->size;

        prev_large = large;
        large = large->next;
      } else {
        // survived
        large->header->age++;

        if (large->header->age >= GC_PROMOTION_AGE) {
          // try to promote
          if (gc_gen_try_promote(gc, gen, large->header, &promoted_count)) {
            // promotion succeeded
            gen->young_used -= sizeof(obj_header_t) + large->header->size;
            gen->stats[GC_GEN_YOUNG].objects--;
            gen->stats[GC_GEN_YOUNG].bytes_used -= large->header->size;

            // remove from young large list
            if (prev_large) {
              prev_large->next = large->next;
            } else {
              gen->young_large = large->next;
            }

            large_block_t *to_free = large;
            large = large->next;
            gen->young_large_count--;
            gc_large_free_block(to_free);
            continue;
          } else {
            // promotion failed
            large->header->marked = false;
          }
        } else {
          // not old enough
          large->header->marked = false;
        }

        prev_large = large;
        large = large->next;
      }
    } else {
      prev_large = large;
      large = large->next;
    }
  }

  clock_t end = clock();
  double duration = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

  gen->minor_count++;
  gen->stats[GC_GEN_YOUNG].collections++;
  gen->stats[GC_GEN_YOUNG].total_time_ms += duration;

  if (gc->trace) {
    GC_TRACE_COLLECT_END(gc,
        gen->stats[GC_GEN_YOUNG].objects,
        gen->stats[GC_GEN_YOUNG].bytes_used,
        collected_count,
        promoted_count,
        duration);
  }
}

void gc_gen_collect_major(gc_t *gc) {
  if (!gc || !gc->gen_context) return;

  gc_gen_t *gen = gc->gen_context;

  clock_t start = clock();
  size_t objects_before = gen->stats[GC_GEN_OLD].objects;
  size_t bytes_before = gen->stats[GC_GEN_OLD].bytes_used;

  if (gc->trace) {
    GC_TRACE_COLLECT_START(gc, "major", objects_before, bytes_before);
  }

  simple_gc_collect(gc);

  clock_t end = clock();
  double duration = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

  gen->major_count++;
  gen->stats[GC_GEN_OLD].collections++;
  gen->stats[GC_GEN_OLD].total_time_ms += duration;

  size_t collected = objects_before - gen->stats[GC_GEN_OLD].objects;

  if (gc->trace) {
    GC_TRACE_COLLECT_END(gc,
        gen->stats[GC_GEN_OLD].objects,
        gen->stats[GC_GEN_OLD].bytes_used,
        collected,
        0,
        duration);
  }
}

gc_generation_id_t gc_gen_which_generation(gc_t *gc, void *ptr) {
  if (!gc || !gc->gen_context || !ptr) return GC_GEN_OLD;

  // young pools
  gc_gen_t *gen = gc->gen_context;
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
    size_class_t *sc = &gen->young_pools[i];
    pool_block_t *block = sc->blocks;

    while (block) {
      if (gc_pool_pointer_in_block(block, ptr)) {
        return GC_GEN_YOUNG;
      }
      block = block->next;
    }
  }

  // young large blocks
  large_block_t *large = gen->young_large;
  while (large) {
    void *check = (void*)(large->header + 1);
    if (check == ptr) {
      return GC_GEN_YOUNG;
    }
    large = large->next;
  }

  return GC_GEN_OLD;
}

size_t gc_gen_young_size(gc_t *gc) {
  if (!gc || !gc->gen_context) return 0;
  return gc->gen_context->young_used;
}

size_t gc_gen_old_size(gc_t *gc) {
  if (!gc || !gc->gen_context) return 0;
  return gc->gen_context->stats[GC_GEN_OLD].bytes_used;
}

void gc_gen_get_stats(gc_t *gc, gc_gen_stats_t *young, gc_gen_stats_t *old) {
  if (!gc || !gc->gen_context || !young || ! old) return;

  gc_gen_t *gen = gc->gen_context;
  if (young) {
    *young = gen->stats[GC_GEN_YOUNG];
  }
  if (old) {
    *old = gen->stats[GC_GEN_OLD];
  }
}

void gc_gen_print_stats(gc_t *gc) {
  if (!gc || !gc->gen_context) return;

  gc_gen_t *gen = gc->gen_context;

  printf("\n=== Generational GC Statistics ===\n");
  printf("Minor collections: %zu\n", gen->minor_count);
  printf("Major collections: %zu\n", gen->major_count);
  printf("\n");

  printf("Young Generation:\n");
  printf("  Collections:   %zu\n", gen->stats[GC_GEN_YOUNG].collections);
  printf("  Objects:       %zu\n", gen->stats[GC_GEN_YOUNG].objects);
  printf("  Bytes:         %zu\n", gen->stats[GC_GEN_YOUNG].bytes_used);
  printf("  Promotions:    %zu\n", gen->stats[GC_GEN_YOUNG].promotions);
  printf("  Total time:    %.3f ms\n", gen->stats[GC_GEN_YOUNG].total_time_ms);
  if (gen->stats[GC_GEN_YOUNG].collections > 0) {
    printf("  Avg pause:     %.3f ms\n",
           gen->stats[GC_GEN_YOUNG].total_time_ms / gen->stats[GC_GEN_YOUNG].collections);
  }
  printf("\n");

  printf("Old Generation:\n");
  printf("  Collections:   %zu\n", gen->stats[GC_GEN_OLD].collections);
  printf("  Objects:       %zu\n", gen->stats[GC_GEN_OLD].objects);
  printf("  Bytes:         %zu\n", gen->stats[GC_GEN_OLD].bytes_used);
  printf("  Total time:    %.3f ms\n", gen->stats[GC_GEN_OLD].total_time_ms);
  if (gen->stats[GC_GEN_OLD].collections > 0) {
    printf("  Avg pause:     %.3f ms\n",
           gen->stats[GC_GEN_OLD].total_time_ms / gen->stats[GC_GEN_OLD].collections);
  }
}

