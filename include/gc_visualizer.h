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




#endif /* GC_VISUALIZER_H */
