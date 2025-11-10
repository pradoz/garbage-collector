#include "gc_barrier.h"
#include "simple_gc.h"
#include "gc_generation.h"
#include "gc_cardtable.h"
#include "gc_trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


bool gc_barrier_init(gc_t *gc, gc_barrier_type_t type) {
  if (!gc) return false;

  gc_barrier_t *barrier = (gc_barrier_t*) calloc(1, sizeof(gc_barrier_t));
  if (!barrier) return false;

  barrier->type = type;
  barrier->enabled = true;
  memset(&barrier->stats, 0, sizeof(gc_barrier_stats_t));

  gc->barrier_context = barrier;
  return true;
}

void gc_barrier_destroy(gc_t *gc) {
  if (!gc || !gc->barrier_context) return;

  free(gc->barrier_context);
  gc->barrier_context = NULL;
}

void gc_barrier_write(gc_t *gc, void *from_obj, void *to_obj) {
  if (!gc || !from_obj || !to_obj) return;

  gc_barrier_t *barrier = gc->barrier_context;
  if (!barrier || !barrier->enabled) return;

  barrier->stats.total_writes++;

  obj_header_t *from_header = simple_gc_find_header(gc, from_obj);
  obj_header_t *to_header = simple_gc_find_header(gc, to_obj);
  if (!from_header || !to_header) return;

  // track cross-gen references
  if (from_header->generation != to_header->generation) {
    barrier->stats.barrier_hits++;

    // track old->young references for minor GC
    if (from_header->generation == GC_GEN_OLD && to_header->generation == GC_GEN_YOUNG) {
      barrier->stats.old_to_young++;

      if (barrier->type == GC_BARRIER_CARD_MARKING && gc->gen_context) {
        gc_gen_t *gen = gc->gen_context;

        // lazily initialize card table when we have heap bounds
        if (!gen->cardtable.enabled && gc->heap_start && gc->heap_end) {
          size_t heap_size = (char*)gc->heap_end - (char*)gc->heap_start;
          if (gc_cardtable_init(&gen->cardtable, gc->heap_start, heap_size)) {
            gen->cardtable.enabled = true;
          }
        }
        if (gen->cardtable.enabled) {
          gc_cardtable_mark_dirty(&gen->cardtable, from_obj);
        }
      }

      if (gc->trace) {
        gc_trace_event_t event = {
          .type = GC_EVENT_PROMOTION,
          .data.promotion = {from_obj, from_header->generation, to_header->generation},
        };
        gc_trace_event(gc, &event);
      }
    } else if (from_header->generation == GC_GEN_YOUNG && to_header->generation == GC_GEN_OLD) {
      // track old->young references
      barrier->stats.young_to_old++;
    }
  } else {
    barrier->stats.same_generation++;
  }
}

void gc_barrier_array_write(gc_t *gc, void *array, size_t index, void *value) {
  if (!gc || !array || !value) return;

  gc_barrier_write(gc, array, value);
  void **arr = (void*) array;
  arr[index] = value;
}

void gc_barrier_get_stats(gc_t *gc, gc_barrier_stats_t *stats) {
  if (!gc || !gc->barrier_context || !stats) return;

  *stats = gc->barrier_context->stats;
}

void gc_barrier_print_stats(gc_t *gc) {
  if (!gc || !gc->barrier_context) return;

  gc_barrier_stats_t *stats = &gc->barrier_context->stats;

  printf("\n=== Write Barrier Statistics ===\n");
  printf("Total writes:      %zu\n", stats->total_writes);
  printf("Barrier hits:      %zu (%.2f%%)\n",
      stats->barrier_hits,
      stats->total_writes > 0
      ? (float)stats->barrier_hits / stats->total_writes * 100.0f
      : 0.0f);
  printf("Old -> Young:      %zu\n", stats->old_to_young);
  printf("Young -> Old:      %zu\n", stats->young_to_old);
  printf("Same generation:   %zu\n", stats->same_generation);
  printf("================================\n\n");
}

void gc_barrier_reset_stats(gc_t *gc) {
  if (!gc || !gc->barrier_context) return;

  memset(&gc->barrier_context->stats, 0, sizeof(gc_barrier_stats_t));
}
