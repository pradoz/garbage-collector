#include "munit.h"
#include "simple_gc.h"


// externals, forward declarations, helpers
extern const size_t GC_SIZE_CLASS_SIZES[];

static int gc_size_to_class(size_t size);
// static size_class_t* gc_get_size_class(gc_t *gc, size_t size);


static MunitResult test_size_class_selection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // exact match
  munit_assert_int(size_to_class(8), ==, 0);
  munit_assert_int(size_to_class(16), ==, 1);
  munit_assert_int(size_to_class(32), ==, 2);
  munit_assert_int(size_to_class(64), ==, 3);
  munit_assert_int(size_to_class(128), ==, 4);
  munit_assert_int(size_to_class(256), ==, 5);

  // rounding up
  munit_assert_int(size_to_class(7), ==, 0);
  munit_assert_int(size_to_class(11), ==, 1);
  munit_assert_int(size_to_class(22), ==, 2);
  munit_assert_int(size_to_class(63), ==, 3);
  munit_assert_int(size_to_class(127), ==, 4);
  munit_assert_int(size_to_class(129), ==, 5);

  // boundary
  munit_assert_int(size_to_class(256), ==, 5);
  munit_assert_int(size_to_class(257), ==, -1); // too large
  munit_assert_int(size_to_class(0), ==, -1);

  return MUNIT_OK;
}


static MunitResult test_pool_initialization(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  bool result = simple_gc_init(&gc, 1024);

  munit_assert_true(result);
  munit_assert_true(gc.use_pools);

  // verify size classes are initialized
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; i++) {
    munit_assert_size(gc.size_classes[i].size, == GC_SIZE_CLASS_SIZES[i]);
    munit_assert_size(gc.size_classes[i].total_used, == 0);
    munit_assert_null(gc.size_classes[i].blocks);
  }

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_cleanup(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  simple_gc_destroy(&gc); // should not crash

  return MUNIT_OK;
}
static MunitTest tests[] = {
    {"/gc_size_class_selection", test_size_class_selection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_pool_initialization", test_pool_initialization, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_pool_cleanup", test_pool_cleanup, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}

