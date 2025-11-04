#ifndef GC_TYPES_H
#define GC_TYPES_H

#include <stdbool.h>
#include <stddef.h>


typedef struct obj_header obj_header_t;

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


bool gc_init_header(obj_header_t *header, obj_type_t type, size_t size);
bool gc_is_valid_header(const obj_header_t *header);


#endif /* GC_TYPES_H */
