#ifndef GC_DEBUG_H
#define GC_DEBUG_H


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "gc_types.h"


typedef struct gc_context gc_t;
typedef struct alloc_info alloc_info_t;


typedef struct alloc_info {
  void *address;
  size_t size;
  obj_type_t type;

  const char *file;
  int line;
  const char *function;

  uint64_t alloc_time;
  uint64_t alloc_id;
  uint32_t thread_id;

  bool freed;
  uint64_t free_time;

  alloc_info_t *next;
} alloc_info_t;

typedef struct gc_debug_context {
  alloc_info_t *allocations;
  size_t alloc_count;
  uint64_t next_alloc_id;

  bool track_stacks; // capture call stacks
  bool check_double_free;
  bool check_use_after_free;

  pthread_mutex_t lock;
} gc_debug_t;

typedef struct {
  size_t leaked_objects;
  size_t leaked_bytes;
  alloc_info_t **leaks;
} gc_leak_report_t;


bool gc_debug_enable(gc_t *gc);
void gc_debug_disable(gc_t *gc);
bool gc_debug_is_enabled(gc_t *gc);

void gc_debug_track_alloc(gc_t *gc, void *ptr, size_t size, obj_type_t type,
        const char *file, int line, const char *func);
void gc_debug_track_free(gc_t *gc, void *ptr);

gc_leak_report_t *gc_debug_find_leaks(gc_t *gc);
void gc_debug_free_leak_report(gc_leak_report_t *report);

alloc_info_t *gc_debug_find_alloc(gc_t *gc, void *ptr);
void gc_debug_print_alloc_info(alloc_info_t *info, FILE *out);

void gc_debug_print_leaks(gc_t *gc, FILE *out);
void gc_debug_print_alloc_info(alloc_info_t *info, FILE *out);

void gc_debug_dump_allocations(gc_t *gc, FILE *out);
bool gc_debug_validate_heap(gc_t *gc);
bool gc_debug_check_pointer(gc_t *gc, void *ptr);


#define GC_ALLOC_DEBUG(gc, type, size) \
  simple_gc_alloc_debug(gc, type, size, __FILE__, __LINE__, __func__)


#endif /* GC_DEBUG_H */
