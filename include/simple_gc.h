#ifndef SIMPLE_GC_H
#define SIMPLE_GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "gc_types.h"
#include "gc_pool.h"
#include "gc_large.h"
#include "gc_mark.h"


// versioning
#define SIMPLE_GC_VERSION_MAJOR 0
#define SIMPLE_GC_VERSION_MINOR 2
#define SIMPLE_GC_VERSION_PATCH 2


// wrappers for backward compatibility
bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool simple_gc_is_valid_header(const obj_header_t *header);


typedef struct gc_context gc_t;
typedef struct gc_config gc_config_t;
typedef struct reference_node ref_node_t;

typedef struct relocation_entry relocation_entry_t;
typedef struct compaction_ctx compaction_ctx_t;
typedef struct live_obj live_obj_t;


typedef enum {
  GC_PRESSURE_NONE = 0,
  GC_PRESSURE_LOW = 1,
  GC_PRESSURE_MEDIUM = 2,
  GC_PRESSURE_HIGH = 3,
  GC_PRESSURE_CRITICAL = 4,
} gc_pressure_t;

typedef struct relocation_entry {
  void *old_addr;
  void *new_addr;
  relocation_entry_t *next;
} relocation_entry_t;

typedef struct compaction_ctx {
  relocation_entry_t *relocations;
  size_t relocation_count;
  bool in_progress;
} compaction_ctx_t;

typedef struct live_obj {
    obj_header_t *header;
    void *data;
    pool_block_t *block;
} live_obj_t;

typedef struct gc_config {
  bool auto_collect;
  float collect_threshold;
  bool auto_expand_pools;
  bool auto_shrink_pools;
  size_t expansion_trigger;
} gc_config_t;

typedef struct gc_context {
  obj_header_t *objects; // linked-list
  size_t object_count;
  size_t heap_used;
  size_t heap_capacity;
  void **roots; // array of root pointers
  size_t root_count;
  size_t root_capacity;
  ref_node_t *references;

  // stack scanning
  void *stack_bottom;  // highest address (architecture assumption)
  bool auto_root_scan_enabled;

  // heap bounds
  void *heap_start;
  void *heap_end;

  // memory pools
  size_class_t size_classes[GC_NUM_SIZE_CLASSES];
  bool use_pools;
  large_block_t *large_blocks;
  size_t large_block_count;
  huge_object_t *huge_objects;
  size_t huge_object_count;
  compaction_ctx_t compaction;

  // memory pressure
  gc_config_t config;
  gc_pressure_t pressure;
  size_t allocs_since_collect;
  size_t alloc_rate; // estimate
  clock_t last_collect_time;
  clock_t last_alloc_time;
  double last_collection_duration;

  // statistics
  size_t total_allocations;
  size_t total_collections;
  size_t total_bytes_allocated;
  size_t total_bytes_freed;
  size_t total_compactions;
  size_t bytes_reclaimed;
} gc_t;

typedef struct reference_node {
  void *from_obj;
  void *to_obj;
  struct reference_node *next;
} ref_node_t;

typedef struct gc_stats {
  size_t object_count;
  size_t heap_used;
  size_t heap_capacity;
  size_t total_allocations;
  size_t total_collections;
  size_t large_block_count;
  size_t huge_object_count;
  size_t pool_blocks_allocated;
  size_t size_class_stats[GC_NUM_SIZE_CLASSES];
  size_t total_fragmented_bytes;
  float fragmentation_ratio;
} gc_stats_t;


const char *simple_gc_version(void);

// object header management
bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool simple_gc_is_valid_header(const obj_header_t *header);

// GC initialization/cleanup
gc_t *simple_gc_new(size_t init_capacity);
gc_t *simple_gc_new_auto(size_t init_capacity);
bool simple_gc_init(gc_t *gc, size_t init_capacity);
void simple_gc_destroy(gc_t *gc);

// GC stats
size_t simple_gc_object_count(const gc_t *gc);
size_t simple_gc_heap_capacity(const gc_t *gc);
size_t simple_gc_heap_used(const gc_t *gc);

// memory allocation/object management
void *simple_gc_alloc(gc_t *gc, obj_type_t type, size_t size);
obj_header_t *simple_gc_find_header(gc_t *gc, void *ptr);
bool simple_gc_add_root(gc_t *gc, void *ptr);
bool simple_gc_remove_root(gc_t *gc, void *ptr);

// TODO: move to utils?
bool simple_gc_is_root(gc_t *gc, void *ptr);

// mark and sweep
static inline void simple_gc_mark(gc_t *gc, void *ptr) {
  gc_mark_object(gc, ptr);
}
static inline void simple_gc_mark_roots(gc_t *gc) {
  gc_mark_all_roots(gc);
}
void simple_gc_sweep(gc_t *gc);
void simple_gc_collect(gc_t *gc);

// ref counting
bool simple_gc_add_reference(gc_t *gc, void *from_ptr, void *to_ptr);
bool simple_gc_remove_reference(gc_t *gc, void *from_ptr, void *to_ptr);

// stack scanning
bool simple_gc_set_stack_bottom(gc_t *gc, void *hint);
void *simple_gc_get_stack_bottom(gc_t *gc);
bool simple_gc_enable_auto_roots(gc_t *gc, bool enable);
bool simple_gc_is_heap_pointer(gc_t *gc, void *ptr);
void simple_gc_scan_stack(gc_t* gc);
bool simple_gc_auto_init_stack(gc_t *gc);

// compaction
bool simple_gc_should_compact(gc_t *gc);
void simple_gc_compact(gc_t *gc);

// memory pressure
gc_pressure_t simple_gc_check_pressure(gc_t *gc);
void simple_gc_set_config(gc_t *gc, gc_config_t *config);
void simple_gc_auto_tune(gc_t *gc);

// stats
void simple_gc_get_stats(gc_t *gc, gc_stats_t *stats);
void simple_gc_print_stats(gc_t *gc);


#endif /* SIMPLE_GC_H */
