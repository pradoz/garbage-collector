#ifndef GC_TRACE_H
#define GC_TRACE_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "gc_types.h"


typedef struct gc_context gc_t;
typedef struct gc_trace_context gc_trace_t;


typedef enum {
  GC_EVENT_ALLOC,
  GC_EVENT_FREE,
  GC_EVENT_COLLECT_START,
  GC_EVENT_COLLECT_END,
  GC_EVENT_MARK_START,
  GC_EVENT_MARK_END,
  GC_EVENT_SWEEP_START,
  GC_EVENT_SWEEP_END,
  GC_EVENT_COMPACT_START,
  GC_EVENT_COMPACT_END,
  GC_EVENT_PRESSURE_CHANGE,
  GC_EVENT_PROMOTION,
  GC_EVENT_ROOT_ADD,
  GC_EVENT_ROOT_REMOVE,
} gc_event_type_t;

// trace event
typedef struct {
  gc_event_type_t type;
  uint64_t timestamp_ns;
  uint32_t thread_id;

  union {
    struct {
      void *address;
      size_t size;
      obj_type_t obj_type;
      const char *file;
      int line;
    } alloc;

    struct {
      void *address;
      size_t size;
    } free;

    struct {
      const char *type; // minor | major | full
      size_t objects_before;
      size_t bytes_before;
    } collect_start;

    struct {
      size_t objects_after;
      size_t bytes_after;
      size_t collected;
      size_t promoted;
      double duration_ms;
    } collect_end;

    struct {
      int pressure_level; // [0, 4]
    } pressure;

    struct {
      void *address;
      uint8_t old_gen;
      uint8_t new_gen;
    } promotion;
  } data;
} gc_trace_event_t;

typedef enum {
  GC_TRACE_FORMAT_TEXT,
  GC_TRACE_FORMAT_JSON,
  GC_TRACE_FORMAT_CHROME,
} gc_trace_format_t;

typedef struct {
  bool enabled;
  gc_trace_format_t format;
  FILE *output;
  size_t buffer_size;  // event buffer
  bool trace_allocs;
  bool trace_collections;
  bool trace_pressure;
  bool trace_promotions;
} gc_trace_config_t;

typedef struct {
  size_t total_events;
  size_t alloc_count;
  size_t free_count;
  size_t collection_count;
  size_t promotion_count;

  size_t total_allocated;
  size_t total_freed;
  size_t peak_memory;

  double total_gc_time_ms;
  double avg_gc_pause_ms;
  double max_gc_pause_ms;

  size_t objects_leaked; // allocated but not freed
} gc_trace_stats_t;


gc_trace_t *gc_trace_create(const gc_trace_config_t *config);
void gc_trace_destroy(gc_trace_t *trace);

bool gc_trace_begin(gc_t *gc, const char *filename, gc_trace_format_t format);
void gc_trace_end(gc_t *gc);
void gc_trace_flush(gc_t *gc);
void gc_trace_event(gc_t *gc, const gc_trace_event_t *event);

void gc_trace_get_stats(gc_t *gc, gc_trace_stats_t *stats);
void gc_trace_print_stats(gc_t *gc, FILE *out);

gc_trace_config_t gc_trace_default_config(void);


// convenience macros for common events
#define GC_TRACE_ALLOC(gc, addr, size, type, file, line) \
  do { \
    if ((gc)->trace) { \
      gc_trace_event_t event = { \
        .type = GC_EVENT_ALLOC, \
        .data.alloc = {addr, size, type, file, line} \
      }; \
        gc_trace_event(gc, &event); \
    } \
  } while(0)

#define GC_TRACE_COLLECT_START(gc, ctype, objs, bytes) \
  do { \
    if ((gc)->trace) { \
      gc_trace_event_t event = { \
        .type = GC_EVENT_COLLECT_START, \
        .data.collect_start = {ctype, objs, bytes} \
      }; \
        gc_trace_event(gc, &event); \
    } \
  } while(0)

#define GC_TRACE_COLLECT_END(gc, objs, bytes, collected, promoted, duration) \
  do { \
    if ((gc)->trace) { \
      gc_trace_event_t event = { \
        .type = GC_EVENT_COLLECT_END, \
        .data.collect_end = {objs, bytes, collected, promoted, duration} \
      }; \
        gc_trace_event(gc, &event); \
    } \
  } while(0)


#endif /* GC_TRACE_H */
