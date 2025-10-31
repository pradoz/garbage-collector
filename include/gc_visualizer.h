#ifndef GC_VISUALIZER_H
#define GC_VISUALIZER_H

#include "simple_gc.h"
#include <stdio.h>


typedef struct {
  bool show_addresses;
  bool use_colors;
  int graph_width;
  FILE *output; // default display stdout
} gc_viz_config_t;


gc_viz_config_t gc_viz_default_config(void);

// utils
const char *gc_viz_type_string(obj_type_t type);
void gc_viz_separator(const gc_viz_config_t *config, char c, int width);
void gc_viz_clear_screen(void);

void gc_viz_heap_bar(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_object_list(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_reference_graph(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_stats_dashboard(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_full_state(const gc_t *gc, const gc_viz_config_t *config);



#endif /* GC_VISUALIZER_H */
