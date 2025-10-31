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
} gc_t;

const char *simple_gc_version(void);
bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool simple_gc_is_valid_header(const obj_header_t *header);
gc_t *simple_gc_new(size_t init_capacity);
bool simple_gc_init(gc_t *gc, size_t init_capacity);
void simple_gc_destroy(gc_t *gc);
size_t simple_gc_object_count(const gc_t *gc);
size_t simple_gc_heap_capacity(const gc_t *gc);
size_t simple_gc_heap_used(const gc_t *gc);
void *simple_gc_alloc(gc_t *gc, obj_type_t type, size_t size);
obj_header_t *simple_gc_find_header(gc_t *gc, void *ptr);

#endif /* SIMPLE_GC_H */
