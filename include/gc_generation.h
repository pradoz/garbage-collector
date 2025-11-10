#ifndef GC_GENERATION_H
#define GC_GENERATION_H


#include <stddef.h>
#include <stdbool.h>
#include "gc_types.h"
#include "gc_pool.h"
#include "gc_large.h"
#include "gc_cardtable.h"


#define GC_PROMOTION_AGE 3

typedef struct gc_context gc_t;
typedef struct gc_gen_context gc_gen_t;


typedef enum {
  GC_GEN_YOUNG = 0,
  GC_GEN_OLD = 1,
  GC_GEN_COUNT = 2,
} gc_generation_id_t;

typedef struct {
  size_t collections;
  size_t objects;
  size_t bytes_used;
  size_t promotions;
  double total_time_ms;
} gc_gen_stats_t;

typedef struct gc_gen_context {
  bool enabled;

  size_class_t young_pools[GC_NUM_SIZE_CLASSES];
  large_block_t *young_large;
  size_t young_large_count;

  size_t young_capacity;
  size_t young_used;

  gc_cardtable_t cardtable;

  gc_gen_stats_t stats[GC_GEN_COUNT];
  size_t minor_count;
  size_t major_count;
} gc_gen_t;


bool gc_gen_init(gc_t *gc, size_t young_size);
void gc_gen_destroy(gc_t *gc);
bool gc_gen_enabled(const gc_t *gc);

void *gc_gen_alloc(gc_t *gc, obj_type_t type, size_t size);

bool gc_gen_should_collect_minor(gc_t *gc);
bool gc_gen_should_collect_major(gc_t *gc);
void gc_gen_collect_minor(gc_t *gc);
void gc_gen_collect_major(gc_t *gc);

gc_generation_id_t gc_gen_which_generation(gc_t *gc, void *ptr);
size_t gc_gen_young_size(gc_t *gc);
size_t gc_gen_old_size(gc_t *gc);

void gc_gen_get_stats(gc_t *gc, gc_gen_stats_t *young, gc_gen_stats_t *old);
void gc_gen_print_stats(gc_t *gc);


#endif /* GC_GENERATION_H */
