#ifndef SIMPLE_GC_H
#define SIMPLE_GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// versioning
#define SIMPLE_GC_VERSION_MAJOR 0
#define SIMPLE_GC_VERSION_MINOR 2
#define SIMPLE_GC_VERSION_PATCH 0

// memory pools
#define GC_NUM_SIZE_CLASSES 6
#define GC_POOL_BLOCK_SIZE 4096  // 4KB blocks
#define GC_LARGE_OBJECT_THRESHOLD 256
#define GC_HUGE_OBJECT_THRESHOLD 4096
#define GC_SIZE_MAX 1024 * 1024 * 5  // 5 MB


extern const size_t GC_SIZE_CLASS_SIZES[GC_NUM_SIZE_CLASSES];


typedef struct obj_header obj_header_t;
typedef struct gc_context gc_t;
typedef struct reference_node ref_node_t;
typedef struct size_class size_class_t;
typedef struct large_block large_block_t;
typedef struct huge_object huge_object_t;


typedef enum {
  OBJ_TYPE_UNKNOWN = 0,
  OBJ_TYPE_PRIMITIVE,
  OBJ_TYPE_ARRAY,
  OBJ_TYPE_STRUCT,
} obj_type_t;

typedef struct obj_header {
  obj_type_t type;
  size_t size;
  bool marked;
  obj_header_t *next;
} obj_header_t;

typedef struct free_node {
  struct free_node *next;
} free_node_t;

typedef struct pool_block {
  void *memory;
  size_t slot_size;
  size_t capacity;
  size_t used;
  free_node_t *free_list;
  struct pool_block *next;
} pool_block_t;

typedef struct size_class {
  size_t size;
  size_t slot_size;
  pool_block_t *blocks;
  size_t total_capacity;
  size_t total_used;
  size_t total_allocated;
} size_class_t;

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

  // statistics
  size_t total_allocations;
  size_t total_collections;
  size_t total_bytes_allocated;
  size_t total_bytes_freed;
} gc_t;

typedef struct large_block {
  void *memory;
  size_t size;
  bool in_use;
  obj_header_t *header;
  large_block_t *next;
} large_block_t;

// >GC_HUGE_OBJECT_THRESHOLD, uses mmap
typedef struct huge_object {
  void *memory;
  size_t size;
  obj_header_t *header;
  huge_object_t *next;
} huge_object_t;

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
void simple_gc_mark(gc_t *gc, void *ptr);
void simple_gc_mark_roots(gc_t *gc);
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

// memory pools
int gc_size_to_class(size_t size);
size_class_t* gc_get_size_class(gc_t *gc, size_t size);
pool_block_t* gc_create_pool_block(size_t slot_size, size_t capacity);
void* gc_alloc_from_block(pool_block_t *block, obj_type_t type, size_t size);
void* gc_alloc_from_size_class(gc_t *gc, size_class_t* sc, obj_type_t type, size_t size);
void* gc_alloc_large(gc_t *gc, obj_type_t type, size_t size);
void* gc_alloc_large_object(gc_t *gc, obj_type_t type, size_t size);
void* gc_alloc_huge_object(gc_t *gc, obj_type_t type, size_t size);
bool gc_init_size_class(size_class_t *sc, size_t object_size);
bool gc_init_pools(gc_t *gc);
void gc_free_pool_block(pool_block_t *block);
void gc_destroy_size_class(size_class_t *sc);
void gc_destroy_pools(gc_t* gc);
bool gc_pointer_in_block(pool_block_t *block, void *ptr);
obj_header_t* gc_find_header_in_pools(gc_t *gc, void *ptr);
void gc_free_to_pool(gc_t *gc, obj_header_t *header);
void gc_sweep_pools(gc_t *gc);
void gc_sweep_large_objects(gc_t *gc);
void gc_sweep_huge_objects(gc_t *gc);

void simple_gc_get_stats(gc_t *gc, gc_stats_t *stats);
void simple_gc_print_stats(gc_t *gc);


#endif /* SIMPLE_GC_H */
