#include "munit.h"
#include "simple_gc.h"
#include "gc_barrier.h"
#include "gc_generation.h"
#include <stdio.h>

static MunitResult test_init(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  bool result = simple_gc_enable_write_barrier(&gc);
  munit_assert_true(result);
  munit_assert_not_null(gc.barrier_context);

  simple_gc_disable_write_barrier(&gc);
  munit_assert_null(gc.barrier_context);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_basic_write(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  simple_gc_enable_write_barrier(&gc);

  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // trigger write barrier
  simple_gc_write(&gc, obj1, obj2);

  // verify statistics
  gc_barrier_stats_t stats;
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.total_writes, ==, 1);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_cross_generation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);
  gc.config.auto_collect = false;

  // create old object
  int *old_obj = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *old_obj = 999;
  simple_gc_add_root(&gc, old_obj);

  // age to old generation
  for (int i = 0; i <= GC_PROMOTION_AGE; ++i) {
    simple_gc_collect_minor(&gc);
  }

  // get updated pointer (object may move after promotion)
  old_obj = (int *)gc.roots[0];
  obj_header_t *old_header = simple_gc_find_header(&gc, old_obj);
  munit_assert_not_null(old_header);

  // create young object
  int *young_obj = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *young_obj = 111;

  // old -> young write
  simple_gc_write(&gc, old_obj, young_obj);

  // verify barrier detected cross-generational write
  gc_barrier_stats_t stats;
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.old_to_young, ==, 1);
  munit_assert_size(stats.barrier_hits, ==, 1);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_with_cardtable(const MunitParameter params[], void *data) {
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

  // age to old generation
  for (int i = 0; i <= GC_PROMOTION_AGE; i++) {
    simple_gc_collect_minor(&gc);
  }

  // get updated pointer
  old_obj = (int *)gc.roots[0];

  // verify card table is clean
  gc_gen_t *gen = gc.gen_context;
  munit_assert_size(gc_cardtable_dirty_count(&gen->cardtable), ==, 0);

  // create young object
  int *young_obj = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // old -> young write should mark card dirty
  simple_gc_write(&gc, old_obj, young_obj);

  // verify card is dirty
  munit_assert_size(gc_cardtable_dirty_count(&gen->cardtable), >, 0);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_same_generation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  simple_gc_enable_generations(&gc, 200 * 1024);
  simple_gc_enable_write_barrier(&gc);

  // two young objects
  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_write(&gc, obj1, obj2);

  // should count as same generation
  gc_barrier_stats_t stats;
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.same_generation, ==, 1);
  munit_assert_size(stats.barrier_hits, ==, 0);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_statistics(const MunitParameter params[], void *data) {
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

  // perform various writes
  for (int i = 0; i < 10; i++) {
    int *young = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_write(&gc, old_obj, young);  // old->young
  }

  simple_gc_print_barrier_stats(&gc);

  gc_barrier_stats_t stats;
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.total_writes, ==, 10);
  munit_assert_size(stats.old_to_young, ==, 10);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_reset_stats(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  simple_gc_enable_write_barrier(&gc);

  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_write(&gc, obj1, obj2);

  gc_barrier_stats_t stats;
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.total_writes, ==, 1);

  gc_barrier_reset_stats(&gc);
  gc_barrier_get_stats(&gc, &stats);
  munit_assert_size(stats.total_writes, ==, 0);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/init", test_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/basic_write", test_basic_write, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/cross_generation", test_cross_generation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/with_cardtable", test_with_cardtable, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/same_generation", test_same_generation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/statistics", test_statistics, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/reset_stats", test_reset_stats, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/barrier", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
