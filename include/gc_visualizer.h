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


typedef struct {
  size_t object_count;
  size_t heap_used;
  void **object_ptrs;
  bool *marked_states;
} gc_snapshot_t;

gc_viz_config_t gc_viz_default_config(void);

// utils
const char *gc_viz_type_string(obj_type_t type);
void gc_viz_separator(const gc_viz_config_t *config, char c, int width);
void gc_viz_clear_screen(void);

// basic visualizations
void gc_viz_heap_bar(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_object_list(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_reference_graph(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_stats_dashboard(const gc_t *gc, const gc_viz_config_t *config);
void gc_viz_full_state(const gc_t *gc, const gc_viz_config_t *config);

// snapshot
gc_snapshot_t *gc_viz_snapshot(const gc_t *gc);
void gc_viz_free_snapshot(gc_snapshot_t *snapshot);
void gc_viz_diff(const gc_snapshot_t *before, const gc_snapshot_t *after, const gc_viz_config_t *config);


#endif /* GC_VISUALIZER_H */
