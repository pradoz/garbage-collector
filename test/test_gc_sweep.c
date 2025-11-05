#include "munit.h"
#include "gc_sweep.h"
#include "gc_mark.h"
#include "simple_gc.h"


static MunitResult test_sweep_unmarked(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // allocate 3 objects
  int *obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *obj1 = 1;
  *obj2 = 2;
  *obj3 = 3;

  munit_assert_size(gc.object_count, ==, 3);

  // mark obj1 and obj3
  gc_mark_object(&gc, obj1);
  gc_mark_object(&gc, obj3);

  // sweep
  gc_sweep_all(&gc);

  // should have 2 objects left
  munit_assert_size(gc.object_count, ==, 2);

  // obj1 and obj3 should be unmarked now (for next cycle)
  munit_assert_false(gc_is_marked(&gc, obj1));
  munit_assert_false(gc_is_marked(&gc, obj3));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_pools(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  // allocate many small objects
  for (int i = 0; i < 50; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (i % 2 == 0) {
      gc_mark_object(&gc, obj);
    }
  }

  munit_assert_size(gc.object_count, ==, 50);

  // sweep pools
  gc_sweep_pools(&gc);

  // should have 25 objects left (marked ones)
  munit_assert_size(gc.object_count, ==, 25);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_large(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 8192);

  // allocate large objects
  void *large1 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  // void *large2 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  void *large3 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);

  munit_assert_size(gc.large_block_count, ==, 3);

  // mark large1 and large3
  gc_mark_object(&gc, large1);
  gc_mark_object(&gc, large3);

  // sweep large blocks
  gc_sweep_large_blocks(&gc);

  // should have 2 in use, 1 free (but block kept for reuse)
  munit_assert_size(gc.object_count, ==, 2);
  munit_assert_size(gc.large_block_count, ==, 3);  // blocks kept

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_huge(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 10 * 1024 * 1024);

  // allocate huge objects
  void *huge1 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);
  // void *huge2 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);
  // void *huge3 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);

  munit_assert_size(gc.huge_object_count, ==, 3);

  // mark only huge1
  gc_mark_object(&gc, huge1);

  // sweep huge objects
  gc_sweep_huge_objects(&gc);

  // should have 1 object, 1 huge object left
  munit_assert_size(gc.object_count, ==, 1);
  munit_assert_size(gc.huge_object_count, ==, 1);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_mixed(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 10 * 1024 * 1024);

  // allocate mix: pools, large, huge
  void *small = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 16);
  // void *large = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  void *huge = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 8192);

  munit_assert_size(gc.object_count, ==, 3);

  // mark only small and huge
  gc_mark_object(&gc, small);
  gc_mark_object(&gc, huge);

  // sweep all
  gc_sweep_all(&gc);

  // should have 2 objects left
  munit_assert_size(gc.object_count, ==, 2);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_legacy_mode(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc.use_pools = false;  // disable pools to test legacy mode

  int *obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *obj1 = 1;
  *obj2 = 2;
  *obj3 = 3;

  munit_assert_size(gc.object_count, ==, 3);

  // mark obj1 and obj3
  gc_mark_object(&gc, obj1);
  gc_mark_object(&gc, obj3);

  // sweep legacy
  gc_sweep_legacy(&gc);

  // should have 2 objects left
  munit_assert_size(gc.object_count, ==, 2);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/unmarked", test_sweep_unmarked, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pools", test_sweep_pools, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large", test_sweep_large, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/huge", test_sweep_huge, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mixed", test_sweep_mixed, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/legacy_mode", test_sweep_legacy_mode, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/sweep", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
