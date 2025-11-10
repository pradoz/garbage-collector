#ifndef GC_BARRIER_H
#define GC_BARRIER_H


#include <stdbool.h>
#include <stddef.h>
#include "gc_types.h"


typedef struct gc_context gc_t;


typedef enum {
  GC_BARRIER_NONE = 0,
  GC_BARRIER_CARD_MARKING = 1,
  GC_BARRIER_SNAPSHOT = 2,
  GC_BARRIER_INCREMENTAL = 3,
} gc_barrier_type_t;

typedef struct {
  size_t total_writes;
  size_t barrier_hits;
  size_t young_to_old;
  size_t old_to_young;
  size_t same_generation;
} gc_barrier_stats_t;

typedef struct gc_barrier_context {
  gc_barrier_type_t type;
  bool enabled;
  gc_barrier_stats_t stats;
} gc_barrier_t;


bool gc_barrier_init(gc_t *gc, gc_barrier_type_t type);
void gc_barrier_destroy(gc_t *gc);

void gc_barrier_write(gc_t *gc, void *from_obj, void *to_obj);
void gc_barrier_array_write(gc_t *gc, void *array, size_t index, void *value);

void gc_barrier_get_stats(gc_t *gc, gc_barrier_stats_t *stats);
void gc_barrier_print_stats(gc_t *gc);
void gc_barrier_reset_stats(gc_t *gc);

#define GC_WRITE(gc, from, to) gc_barrier_write(gc, from, to)
#define GC_ARRAY_WRITE(gc, array, index, value) gc_barrier_array_write(gc, array, index, value)


#endif /* GC_BARRIER_H */
