#include "simple_gc.h"
#include <stdlib.h>

const char *simple_gc_version(void) { return "0.1.0"; }

bool simple_gc_init_header(obj_header_t *header, obj_type_t type, size_t size) {
  if (!header || size == 0) {
    return false;
  }

  header->type = type;
  header->size = size;
  header->marked = false;
  header->next = NULL;
  return true;
}

bool simple_gc_is_valid_header(const obj_header_t *header) {
  if (!header) {
    return false;
  }

  if (header->type < OBJ_TYPE_UNKNOWN || header->type > OBJ_TYPE_STRUCT) {
    return false;
  }

  if (header->size == 0) {
    return false;
  }
  return true;
}

gc_t* simple_gc_new(size_t init_capacity) {
  if (init_capacity == 0) {
    return NULL;
  }

  gc_t* gc = (gc_t*) malloc(sizeof(gc_t));
  if (!gc) {
    // free(gc);
    return NULL;
  }

  if (!simple_gc_init(gc, init_capacity)) {
    free(gc);
    return NULL;
  }

  return gc;
}

bool simple_gc_init(gc_t* gc, size_t init_capacity) {
  if (!gc || init_capacity == 0) {
    return false;
  }

  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
  gc->heap_capacity = init_capacity;

  return true;
}

void simple_gc_destroy(gc_t* gc) {
  if (!gc) {
    return;
  }

  // free gc objects
  obj_header_t* curr = gc->objects;
  while (curr) {
    obj_header_t* tmp = curr;
    curr = curr->next;
    free(tmp);
  }

  // reset context
  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
}

size_t simple_gc_object_count(const gc_t* gc) {
  return gc ? gc->object_count : 0;
}

size_t simple_gc_heap_capacity(const gc_t* gc) {
  return gc ? gc->heap_capacity : 0;
}
