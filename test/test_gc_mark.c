#include "munit.h"
#include "gc_mark.h"
#include "simple_gc.h"


static MunitResult test_mark_single_object(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *obj = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *obj = 42;

  // initially unmarked
  munit_assert_false(gc_is_marked(&gc, obj));

  // mark it
  gc_mark_object(&gc, obj);
  munit_assert_true(gc_is_marked(&gc, obj));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mark_roots(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_add_root(&gc, obj1);
  simple_gc_add_root(&gc, obj3);

  // mark all roots
  gc_mark_all_roots(&gc);

  // obj1 and obj3 should be marked, obj2 should not
  munit_assert_true(gc_is_marked(&gc, obj1));
  munit_assert_false(gc_is_marked(&gc, obj2));
  munit_assert_true(gc_is_marked(&gc, obj3));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mark_reference_chain(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *a = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *b = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *c = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *a = 1;
  *b = 2;
  *c = 3;

  // chain: a -> b -> c
  simple_gc_add_reference(&gc, a, b);
  simple_gc_add_reference(&gc, b, c);
  simple_gc_add_root(&gc, a);

  // mark from roots
  gc_mark_all_roots(&gc);

  // all should be marked
  munit_assert_true(gc_is_marked(&gc, a));
  munit_assert_true(gc_is_marked(&gc, b));
  munit_assert_true(gc_is_marked(&gc, c));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mark_cycle(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *a = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *b = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *c = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // cycle: a -> b -> c -> a
  simple_gc_add_reference(&gc, a, b);
  simple_gc_add_reference(&gc, b, c);
  simple_gc_add_reference(&gc, c, a);
  simple_gc_add_root(&gc, a);

  // mark from roots (should handle cycle)
  gc_mark_all_roots(&gc);

  // all should be marked
  munit_assert_true(gc_is_marked(&gc, a));
  munit_assert_true(gc_is_marked(&gc, b));
  munit_assert_true(gc_is_marked(&gc, c));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mark_iterative(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *a = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *b = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *c = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_add_reference(&gc, a, b);
  simple_gc_add_reference(&gc, b, c);
  simple_gc_add_root(&gc, a);

  // mark iteratively (non-recursive)
  gc_mark_all_roots_iterative(&gc);

  // all should be marked
  munit_assert_true(gc_is_marked(&gc, a));
  munit_assert_true(gc_is_marked(&gc, b));
  munit_assert_true(gc_is_marked(&gc, c));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_unmark_all(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_add_root(&gc, obj1);
  simple_gc_add_root(&gc, obj2);

  // mark all
  gc_mark_all_roots(&gc);
  munit_assert_true(gc_is_marked(&gc, obj1));
  munit_assert_true(gc_is_marked(&gc, obj2));

  // unmark all
  gc_unmark_all(&gc);
  munit_assert_false(gc_is_marked(&gc, obj1));
  munit_assert_false(gc_is_marked(&gc, obj2));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_count_marked(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // allocate 10 objects
  for (int i = 0; i < 10; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (i < 3) {
      simple_gc_add_root(&gc, obj);
    }
  }

  // mark roots
  gc_mark_all_roots(&gc);

  // should have 3 marked, 7 unmarked
  munit_assert_size(gc_count_marked(&gc), ==, 3);
  munit_assert_size(gc_count_unmarked(&gc), ==, 7);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_deep_reference_chain(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 8192);

  // create a deep chain: root -> obj1 -> obj2 -> ... -> obj99
  void *prev = NULL;
  void *root = NULL;

  for (int i = 0; i < 100; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    if (i == 0) {
      root = obj;
      simple_gc_add_root(&gc, root);
    } else {
      simple_gc_add_reference(&gc, prev, obj);
    }

    prev = obj;
  }

  // mark from root (should traverse entire chain)
  gc_mark_all_roots(&gc);

  // all 100 objects should be marked
  munit_assert_size(gc_count_marked(&gc), ==, 100);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mark_diamond_graph(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  /* diamond graph:
   *     root
   *     /  \
   *    b    c
   *     \  /
   *      d
  */

  void *root = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *b = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *c = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *d = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_add_reference(&gc, root, b);
  simple_gc_add_reference(&gc, root, c);
  simple_gc_add_reference(&gc, b, d);
  simple_gc_add_reference(&gc, c, d);
  simple_gc_add_root(&gc, root);

  // mark from root
  gc_mark_all_roots(&gc);

  // all should be marked
  munit_assert_true(gc_is_marked(&gc, root));
  munit_assert_true(gc_is_marked(&gc, b));
  munit_assert_true(gc_is_marked(&gc, c));
  munit_assert_true(gc_is_marked(&gc, d));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/mark/single_object", test_mark_single_object, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/roots", test_mark_roots, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/reference_chain", test_mark_reference_chain, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/cycle", test_mark_cycle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/iterative", test_mark_iterative, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/unmark_all", test_unmark_all, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/count_marked", test_count_marked, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/deep_chain", test_deep_reference_chain, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark/diamond_graph", test_mark_diamond_graph, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/gc_mark", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
