#ifndef SIMPLE_GC_H
#define SIMPLE_GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIMPLE_GC_VERSION_MAJOR 0
#define SIMPLE_GC_VERSION_MINOR 1
#define SIMPLE_GC_VERSION_PATCH 0

typedef enum {
  OBJ_TYPE_UNKNOWN = 0,
  OBJ_TYPE_PRIMITIVE,
  OBJ_TYPE_ARRAY,
  OBJ_TYPE_STRUCT,
} obj_type_t;

typedef struct obj_header obj_header_t;
typedef struct obj_header {
  obj_type_t type;
  size_t size;
  bool marked;
  obj_header_t *next;
} obj_header_t;

const char *simple_gc_version(void);
bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool simple_gc_is_valid_header(const obj_header_t *header);

#endif /* SIMPLE_GC_H */
