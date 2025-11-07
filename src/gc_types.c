#include "gc_types.h"


bool gc_init_header(obj_header_t *header, obj_type_t type, size_t size) {
  if (!header || size == 0) {
    return false;
  }

  header->type = type;
  header->size = size;
  header->marked = false;
  header->next = NULL;

  header->age = 0;
  header->generation = 0;

  return true;
}

bool gc_is_valid_header(const obj_header_t *header) {
  if (!header
      || header->type < OBJ_TYPE_UNKNOWN
      || header->type > OBJ_TYPE_STRUCT
      || header->size == 0
      ) {
    return false;
  }
  return true;
}
