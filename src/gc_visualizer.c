#include "gc_visualizer.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


// colors!
#define ANSI_RESET   "\x1b[0m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_BOLD    "\x1b[1m"


gc_viz_config_t gc_viz_default_config(void) {
  gc_viz_config_t config = {
    .show_addresses = true,
    .use_colors = true,
    .graph_width = 50,
    .output = stdout,
  };
  return config;
}

const char *gc_viz_type_string(obj_type_t type) {
  switch (type) {
    case OBJ_TYPE_PRIMITIVE: return "PRIM";
    case OBJ_TYPE_ARRAY: return "ARRAY";
    case OBJ_TYPE_STRUCT: return "STRUCT";
    case OBJ_TYPE_UNKNOWN: return "UNKNOWN";
    default: return "UNKNOWN";
  }
}

void gc_viz_separator(const gc_viz_config_t *config, char c, const int width) {
  FILE *out = config->output;
  for (int i = 0; i < width; ++i) {
    fputc(c, out);
  }
  fputc('\n', out);
}

void gc_viz_clear_screen(void) {
  system("clear");
  // #ifdef _WIN32
  //   system("cls");
  // #else
  //   system("clear");
  // #endif
}

void gc_viz_heap_bar(const gc_t *gc, const gc_viz_config_t *config) {
  if (!gc || !config) return;

  FILE *out = config->output;
  size_t used = simple_gc_heap_used(gc);
  size_t capacity = simple_gc_heap_capacity(gc);

  if (capacity == 0) {
    fprintf(out, "Heap: [empty]\n");
    return;
  }

  int bar_width = config->graph_width;
  int filled = (int)((double) used / capacity * bar_width);
  int percent = (int)((double) used / capacity * 100);

  if (config->use_colors) {
    fprintf(out, "%sHeap Memory Layout%s (%zu bytes / %zu capacity)\n",
            ANSI_BOLD, ANSI_RESET, used, capacity);
  } else {
    fprintf(out, "Heap Memory Layout (%zu bytes / %zu capacity)\n",
            used, capacity);
  }

  fprintf(out, "[");
  for (int i = 0; i < bar_width; ++i) {
    if (i < filled) {
      if (config->use_colors) {
        fprintf(out, "%s#%s", ANSI_GREEN, ANSI_RESET);

      } else {
        fprintf(out, "#");
      }
    } else {
      fprintf(out, ".");
    }
  }

  fprintf(out, "] %d%% used", percent);
}

void gc_viz_object_list(const gc_t *gc, const gc_viz_config_t *config) {
  if (!gc || !config) return;

  FILE *out = config->output;
  size_t count = simple_gc_object_count(gc);

  if (config->use_colors) {
    fprintf(out, "%s\nObjects%s (%zu):\n", ANSI_BOLD, ANSI_RESET, count);

  } else {
    fprintf(out, "\nObjects (%zu):\n", count);
  }

  if (count == 0) {
    fprintf(out, "[none]");
  }

  obj_header_t* curr = gc->objects;
  while (curr) {
    void *obj_ptr = (void*)(curr + 1);
    bool is_root = simple_gc_is_root((gc_t*) gc, obj_ptr);

    fprintf(out, "  ");
    if (config->show_addresses) {
      fprintf(out, "[%p] ", obj_ptr);
    }

    const char* type_str = gc_viz_type_string(curr->type);
    fprintf(out, "%s(%zu) ", type_str, curr->size);

    if (curr->marked) {
      if (config->use_colors) {
        fprintf(out, "%smarked%s ", ANSI_YELLOW, ANSI_RESET);
      } else {
        fprintf(out, "marked ");
      }
    } else {
      fprintf(out, "unmarked ");
    }

    if (is_root) {
      if (config->use_colors) {
        fprintf(out, "%s[ROOT]%s", ANSI_CYAN, ANSI_RESET);
      } else {
        fprintf(out, "[ROOT]");
      }
    }

    curr = curr->next;
  }
  fprintf(out, "\n");
}

void gc_viz_reference_graph(const gc_t *gc, const gc_viz_config_t *config) {
  if (!gc || !config) return;

  FILE *out = config->output;

  if (config->use_colors) {
    fprintf(out, "%sReference Graph%s:\n", ANSI_BOLD, ANSI_RESET);
  } else {
    fprintf(out, "Reference Graph:\n");
  }

  ref_node_t* ref = gc->references;
  if (!ref) {
    fprintf(out, " [no references]\n");
  }

  void* last = NULL;
  while (ref) {
    if (ref->from_obj != last) {
      if (last) {
        fprintf(out, "\n");
      }
      if (config->show_addresses) {
        fprintf(out, "  %p", ref->from_obj);
      } else {
        fprintf(out, "  obj_%p", ref->from_obj);
      }

      if (config->use_colors) {
        fprintf(out, " %s-->%s ", ANSI_GREEN, ANSI_RESET);
      } else {
        fprintf(out, " --> ");
      }
      last = ref->from_obj;
    } else {
      fprintf(out, ", ");
    }

    if (config->show_addresses) {
      fprintf(out, "  %p", ref->to_obj);
    } else {
      fprintf(out, "  obj_%p", ref->to_obj);
    }

    ref = ref->next;
  }
  fprintf(out, "\n");
}

void gc_viz_stats_dashboard(const gc_t *gc, const gc_viz_config_t *config) {
  if (!gc || !config) return;

  FILE *out = config->output;

  // TODO: probably rename this to widget width or something
  int width = config->graph_width;

  if (config->use_colors) {
    fprintf(out, "%sGC Statistics Dashboard%s\n", ANSI_BOLD, ANSI_RESET);
  } else {
    fprintf(out, "     GC Statistics Dashboard\n");
  }

  gc_viz_separator(config, '=', width);
  fprintf(out, " Objects:        %zu\n", simple_gc_object_count(gc));
  fprintf(out, " Heap Used:      %zu / %zu bytes\n", simple_gc_heap_used(gc), simple_gc_heap_capacity(gc));

  double usage_percent = 0.0;
  if (simple_gc_heap_capacity(gc) > 0) {
    usage_percent = (double) simple_gc_heap_used(gc) / simple_gc_heap_capacity(gc)* 100;
  }

  fprintf(out, " Usage:          %.1f%%\n", usage_percent);
  fprintf(out, " Roots:          %zu\n", gc->root_count);

  size_t ref_count = 0;
  ref_node_t* ref = gc->references;
  while (ref) {
    ++ref_count;
    ref = ref->next;
  }

  fprintf(out, " References:     %zu\n", ref_count);
  gc_viz_separator(config, '=', width);
}

void gc_viz_full_state(const gc_t *gc, const gc_viz_config_t *config) {
  if (!gc || !config) return;

  gc_viz_stats_dashboard(gc, config);
  gc_viz_heap_bar(gc, config);
  gc_viz_object_list(gc, config);
  gc_viz_reference_graph(gc, config);

  fprintf(config->output, "\n");
}

gc_snapshot_t *gc_viz_snapshot(const gc_t *gc) {
  if (!gc) return NULL;

  gc_snapshot_t* snapshot = (gc_snapshot_t*) malloc(sizeof(gc_snapshot_t));
  if (!snapshot) return NULL;

  snapshot->object_count = gc->object_count;
  snapshot->heap_used = gc->heap_used;

  if (snapshot->object_count == 0) {
    snapshot->object_ptrs = NULL;
    snapshot->marked_states = NULL;
    return snapshot;
  }

  // allocate arrays
  snapshot->object_ptrs = (void**) malloc(sizeof(void*) * snapshot->object_count);;
  snapshot->marked_states  = (bool*) malloc(sizeof(bool) * snapshot->object_count);;

  if (!snapshot->object_ptrs || !snapshot->marked_states) {
    gc_viz_free_snapshot(snapshot);
    return NULL;
  }

  // copy object data
  obj_header_t* curr = gc->objects;
  size_t i = 0;
  while (curr && i < snapshot->object_count) {
    snapshot->object_ptrs[i] = (void*)(curr + 1);
    snapshot->marked_states[i] = curr->marked;
    curr = curr->next;
    ++i;
  }

  return snapshot;
}

void gc_viz_free_snapshot(gc_snapshot_t *snapshot) {
  if (!snapshot) return;
  free(snapshot->object_ptrs);
  free(snapshot->marked_states);
  free(snapshot);
}

void gc_viz_diff(const gc_snapshot_t *before, const gc_snapshot_t *after, const gc_viz_config_t *config) {
  if (!before || !after || !config) return;

  FILE *out = config->output;
  if (config->use_colors) {
     fprintf(out, "\n%sGC State Diff%s:\n", ANSI_BOLD, ANSI_RESET);
  } else {
     fprintf(out, "\nGC State Diff:\n");
  }

  // TODO: probably rename this to widget width or something
  int width = config->graph_width;
  gc_viz_separator(config, '-', width);

  // object count
  int obj_diff = (int)after->object_count - (int)before->object_count;
  fprintf(out, "Objects:      %zu -> %zu", before->object_count, after->object_count);

  if (obj_diff > 0) {
    if (config->use_colors) {
       fprintf(out, "%s(+%d)%s:\n", ANSI_GREEN, obj_diff, ANSI_RESET);
    } else {
       fprintf(out, "(+%d):\n", obj_diff);
    }
  } else if (obj_diff < 0) {
    if (config->use_colors) {
       fprintf(out, "%s(%d)%s:\n", ANSI_RED, obj_diff, ANSI_RESET);
    } else {
       fprintf(out, "(%d):\n", obj_diff);
    }
  } else {
    fprintf(out, "(no change)\n");
  }

  // heap
  int heap_diff = (int)after->heap_used - (int)before->heap_used;
  fprintf(out, "Heap Used:     %zu -> %zu", before->heap_used, after->heap_used);

  if (heap_diff > 0) {
    if (config->use_colors) {
       fprintf(out, "%s(+%d bytes)%s:\n", ANSI_YELLOW, heap_diff, ANSI_RESET);
    } else {
       fprintf(out, "(+%d bytes):\n", heap_diff);
    }
  } else if (heap_diff < 0) {
    if (config->use_colors) {
       fprintf(out, "%s(%d bytes)%s:\n", ANSI_GREEN, heap_diff, ANSI_RESET);
    } else {
       fprintf(out, "(%d bytes):\n", heap_diff);
    }
  } else {
    fprintf(out, "(no change)\n");
  }

  // collected objects
  size_t collected = 0;
  for (size_t i = 0; i < before->object_count; ++i) {
    bool found = false;
    for (size_t j = 0; j < after->object_count; ++j) {
      if (before->object_ptrs[i] == after->object_ptrs[j]) {
        found = true;
        break;
      }
    }
    if (!found) ++collected;
  }

  if (collected > 0) {
    if (config->use_colors) {
      fprintf(out, "%sCollected:    %zu objects%s\n", ANSI_RED, collected, ANSI_RESET);
    } else {
      fprintf(out, "Collected:    %zu objects\n", collected);
    }
  }
  gc_viz_separator(config, '-', width);

}
