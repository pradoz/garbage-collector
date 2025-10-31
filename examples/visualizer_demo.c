#include "simple_gc.h"
#include "gc_visualizer.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


int main(void) {
  printf("=== Garbage Collection Visualizer ===\n\n");

  // initialize GC
  gc_t *gc = simple_gc_new(2048);
  if (!gc) {
    fprintf(stderr, "Failed to create GC\n");
    return 1;
  }

  gc_viz_config_t config = gc_viz_default_config();

  printf("Empty GC\n");
  gc_viz_full_state(gc, &config);
  printf("\nPress Enter to continue...");
  getchar();

  // allocate some objects
  printf("\nAllocating objects...\n");
  int *num1 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *num2 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  char *str = (char *)simple_gc_alloc(gc, OBJ_TYPE_ARRAY, 100);

  *num1 = 42;
  *num2 = 99;
  snprintf(str, 100, "Hello, GC!");

  gc_viz_full_state(gc, &config);
  printf("\nPress Enter to continue...");
  getchar();

  // add root
  printf("\nAdding num1 as root...\n");
  simple_gc_add_root(gc, num1);
  gc_viz_full_state(gc, &config);
  printf("\nPress Enter to continue...");
  getchar();

  // add references
  printf("\nCreating reference graph...\n");
  void *struct1 = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 64);
  void *struct2 = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 64);
  void *struct3 = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 64);

  simple_gc_add_root(gc, struct1);
  simple_gc_add_reference(gc, struct1, struct2);
  simple_gc_add_reference(gc, struct1, num1);
  simple_gc_add_reference(gc, struct2, str);
  simple_gc_add_reference(gc, struct2, struct3);
  simple_gc_add_reference(gc, struct3, struct1);

  gc_viz_full_state(gc, &config);
  printf("\nPress Enter to continue...");
  getchar();

  printf("\nMarking struct1...\n");
  simple_gc_mark(gc, (void*)struct1);
  // simple_gc_sweep(gc);

  gc_viz_full_state(gc, &config);
  printf("\nPress Enter to continue...");
  getchar();

  // TODO: snapshot
  // printf("\nSnapshotting before GC...\n");
  // gc_snapshot_t *before = gc_viz_snapshot(gc);

  // run collection
  printf("Running garbage collection...\n");
  simple_gc_collect(gc);

  // gc_snapshot_t *after = gc_viz_snapshot(gc);

  gc_viz_full_state(gc, &config);
  printf("\n");
  // TODO: diff
  // gc_viz_diff(before, after, &config);

  printf("\nPress Enter to continue...");
  getchar();

  // cleanup
  printf("\nCleaning up...\n");
  // TODO: snapshot
  // gc_viz_free_snapshot(before);
  // gc_viz_free_snapshot(after);
  simple_gc_destroy(gc);
  free(gc);

  printf("Done\n");

  return 0;
}
