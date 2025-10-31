#ifndef SIMPLE_GC_H
#define SIMPLE_GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIMPLE_GC_VERSION_MAJOR 0
#define SIMPLE_GC_VERSION_MINOR 1
#define SIMPLE_GC_VERSION_PATCH 0

typedef struct obj_header obj_header_t;
typedef struct gc_context gc_t;
typedef struct reference_node ref_node_t;

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

} gc_t;

typedef struct reference_node {
  void *from_obj;
  void *to_obj;
  struct reference_node *next;
} ref_node_t;


const char *simple_gc_version(void);

// object header management
bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool simple_gc_is_valid_header(const obj_header_t *header);

// GC initialization/cleanup
gc_t *simple_gc_new(size_t init_capacity);
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


#endif /* SIMPLE_GC_H */
