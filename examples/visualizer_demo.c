#include "simple_gc.h"
#include "gc_visualizer.h"
#include <stdio.h>
#include <stdlib.h>


void clear_and_title(const char *title) {
  gc_viz_clear_screen();
  printf("\n");
  printf(" ---------------------------------------------------------------- \n");
  printf("|                                                                |\n");
  printf("|              GARBAGE COLLECTION VISUALIZER DEMO                |\n");
  printf("|________________________________________________________________|\n");
  printf("|   %-60s |\n", title);
  printf("|________________________________________________________________|\n\n");
}

void pause_demo(const char *next_step) {
  printf("\n");
  printf("------------------------------------------------------------------\n");
  if (next_step) {
    printf("   Next: %s\n", next_step);
  }
  printf("Press ENTER to continue...\n");
  getchar();
}

int main(void) {
  gc_viz_config_t config = gc_viz_default_config();
  gc_t *gc = NULL;

  clear_and_title("Step 1: Initialize Empty Garbage Collector");

  gc = simple_gc_new(2048);
  if (!gc) {
    fprintf(stderr, "Failed to create GC\n");
    return 1;
  }

  printf("Created GC with 2048 bytes capacity\n\n");
  gc_viz_full_state(gc, &config);
  pause_demo("Allocate primitive objects");

  clear_and_title("Step 2: Allocate Primitive Objects");

  printf("Allocating integers and a string...\n\n");

  int *num1 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *num2 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *num3 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *num1 = 42;
  *num2 = 99;
  *num3 = 7;

  printf("  num1 = %d\n", *num1);
  printf("  num2 = %d\n", *num2);
  printf("  num3 = %d\n\n", *num3);

  gc_viz_full_state(gc, &config);
  pause_demo("Allocate array object");

  clear_and_title("Step 3: Allocate Array Object");

  printf("Allocating a 256-byte character array...\n\n");

  char *message = (char *)simple_gc_alloc(gc, OBJ_TYPE_ARRAY, 256);
  snprintf(message, 256, "Hello, Garbage Collector!");

  printf("  message = \"%s\"\n\n", message);

  gc_viz_full_state(gc, &config);
  pause_demo("Add root objects");

  clear_and_title("Step 4: Register Root Objects");

  printf("Adding num1 and message as GC roots...\n");
  printf("   (Roots are never collected - they're in scope)\n\n");

  simple_gc_add_root(gc, num1);
  simple_gc_add_root(gc, message);

  gc_viz_full_state(gc, &config);
  pause_demo("Create complex object graph");

  clear_and_title("Step 5: Build Complex Object Graph");

  printf("Creating structs with references...\n\n");

  void *person = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 128);
  void *address = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 64);
  void *company = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 96);
  void *project = simple_gc_alloc(gc, OBJ_TYPE_STRUCT, 80);

  printf("  Created: person, address, company, project\n\n");

  // Make person a root
  simple_gc_add_root(gc, person);

  // Create reference chain: person -> address -> company -> project
  simple_gc_add_reference(gc, person, address);
  simple_gc_add_reference(gc, person, num1);
  simple_gc_add_reference(gc, address, company);
  simple_gc_add_reference(gc, company, project);
  simple_gc_add_reference(gc, company, message);

  printf("  Reference structure:\n");
  printf("    person (ROOT) --> address --> company --> project\n");
  printf("           |                            |\n");
  printf("           ---> num1                    ---> message (ROOT)\n\n");

  gc_viz_full_state(gc, &config);
  pause_demo("Take snapshot before GC");

  clear_and_title("Step 6: Create Snapshot Before Collection");
  printf("Taking snapshot of current GC state...\n\n");

  gc_snapshot_t *before = gc_viz_snapshot(gc);

  printf("  Snapshot captured:\n");
  printf("    * Objects: %zu\n", before->object_count);
  printf("    * Heap used: %zu bytes\n\n", before->heap_used);

  gc_viz_full_state(gc, &config);
  pause_demo("Run garbage collection");

  clear_and_title("Step 7: Run Garbage Collection");

  printf("Running mark-and-sweep garbage collection...\n");
  printf("   * Mark phase: Starting from roots\n");
  printf("   * Sweep phase: Collecting unmarked objects\n\n");

  printf("  Objects that should be collected:\n");
  printf("    - num2 (not reachable)\n");
  printf("    - num3 (not reachable)\n\n");

  printf("  Objects that should survive:\n");
  printf("    + person (root)\n");
  printf("    + message (root)\n");
  printf("    + num1 (referenced by person)\n");
  printf("    + address, company, project (reachable from person)\n\n");

  size_t before_count = simple_gc_object_count(gc);
  size_t before_used = simple_gc_heap_used(gc);

  simple_gc_collect(gc);

  size_t after_count = simple_gc_object_count(gc);
  size_t after_used = simple_gc_heap_used(gc);

  printf("  Collection complete!\n");
  printf("    Objects: %zu → %zu (freed %zu)\n",
         before_count, after_count, before_count - after_count);
  printf("    Memory: %zu → %zu bytes (freed %zu)\n\n",
         before_used, after_used, before_used - after_used);

  gc_viz_full_state(gc, &config);
  pause_demo("Take snapshot after GC");

  clear_and_title("Step 8: Compare Before/After Snapshots");

  printf("Taking snapshot after collection...\n\n");

  gc_snapshot_t *after = gc_viz_snapshot(gc);

  printf("  Snapshot comparison:\n");
  printf("    Before: %zu objects, %zu bytes\n",
         before->object_count, before->heap_used);
  printf("    After:  %zu objects, %zu bytes\n\n",
         after->object_count, after->heap_used);

  gc_viz_diff(before, after, &config);
  pause_demo("Remove a root and collect again");

  clear_and_title("Step 9: Remove Root and Collect Again");

  printf("Removing 'person' root...\n");
  printf("   This should make the entire object graph unreachable!\n\n");

  simple_gc_remove_root(gc, person);

  printf("  Current roots: message only\n");
  printf("  Unreachable: person, address, company, project, num1\n\n");

  gc_viz_full_state(gc, &config);

  printf("\nRunning collection again...\n\n");

  gc_snapshot_t *before_second = gc_viz_snapshot(gc);
  simple_gc_collect(gc);
  gc_snapshot_t *after_second = gc_viz_snapshot(gc);

  printf("  Only message (root) should remain!\n\n");

  gc_viz_full_state(gc, &config);
  gc_viz_diff(before_second, after_second, &config);

  pause_demo("Clean up and exit");

  clear_and_title("Step 10: Cleanup");

  printf("Freeing snapshots and destroying GC...\n\n");

  gc_viz_free_snapshot(before);
  gc_viz_free_snapshot(after);
  gc_viz_free_snapshot(before_second);
  gc_viz_free_snapshot(after_second);
  simple_gc_destroy(gc);
  free(gc);

  printf(" Done\n\n");

  return 0;
}
