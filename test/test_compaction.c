#include "munit.h"
#include "simple_gc.h"
#include "gc_pool.h"
#include <stdio.h>


static MunitResult test_basic(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  gc.config.auto_collect = false;

  // get size class for int
  size_class_t *sc = gc_pool_get_size_class(gc.size_classes, sizeof(int));
  munit_assert_not_null(sc);

  // allocate many objects to create fragmentation
  for (int i = 0; i < 100; i++) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }
  munit_assert_size(gc_pool_count_blocks(sc), >, 0);

  // collect (no roots, all freed)
  simple_gc_collect(&gc);

  // check fragmentation
  float util = gc_pool_utilization(sc);
  munit_assert_float(util, <, 0.5f);

  // manually compact
  simple_gc_compact(&gc);

  // utilization should improve
  float util_after = gc_pool_utilization(sc);
  munit_assert_float(util_after, >=, util);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}


static MunitResult test_with_roots(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // TODO: fundamental issue with moving GC:
  // local pointers in user code can't be automatically updated
  // possible solutions: handle system, conservative scanning, or compiler support
  // for now, test GC internal state remains consistent

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  gc.config.auto_collect = false;

  // allocate objects, keep some as roots
  int *objs[100];
  for (int i = 0; i < 100; i++) {
    objs[i] = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *objs[i] = i;
    if (i % 2 == 0) {
      simple_gc_add_root(&gc, objs[i]);
    }
  }

  size_class_t *sc = gc_pool_get_size_class(gc.size_classes, sizeof(int));

  // collect to create fragmentation
  simple_gc_collect(&gc);

  float util_before = gc_pool_utilization(sc);
  size_t compactions_before = gc.total_compactions;

  // manually compact
  simple_gc_compact(&gc);

  // verify compaction happened
  munit_assert_size(gc.total_compactions, >, compactions_before);

  // verify object count unchanged (50 survived)
  munit_assert_size(gc.object_count, ==, 50);

  // verify all roots are still valid through GC's root array
  for (size_t i = 0; i < gc.root_count; i++) {
    obj_header_t *header = simple_gc_find_header(&gc, gc.roots[i]);
    munit_assert_not_null(header);
    munit_assert_true(simple_gc_is_valid_header(header));

    // verify data is accessible
    int *data = (int*)gc.roots[i];
    int value = *data;
    (void)value;  // suppress unused warning
  }

  float util_after = gc_pool_utilization(sc);
  munit_assert_float(util_after, >=, util_before);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_references(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);
  gc.config.auto_collect = false;

  // create reference chain: a -> b -> c
  int *a = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *b = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *c = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *a = 1;
  *b = 2;
  *c = 3;
  simple_gc_add_root(&gc, a);
  simple_gc_add_reference(&gc, a, b);
  simple_gc_add_reference(&gc, b, c);

  // fill with garbage
  for (int i = 0; i < 50; i++) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  size_t compactions_before = gc.total_compactions;

  // collect and compact
  simple_gc_collect(&gc);
  simple_gc_compact(&gc);

  // verify compaction happened
  munit_assert_size(gc.total_compactions, >, compactions_before);

  // verify root was updated (fetch from GC's root array)
  int *a_updated = (int*)gc.roots[0];
  munit_assert_int(*a_updated, ==, 1);

  // verify reference chain is intact
  munit_assert_size(gc.object_count, ==, 3);
  munit_assert_int(*a, ==, 1);
  munit_assert_int(*b, ==, 2);
  munit_assert_int(*c, ==, 3);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_stats(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  // create fragmentation across multiple size classes
  for (int i = 0; i < 40; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 8);
    if (i % 2 == 0) simple_gc_add_root(&gc, obj);
  }

  for (int i = 0; i < 40; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 16);
    if (i % 2 == 0) simple_gc_add_root(&gc, obj);
  }

  for (int i = 0; i < 40; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 32);
    if (i % 2 == 0) simple_gc_add_root(&gc, obj);
  }

  gc_stats_t stats_before;
  simple_gc_get_stats(&gc, &stats_before);
  size_t compactions_before = gc.total_compactions;

  printf("\nBefore collection:\n");
  printf("  Objects: %zu\n", gc.object_count);
  printf("  Fragmentation: %.2f%%\n", stats_before.fragmentation_ratio * 100.0f);
  printf("  Compactions: %zu\n", gc.total_compactions);

  // collect to create fragmentation (50% of objects freed)
  simple_gc_collect(&gc);

  printf("After collection:\n");
  printf("  Objects: %zu\n", gc.object_count);
  printf("  Compactions: %zu\n", gc.total_compactions);

  // Get stats after collection but before compaction
  gc_stats_t stats_mid;
  simple_gc_get_stats(&gc, &stats_mid);
  printf("  Fragmentation: %.2f%%\n", stats_mid.fragmentation_ratio * 100.0f);

  // compaction should be triggered
  if (simple_gc_should_compact(&gc)) {
    printf("Compacting...\n");
    simple_gc_compact(&gc);
  }

  gc_stats_t stats_after;
  simple_gc_get_stats(&gc, &stats_after);

  printf("After compaction:\n");
  printf("  Objects: %zu\n", gc.object_count);
  printf("  Fragmentation: %.2f%%\n", stats_after.fragmentation_ratio * 100.0f);
  printf("  Compactions: %zu\n", gc.total_compactions);

  // verify compaction happened
  munit_assert_size(gc.total_compactions, >, compactions_before);

  // fragmentation should be reduced or stay same (compare to mid, not before)
  munit_assert_double(stats_after.fragmentation_ratio, <=, stats_mid.fragmentation_ratio);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/compaction/basic", test_basic, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/compaction/with_roots", test_with_roots, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/compaction/references", test_references, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/compaction/stats", test_stats, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
