#include "gc_mark.h"
#include "simple_gc.h"
#include <stdlib.h>


void gc_mark_object(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return;

  obj_header_t* header = simple_gc_find_header(gc, ptr);
  if (!header || header->marked) return;

  // object is reachable
  header->marked = true;

  ref_node_t* ref = gc->references;
  while (ref) {
    if (ref->from_obj == ptr) {
      gc_mark_object(gc, ref->to_obj);
    }
    ref = ref->next;
  }
}

void gc_mark_all_roots(gc_t *gc) {
  if (!gc) return;

  for (size_t i = 0; i < gc->root_count; ++i) {
    gc_mark_object(gc, gc->roots[i]);
  }
}

// iterative marking using explicit stack (avoid stack overflow)
void gc_mark_object_iterative(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return;

  void **worklist = (void**) malloc(sizeof(void*) * 1024);
  if (!worklist) return;

  size_t worklist_size = 0;
  size_t worklist_capacity = 1024;

  // add initial object to worklist
  worklist[worklist_size++] = ptr;

  while (worklist_size > 0) {
    // pop from worklist
    void *current = worklist[--worklist_size];

    obj_header_t *header = simple_gc_find_header(gc, current);
    if (!header || header->marked) {
      continue;
    }

    // mark object
    header->marked = true;

    // add children to worklist
    ref_node_t *ref = gc->references;
    while (ref) {
      if (ref->from_obj == current) {
        // grow worklist if needed
        if (worklist_size >= worklist_capacity) {
          size_t new_capacity = worklist_capacity * 2;
          void **new_worklist = (void**) realloc(worklist, sizeof(void*) * new_capacity);
          if (!new_worklist) {
            free(worklist);
            return;
          }
          worklist = new_worklist;
          worklist_capacity = new_capacity;
        }

        worklist[worklist_size++] = ref->to_obj;
      }
      ref = ref->next;
    }
  }

  free(worklist);
}

void gc_mark_all_roots_iterative(gc_t *gc) {
  if (!gc) return;

  for (size_t i = 0; i < gc->root_count; ++i) {
    gc_mark_object_iterative(gc, gc->roots[i]);
  }
}

bool gc_is_marked(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return false;

  obj_header_t *header = simple_gc_find_header(gc, ptr);
  if (!header) return false;

  return header->marked;
}

void gc_unmark_all(gc_t *gc) {
  if (!gc) return;

  // unmark pool objects
  if (gc->use_pools) {
    for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
      size_class_t *sc = &gc->size_classes[i];
      pool_block_t *block = sc->blocks;

      while (block) {
        char *slot = (char*)block->memory;
        for (size_t j = 0; j < block->capacity; ++j) {
          obj_header_t *header = (obj_header_t*)slot;

          // check if slot is in use
          bool in_use = true;
          free_node_t *free_node = block->free_list;
          while (free_node) {
            if ((void*)free_node == (void*)header) {
              in_use = false;
              break;
            }
            free_node = free_node->next;
          }

          if (in_use) {
            header->marked = false;
          }

          slot += block->slot_size;
        }
        block = block->next;
      }
    }

    // unmark large objects
    large_block_t *large = gc->large_blocks;
    while (large) {
      if (large->in_use && large->header) {
        large->header->marked = false;
      }
      large = large->next;
    }

    // unmark huge objects
    huge_object_t *huge = gc->huge_objects;
    while (huge) {
      if (huge->header) {
        huge->header->marked = false;
      }
      huge = huge->next;
    }
  }

  // unmark legacy objects
  obj_header_t *obj = gc->objects;
  while (obj) {
    obj->marked = false;
    obj = obj->next;
  }
}

size_t gc_count_marked(gc_t *gc) {
  if (!gc) return 0;

  size_t count = 0;

  // count pool objects
  if (gc->use_pools) {
    for (int i = 0; i < GC_NUM_SIZE_CLASSES; ++i) {
      size_class_t *sc = &gc->size_classes[i];
      pool_block_t *block = sc->blocks;

      while (block) {
        char *slot = (char*)block->memory;
        for (size_t j = 0; j < block->capacity; ++j) {
          obj_header_t *header = (obj_header_t*)slot;

          bool in_use = true;
          free_node_t *free_node = block->free_list;
          while (free_node) {
            if ((void*)free_node == (void*)header) {
              in_use = false;
              break;
            }
            free_node = free_node->next;
          }

          if (in_use && header->marked) {
            count++;
          }

          slot += block->slot_size;
        }
        block = block->next;
      }
    }

    // count large blocks
    large_block_t *large = gc->large_blocks;
    while (large) {
      if (large->in_use && large->header && large->header->marked) {
        count++;
      }
      large = large->next;
    }

    // count huge objects
    huge_object_t *huge = gc->huge_objects;
    while (huge) {
      if (huge->header && huge->header->marked) {
        count++;
      }
      huge = huge->next;
    }
  }

  // count legacy objects
  obj_header_t *obj = gc->objects;
  while (obj) {
    if (obj->marked) count++;
    obj = obj->next;
  }

  return count;
}

size_t gc_count_unmarked(gc_t *gc) {
  if (!gc) return 0;
  return gc->object_count - gc_count_marked(gc);
}
