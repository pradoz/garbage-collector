#ifndef GC_SWEEP_H
#define GC_SWEEP_H


#include <stdbool.h>
#include <stddef.h>
#include "gc_types.h"
#include "gc_pool.h"
#include "gc_large.h"


typedef struct gc_context gc_t;


void gc_sweep_pools(gc_t *gc);
void gc_sweep_large_blocks(gc_t *gc);
void gc_sweep_huge_objects(gc_t *gc);
void gc_sweep_legacy(gc_t *gc);
void gc_sweep_all(gc_t *gc);

// statistics
size_t gc_count_swept(gc_t *gc);
size_t gc_bytes_freed_last_sweep(gc_t *gc);


#endif /* GC_SWEEP_H */
