#include "simple_gc.h"
#include "gc_generation.h"
#include "gc_barrier.h"
#include "gc_cardtable.h"
#include <stdio.h>
#include <stdlib.h>

static void print_separator(const char *title) {
  printf("\n");
  printf("========================================\n");
  printf("  %s\n", title);
  printf("========================================\n\n");
}

static void print_gc_state(gc_t *gc) {
  gc_gen_t *gen = gc->gen_context;

  printf("Generation state:\n");
  printf("  Young: %zu objects, %zu bytes\n",
      gen->stats[GC_GEN_YOUNG].objects,
      gen->stats[GC_GEN_YOUNG].bytes_used);
  printf("  Old:   %zu objects, %zu bytes\n",
      gen->stats[GC_GEN_OLD].objects,
      gen->stats[GC_GEN_OLD].bytes_used);

  if (gen->cardtable.enabled) {
    printf("  Card table: %zu/%zu cards dirty (%.1f%%)\n",
        gc_cardtable_dirty_count(&gen->cardtable),
        gen->cardtable.num_cards,
        gc_cardtable_dirty_ratio(&gen->cardtable) * 100.0f);
  }
  printf("\n");
}

int main(void) {
  printf("=== Write Barrier & Card Table Demo ===\n");
  printf("Demonstrates how generational GC tracks cross-generation references\n");

  // create GC
  gc_t *gc = simple_gc_new(1024 * 1024);  // 1 MiB
  if (!gc) {
    fprintf(stderr, "Failed to create GC\n");
    return 1;
  }

  // enable generational GC
  if (!simple_gc_enable_generations(gc, 256 * 1024)) {  // 256 KiB young
    fprintf(stderr, "Failed to enable generational GC\n");
    simple_gc_destroy(gc);
    free(gc);
    return 1;
  }

  // enable write barrier with card marking
  if (!simple_gc_enable_write_barrier(gc)) {
    fprintf(stderr, "Failed to enable write barrier\n");
    simple_gc_destroy(gc);
    free(gc);
    return 1;
  }

  printf("Configuration:\n");
  printf("  Heap size: 1 MiB\n");
  printf("  Young generation: 256 KiB\n");
  printf("  Old generation: 768 KiB\n");
  printf("  Card size: %d bytes\n", GC_CARD_SIZE);
  printf("  Write barrier: Card marking\n");

  gc->config.auto_collect = false;

  // ===== Phase 1: Create old generation objects =====
  print_separator("Phase 1: Creating Old Generation Objects");

  printf("Allocating 5 long-lived objects...\n");
  for (int i = 0; i < 5; i++) {
    int *obj = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *obj = i * 100;
    simple_gc_add_root(gc, obj);
  }

  printf("Aging objects through %d minor collections...\n", GC_PROMOTION_AGE + 1);
  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(gc);
  }

  // update pointers after promotion
  int *old_objs[5];
  for (int i = 0; i < 5; i++) {
    old_objs[i] = (int *)gc->roots[i];
    obj_header_t *header = simple_gc_find_header(gc, old_objs[i]);
    printf("  old_objs[%d]: gen=%d, age=%d, value=%d\n",
           i, header->generation, header->age, *old_objs[i]);
  }

  print_gc_state(gc);

  // ===== Phase 2: Create references without barrier =====
  print_separator("Phase 2: Young Objects (No Cross-Gen Refs)");

  printf("Allocating 10 young objects (unreferenced)...\n");
  for (int i = 0; i < 10; i++) {
    int *young = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *young = i;
  }

  print_gc_state(gc);
  simple_gc_print_barrier_stats(gc);

  printf("Running minor collection (should collect all 10)...\n");
  simple_gc_collect_minor(gc);

  print_gc_state(gc);

  // ===== Phase 3: Create cross-generation references =====
  print_separator("Phase 3: Cross-Generation References");

  printf("Creating old->young references...\n");
  printf("Each old object will reference a new young object\n\n");

  int *young_objs[5];
  for (int i = 0; i < 5; i++) {
    young_objs[i] = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *young_objs[i] = i * 1000;

    printf("  old_objs[%d] (%p) -> young_objs[%d] (%p)\n",
        i, (void*)old_objs[i], i, (void*)young_objs[i]);

    // This triggers write barrier!
    simple_gc_add_reference(gc, old_objs[i], young_objs[i]);
    simple_gc_write(gc, old_objs[i], young_objs[i]);
  }

  printf("\n");
  print_gc_state(gc);

  gc_gen_t *gen = gc->gen_context;
  if (gen->cardtable.enabled) {
    printf("Card table marked cards dirty for old->young references\n");
    gc_cardtable_print_stats(&gen->cardtable);
  }

  simple_gc_print_barrier_stats(gc);

  // ===== Phase 4: Minor collection with remembered set =====
  print_separator("Phase 4: Minor Collection (Using Card Table)");

  printf("Allocating 100 more young objects (unreferenced)...\n");
  for (int i = 0; i < 100; i++) {
    simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  printf("Before minor collection:\n");
  print_gc_state(gc);

  printf("Running minor collection...\n");
  printf("Card table will identify old->young references\n");
  printf("Young objects referenced by old generation should survive\n\n");

  size_t young_before = gen->stats[GC_GEN_YOUNG].objects;
  simple_gc_collect_minor(gc);
  size_t young_after = gen->stats[GC_GEN_YOUNG].objects;

  printf("After minor collection:\n");
  print_gc_state(gc);

  printf("Results:\n");
  printf("  Young objects before: %zu\n", young_before);
  printf("  Young objects after:  %zu\n", young_after);
  printf("  Collected:            %zu\n", young_before - young_after);
  printf("  Survived (referenced): 5\n");

  // verify the 5 referenced young objects survived
  int survivors_found = 0;
  for (int i = 0; i < 5; i++) {
    obj_header_t *header = simple_gc_find_header(gc, young_objs[i]);
    if (header) {
      printf("  young_objs[%d]: survived (gen=%d, age=%d, value=%d)\n",
             i, header->generation, header->age, *young_objs[i]);
      survivors_found++;
    }
  }
  printf("Found %d/5 survivors\n", survivors_found);

  // ===== Phase 5: Performance comparison =====
  print_separator("Phase 5: Performance Benefits");

  printf("Write barrier overhead:\n");
  gc_barrier_stats_t stats;
  gc_barrier_get_stats(gc, &stats);

  printf("  Total writes tracked: %zu\n", stats.total_writes);
  printf("  Cross-generation:     %zu (%.1f%%)\n",
         stats.barrier_hits,
         stats.total_writes > 0
           ? (float)stats.barrier_hits / stats.total_writes * 100.0f
           : 0.0f);
  printf("  Same generation:      %zu (%.1f%%)\n",
         stats.same_generation,
         stats.total_writes > 0
           ? (float)stats.same_generation / stats.total_writes * 100.0f
           : 0.0f);

  printf("\nCard table efficiency:\n");
  if (gen->cardtable.enabled) {
    printf("  Total cards:   %zu\n", gen->cardtable.num_cards);
    printf("  Dirty cards:   %zu\n", gc_cardtable_dirty_count(&gen->cardtable));
    printf("  Dirty ratio:   %.2f%%\n",
           gc_cardtable_dirty_ratio(&gen->cardtable) * 100.0f);
    printf("  Cards scanned: Much smaller than full heap!\n");
  }

  printf("\nGenerational GC benefits:\n");
  printf("  Minor collections: %zu (fast - young gen only)\n", gen->minor_count);
  printf("  Major collections: %zu (slow - full heap)\n", gen->major_count);
  printf("  Avg minor pause:   %.3f ms\n",
         gen->stats[GC_GEN_YOUNG].collections > 0
           ? gen->stats[GC_GEN_YOUNG].total_time_ms / gen->stats[GC_GEN_YOUNG].collections
           : 0.0);
  printf("  Total GC time:     %.3f ms\n",
         gen->stats[GC_GEN_YOUNG].total_time_ms + gen->stats[GC_GEN_OLD].total_time_ms);

  // ===== Final statistics =====
  print_separator("Final Statistics");
  simple_gc_print_gen_stats(gc);
  simple_gc_print_barrier_stats(gc);

  // cleanup
  simple_gc_destroy(gc);
  free(gc);

  printf("=== Demo Complete ===\n");
  printf("\nKey Takeaways:\n");
  printf("* Most objects die young -> collected quickly\n");
  printf("* Survivors age and promote to old generation\n");
  printf("* Write barrier tracks old->young references\n");
  printf("* Card table minimizes work during minor GC\n");
  printf("* Result: Fast, frequent minor collections\n\n");

  return 0;
}
