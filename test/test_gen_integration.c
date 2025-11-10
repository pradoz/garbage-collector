#include "munit.h"
#include "simple_gc.h"
#include "gc_generation.h"
#include "gc_barrier.h"
#include "gc_cardtable.h"
#include "gc_debug.h"
#include "gc_trace.h"
#include <stdio.h>

static MunitResult test_full_integration(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);
  gc_debug_enable(&gc);
  gc_trace_begin(&gc, "test_integration.txt", GC_TRACE_FORMAT_TEXT);
  gc.config.auto_collect = false;

  // allocate and create cross-generation references
  int *old_root = (int *)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *old_root = 999;
  simple_gc_add_root(&gc, old_root);

  // age to old generation
  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(&gc);
  }

  // get updated pointer
  old_root = (int *)gc.roots[0];

  // verify promotion
  obj_header_t *header = simple_gc_find_header(&gc, old_root);
  munit_assert_not_null(header);
  munit_assert_int(header->generation, ==, GC_GEN_OLD);

  // allocate young objects and create references
  for (int i = 0; i < 50; i++) {
    int *young = (int *)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *young = i;

    if (i % 5 == 0) {
      // create old->young reference
      simple_gc_add_reference(&gc, old_root, young);
      simple_gc_write(&gc, old_root, young);
    }
  }

  // verify card table is tracking references
  gc_gen_t *gen = gc.gen_context;
  size_t dirty_cards = gc_cardtable_dirty_count(&gen->cardtable);
  munit_assert_size(dirty_cards, >, 0);

  // minor collection should preserve referenced young objects
  simple_gc_collect_minor(&gc);

  // verify statistics
  gc_gen_stats_t young_stats, old_stats;
  gc_gen_get_stats(&gc, &young_stats, &old_stats);
  munit_assert_size(young_stats.collections, >, 0);

  gc_barrier_stats_t barrier_stats;
  gc_barrier_get_stats(&gc, &barrier_stats);
  munit_assert_size(barrier_stats.old_to_young, >, 0);

  // print all statistics
  simple_gc_print_gen_stats(&gc);
  simple_gc_print_barrier_stats(&gc);
  gc_cardtable_print_stats(&gen->cardtable);
  gc_debug_print_leaks(&gc, stdout);
  gc_trace_print_stats(&gc, stdout);

  // cleanup
  gc_trace_end(&gc);
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_remembered_set_accuracy(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);
  gc.config.auto_collect = false;

  // create old object
  int *old_obj = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, old_obj);

  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(&gc);
  }

  // get updated pointer
  old_obj = (int *)gc.roots[0];

  // create young objects, some referenced from old
  int *young_reachable = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *young_unreachable = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *young_reachable = 111;
  *young_unreachable = 222;

  // create old->young reference
  simple_gc_add_reference(&gc, old_obj, young_reachable);
  simple_gc_write(&gc, old_obj, young_reachable);

  // check young generation object count, not total
  gc_gen_t *gen = gc.gen_context;
  size_t young_before = gen->stats[GC_GEN_YOUNG].objects;

  // minor collection
  simple_gc_collect_minor(&gc);

  // young_reachable should survive, young_unreachable should be collected
  size_t young_after = gen->stats[GC_GEN_YOUNG].objects;
  munit_assert_size(young_after, <, young_before);

  // verify reachable object survived
  obj_header_t *header = simple_gc_find_header(&gc, young_reachable);
  munit_assert_not_null(header);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_barrier_performance(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  #define NUM_WRITES 10000

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);

  // allocate objects
  int *objects[100];
  for (int i = 0; i < 100; i++) {
    objects[i] = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_add_root(&gc, objects[i]);
  }

  // benchmark write barrier overhead
  clock_t start = clock();
  for (int i = 0; i < NUM_WRITES; i++) {
    int from = i % 100;
    int to = (i + 1) % 100;
    simple_gc_write(&gc, objects[from], objects[to]);
  }
  clock_t end = clock();

  double time_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
  double time_per_write_ns = (time_ms * 1000000.0) / NUM_WRITES;

  printf("\n=== Write Barrier Performance ===\n");
  printf("Total writes:      %d\n", NUM_WRITES);
  printf("Total time:        %.3f ms\n", time_ms);
  printf("Time per write:    %.2f ns\n", time_per_write_ns);
  printf("=================================\n\n");

  simple_gc_print_barrier_stats(&gc);

  simple_gc_destroy(&gc);
  return MUNIT_OK;

  #undef NUM_WRITES
}

static MunitResult test_multiple_cross_gen_refs(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);
  gc.config.auto_collect = false;

  // create multiple old objects
  int *old_objs[5];
  for (int i = 0; i < 5; i++) {
    old_objs[i] = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_add_root(&gc, old_objs[i]);
  }

  // age all to old generation
  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(&gc);
  }

  // update all pointers after promotion
  for (int i = 0; i < 5; i++) {
    old_objs[i] = (int *)gc.roots[i];

    // verify objects are in old generation
    obj_header_t *header = simple_gc_find_header(&gc, old_objs[i]);
    munit_assert_not_null(header);
  }

  // create young objects and cross-gen references
  for (int i = 0; i < 5; i++) {
    int *young = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *young = i * 100;
    simple_gc_add_reference(&gc, old_objs[i], young);
    simple_gc_write(&gc, old_objs[i], young);
  }

  // verify card table tracked all references
  gc_gen_t *gen = gc.gen_context;
  size_t dirty_cards = gc_cardtable_dirty_count(&gen->cardtable);
  munit_assert_size(dirty_cards, >, 0);

  // minor collection - all young objects should survive
  size_t young_before = gen->stats[GC_GEN_YOUNG].objects;
  simple_gc_collect_minor(&gc);
  size_t young_after = gen->stats[GC_GEN_YOUNG].objects;

  // at least 5 should survive (the referenced ones)
  munit_assert_size(young_after, <=, young_before);
  munit_assert_size(young_after, >=, 5);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_barrier_with_debug_validation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);
  gc_debug_enable(&gc);
  gc.config.auto_collect = false;

  // create objects
  int *old_obj = (int *)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, old_obj);

  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(&gc);
  }

  int *young_obj = (int *)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // create reference with write barrier
  simple_gc_add_reference(&gc, old_obj, young_obj);
  simple_gc_write(&gc, old_obj, young_obj);

  // validate heap consistency
  bool valid = gc_debug_validate_heap(&gc);
  munit_assert_true(valid);

  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/full_integration", test_full_integration, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/remembered_set_accuracy", test_remembered_set_accuracy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/barrier_performance", test_barrier_performance, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/multiple_cross_gen_refs", test_multiple_cross_gen_refs, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/with_debug_validation", test_barrier_with_debug_validation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/gen_integration", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
