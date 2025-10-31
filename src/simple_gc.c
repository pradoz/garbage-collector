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

  // GC context
  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
  gc->heap_capacity = init_capacity;

  // roots
  gc->root_capacity = 16;
  gc->root_count = 0;
  gc->roots = (void**) malloc(sizeof(void*) * gc->root_capacity);

  if (!gc->roots) {
    return false;
  }

  // refs
  gc->references = NULL;

  // stack scanning
  gc->stack_bottom = NULL;
  gc->auto_root_scan_enabled = false;

  return true;
}

void simple_gc_destroy(gc_t* gc) {
  if (!gc) {
    return;
  }

  // free gc objects
  obj_header_t* obj = gc->objects;
  while (obj) {
    obj_header_t* tmp = obj;
    obj = obj->next;
    free(tmp);
  }

  // free references
  ref_node_t* ref = gc->references;
  while (ref) {
    ref_node_t* tmp = ref;
    ref = ref->next;
    free(tmp);
  }

  // reset context
  gc->objects = NULL;
  gc->object_count = 0;
  gc->heap_used = 0;
  free(gc->roots);
  gc->roots = NULL;
  gc->root_count = 0;
  gc->root_capacity = 0;
  gc->references = NULL;
}

size_t simple_gc_object_count(const gc_t* gc) {
  return gc ? gc->object_count : 0;
}

size_t simple_gc_heap_capacity(const gc_t* gc) {
  return gc ? gc->heap_capacity : 0;
}

size_t simple_gc_heap_used(const gc_t *gc) {
  return gc ? gc->heap_used : 0;
}

void *simple_gc_alloc(gc_t *gc, obj_type_t type, size_t size) {
  if (!gc || size == 0) {
    return NULL;
  }

  // capacity check
  size_t total_size = sizeof(obj_header_t) + size;
  if (total_size + gc->heap_used > gc->heap_capacity) {
    return NULL;
  }

  // allocate total memory for object+header
  obj_header_t* header = (obj_header_t*) malloc(total_size);
  if (!header) {
    return NULL;
  }

  // verify we can initialize the header
  if (!simple_gc_init_header(header, type, size)) {
    free(header);
    return NULL;
  }

  header->next = gc->objects;
  gc->objects = header;

  // bookkeeping
  gc->object_count++;
  gc->heap_used += total_size;

  // return a pointer to data after the header
  return (void*)(header + 1);
}

obj_header_t *simple_gc_find_header(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return NULL;
  }

  obj_header_t* curr = gc->objects;
  while (curr) {
    void* obj = (void*)(curr + 1);
    if (obj == ptr) {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}

bool simple_gc_add_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  obj_header_t* header = simple_gc_find_header(gc, ptr);
  if (!header) {
    return false;
  }

  // check if we need to resize
  if (gc->root_count >= gc->root_capacity) {
    size_t new_capacity = gc->root_capacity * 2 + 1;
    void** new_roots = (void**) realloc(gc->roots, new_capacity * sizeof(void*));
    if (!new_roots) {
      return false;
    }

    gc->roots = new_roots;
    gc->root_capacity = new_capacity;
  }

  gc->roots[gc->root_count++] = ptr;

  return true;
}

bool simple_gc_remove_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  // find the root
  for (size_t i = 0; i < gc->root_count; ++i) {
    if (gc->roots[i] == ptr) {
      // shift all elements after, overwriting what we want to remove
      for (size_t j = i; j < gc->root_count - 1; ++j) {
        gc->roots[i] = gc->roots[j + 1];
      }
      // update and return found
      gc->root_count--;
      return true;
    }
  }
  return false;  // not found
}

bool simple_gc_is_root(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return false;
  }

  for (size_t i = 0; i < gc->root_count; ++i) {
    if (gc->roots[i] == ptr) {
      return true;
    }
  }

  return false;
}

void simple_gc_mark(gc_t *gc, void *ptr) {
  if (!gc || !ptr) {
    return;
  }

  obj_header_t* header = simple_gc_find_header(gc, ptr);
  if (!header || header->marked) {
    return;
  }

  // object is reachable
  header->marked = true;

  ref_node_t* ref = gc->references;
  while (ref) {
    if (ref->from_obj == ptr) {
      simple_gc_mark(gc, ref->to_obj);
    }
    ref = ref->next;
  }
}

void simple_gc_mark_roots(gc_t *gc) {
  if (!gc) {
    return;
  }

  for (size_t i = 0; i < gc->root_count; ++i) {
    simple_gc_mark(gc, gc->roots[i]);
  }
}

void simple_gc_sweep(gc_t *gc) {
  if (!gc) {
    return;
  }

  obj_header_t** curr = &gc->objects;
  while (*curr) {
    if (!(*curr)->marked) { // unreachable
      obj_header_t* tmp = *curr;
      *curr = (*curr)->next;

      gc->object_count--;
      gc->heap_used -= (sizeof(obj_header_t) + tmp->size);

      free(tmp);
    } else {
      (*curr)->marked = false;
      curr = &(*curr)->next;
    }
  }
}

void simple_gc_collect(gc_t *gc) {
  if (!gc) {
    return;
  }

  simple_gc_mark_roots(gc);
  simple_gc_sweep(gc);
}

bool simple_gc_add_reference(gc_t *gc, void *from_ptr, void *to_ptr) {
  if (!gc || !from_ptr || !to_ptr) {
    return false;
  }

  if (!simple_gc_find_header(gc, from_ptr) || !simple_gc_find_header(gc, to_ptr)) {
    return false;
  }

  ref_node_t* ref = (ref_node_t*) malloc(sizeof(ref_node_t));
  if (!ref) {
    return false;
  }

  ref->from_obj = from_ptr;
  ref->to_obj = to_ptr;

  ref->next = gc->references;
  gc->references = ref;

  return true;
}

bool simple_gc_remove_reference(gc_t *gc, void *from_ptr, void *to_ptr) {
  if (!gc || !from_ptr || !to_ptr) {
    return false;
  }

  ref_node_t** curr = &gc->references;
  while (*curr) {
    ref_node_t* ref = *curr;
    if (ref->from_obj == from_ptr && ref->to_obj == to_ptr) {
      *curr = ref->next;
      free(ref);
      return true;
    }
    curr = &(*curr)->next;
  }

  return false;
}

bool simple_gc_set_stack_bottom(gc_t *gc, void *hint) {
  if (!gc) return false;

  // where hint points to a local variable
  gc->stack_bottom = hint;
  return true;
}

void *simple_gc_get_stack_bottom(gc_t *gc) {
  return gc ? gc->stack_bottom : NULL;
}

bool simple_gc_enable_auto_roots(gc_t *gc, bool enable) {
  if (!gc) return false;
  gc->auto_root_scan_enabled = enable;
  return true;
}
