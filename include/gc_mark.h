#ifndef GC_MARK_H
#define GC_MARK_H

#include <stdbool.h>
#include <stddef.h>
#include "gc_types.h"


typedef struct gc_context gc_t;
typedef struct reference_node ref_node_t;


void gc_mark_object(gc_t *gc, void *ptr);
void gc_mark_all_roots(gc_t *gc);

void gc_mark_object_iterative(gc_t *gc, void *ptr);
void gc_mark_all_roots_iterative(gc_t *gc);

bool gc_is_marked(gc_t *gc, void *ptr);
void gc_unmark_all(gc_t *gc);

size_t gc_count_marked(gc_t *gc);
size_t gc_count_unmarked(gc_t *gc);


#endif /* GC_MARK_H */
