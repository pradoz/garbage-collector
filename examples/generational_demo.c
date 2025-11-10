#include "simple_gc.h"
#include "gc_generation.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  printf("=== Generational GC Demo ===\n\n");

  // create gc with 10MiB heap
  gc_t *gc = simple_gc_new(10 * 1024 * 1024);
  if (!gc) {
    fprintf(stderr, "Failed to create GC\n");
    return 1;
  }

  // enable generational gc with 2MiB young generation
  if (!simple_gc_enable_generations(gc, 2 * 1024 * 1024)) {
    fprintf(stderr, "Failed to enable generational GC\n");
    simple_gc_destroy(gc);
    free(gc);
    return 1;
  }

  printf("Generational GC enabled\n");
  printf("Young generation: 2 MiB\n");
  printf("Old generation:   8 MiB\n\n");

  // allocate many short-lived objects
  printf("Allocating 1000 short-lived objects...\n");
  for (int i = 0; i < 1000; i++) {
    int *temp = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (temp) *temp = i;
  }

  // allocate a few long-lived objects
  printf("Allocating 10 long-lived objects...\n");
  size_t survivor_indices[10];
  for (int i = 0; i < 10; i++) {
    int *survivor = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (survivor) {
      *survivor = i * 100;
      if (simple_gc_add_root(gc, survivor)) {
        survivor_indices[i] = gc->root_count - 1;
      }
    }
  }

  printf("\nBefore collection:\n");
  printf("\nBefore collection:\n");
  printf("Total objects: %zu\n", simple_gc_total_object_count(gc));
  printf("Young gen: %zu objects, %zu bytes\n",
      gc->gen_context->stats[GC_GEN_YOUNG].objects,
      gc->gen_context->stats[GC_GEN_YOUNG].bytes_used);
  printf("Old gen: %zu objects, %zu bytes\n",
      gc->gen_context->stats[GC_GEN_OLD].objects,
      gc->gen_context->stats[GC_GEN_OLD].bytes_used);

  // minor collection (fast)
  printf("\nRunning minor collection...\n");
  simple_gc_collect_minor(gc);

  printf("After minor collection:\n");
  printf("Total objects: %zu\n", simple_gc_total_object_count(gc));
  printf("Young gen: %zu objects, %zu bytes\n",
      gc->gen_context->stats[GC_GEN_YOUNG].objects,
      gc->gen_context->stats[GC_GEN_YOUNG].bytes_used);
  printf("Old gen: %zu objects, %zu bytes\n",
      gc->gen_context->stats[GC_GEN_OLD].objects,
      gc->gen_context->stats[GC_GEN_OLD].bytes_used);

  // allocate more and age survivors
  printf("\nAging survivors through multiple minor collections...\n");
  for (int cycle = 0; cycle < 5; cycle++) {
    printf("Cycle %d: ", cycle);
    for (int i = 0; i < 100; i++) {
      simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    }
    simple_gc_collect_minor(gc);
    printf("Young=%zu, Old=%zu, Promoted=%zu\n",
        gc->gen_context->stats[GC_GEN_YOUNG].objects,
        gc->gen_context->stats[GC_GEN_OLD].objects,
        gc->gen_context->stats[GC_GEN_YOUNG].promotions);
  }

  // verify survivors are still accessible (through roots)
  printf("\nVerifying survivors (through root array)...\n");
  int survivors_found = 0;
  for (int i = 0; i < 10; i++) {
    int *survivor = (int *)gc->roots[survivor_indices[i]];
    if (survivor && *survivor == i * 100) {
      survivors_found++;
    }
  }
  printf("Found %d/10 survivors with correct values\n", survivors_found);

  // print statistics
  simple_gc_print_gen_stats(gc);

  // cleanup
  simple_gc_destroy(gc);
  free(gc);

  printf("=== Demo Complete ===\n");
  return 0;
}
