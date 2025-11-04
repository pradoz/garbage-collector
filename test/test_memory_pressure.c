#include "munit.h"
#include "simple_gc.h"
#include "gc_pool.h"


static MunitResult test_check(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 100);

  munit_assert_int(simple_gc_check_pressure(&gc), ==, GC_PRESSURE_NONE);

  gc.heap_used = 52;
  munit_assert_int(simple_gc_check_pressure(&gc), ==, GC_PRESSURE_LOW);

  gc.heap_used = 77;
  munit_assert_int(simple_gc_check_pressure(&gc), ==, GC_PRESSURE_MEDIUM);

  gc.heap_used = 90;
  munit_assert_int(simple_gc_check_pressure(&gc), ==, GC_PRESSURE_HIGH);

  gc.heap_used = 95;
  munit_assert_int(simple_gc_check_pressure(&gc), ==, GC_PRESSURE_CRITICAL);

  // free
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_auto_collection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1000);

  // should not collect when below threshold
  gc.heap_used = 400;  // 40%
  gc.config.collect_threshold = 0.75f;
  gc.allocs_since_collect = 10;

  // manually call the internal function by triggering allocation
  size_t collections_before = gc.total_collections;
  simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 10);
  munit_assert_size(gc.total_collections, ==, collections_before);

  // should collect when above threshold
  gc.heap_used = 800;
  collections_before = gc.total_collections;
  simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 10);
  munit_assert_size(gc.total_collections, >, collections_before);

  // should collect on high pressure regardless of threshold
  gc.heap_used = 900;
  gc.config.collect_threshold = 0.99f;
  collections_before = gc.total_collections;
  simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 10);
  munit_assert_size(gc.total_collections, >, collections_before);

  // free
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_auto_expansion(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  gc_config_t config = gc.config;
  simple_gc_set_config(&gc, &config);

  size_class_t *sc = gc_pool_get_size_class(gc.size_classes, 16);
  size_t initial_capacity = sc->total_capacity;

  // fill memory pools
  for (int i = 0; i < 500; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 16);
    simple_gc_add_root(&gc, obj);
  }

  // should automatically expanded
  munit_assert_size(sc->total_capacity, >, initial_capacity);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_auto_shrinking(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  gc_config_t config = gc.config;
  simple_gc_set_config(&gc, &config);

  // fill pools
  for (int i = 0; i < 200; i++) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 16);
  }

  size_class_t *sc = gc_pool_get_size_class(gc.size_classes, 16);
  size_t capacity_before = sc->total_capacity;

  // run garbage collection, no roots -> all freed
  simple_gc_collect(&gc);

  // should automatically shrink (remove empty blocks)
  size_t capacity_after = sc->total_capacity;
  munit_assert_size(capacity_after, <, capacity_before);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/pressure/check", test_check, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pressure/auto_collection", test_auto_collection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pressure/auto_expansion", test_auto_expansion, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pressure/auto_shrinking", test_auto_shrinking, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
