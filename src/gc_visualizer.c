#include "gc_visualizer.h"
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
  }

  int bar_width = config->graph_width;
  int filled = (int)((double) used / capacity * bar_width);
  int percent = (int)((double) used / capacity * 100);

  // TODO: colors

  fprintf(out, "[");
  for (int i = 0; i < bar_width; ++i) {
    if (i < filled) {
      fprintf(out, "#");
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

  // TODO: colors
  fprintf(out, "Objects(%zu):\n", count);

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
      // TODO: colors
      fprintf(out, "marked ");
    } else {
      fprintf(out, "unmarked ");
    }

    if (is_root) {
      fprintf(out, "[ROOT]");
    }

    curr = curr->next;
  }
  fprintf(out, "\n");
}
