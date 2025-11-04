#include "munit.h"
#include "simple_gc.h"
#include <stdio.h>


static MunitResult test_basic(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  const int NUM_OBJS = 100;
  void *objs[NUM_OBJS];

  // allocate NUM_OBJS objects
  for (int i = 0; i < NUM_OBJS; ++i) {
    objs[i] = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_add_root(&gc, objs[i]);
  }

  size_class_t *sc = gc_get_size_class(&gc, sizeof(int));
  float util_before = (float) sc->total_used / (float) sc->total_capacity;

  // remove every odd-indexed root (fragment) and collect
  for (int i = 1; i < NUM_OBJS; i += 2) {
    simple_gc_remove_root(&gc, objs[i]);
  }
  // auto-compaction should happen
  simple_gc_collect(&gc);
  // TODO: should it be exactly half?
  // munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJS / 2);
  munit_assert_size(simple_gc_object_count(&gc), <=, NUM_OBJS / 2);

  // heuristic evaluation (basically, just make sure it doesn't crash)
  float util_after = (float) sc->total_used / (float) sc->total_capacity;
  printf("\n\t[test_basic] Utilization before/after compaction: %.2f -> %.2f\n", util_before, util_after);

  // free
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_preserves_data(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  const int NUM_OBJS = 50;
  int *objs[NUM_OBJS];

  for (int i = 0; i < NUM_OBJS; i++) {
    objs[i] = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *objs[i] = i * 100;
    simple_gc_add_root(&gc, objs[i]);
  }

  // TODO: use this to visualize fragmented compaction
  // printf("\n\t[test_preserves_data] Array before compaction:\n");
  // for (int i = 0; i < NUM_OBJS; i++) {
  //   printf("%d, ", *(int*)gc.roots[i]);
  // }
  // printf("\n");

  // // remove every odd-indexed root (fragment) and collect
  // for (int i = 1; i < NUM_OBJS; i += 2) {
  //   simple_gc_remove_root(&gc, objs[i]);
  // }

  // force compaction
  simple_gc_compact(&gc);

  // verify all data intact
  for (int i = 0; i < NUM_OBJS; i++) {
    // read from roots (may have updated)
    int *obj = (int*) gc.roots[i];
    munit_assert_int(*obj, ==, i * 100);
  }

  // printf("\n\t[test_preserves_data] Array after compaction:\n");
  // for (int i = 0; i < NUM_OBJS; i++) {
  //   printf("%d, ", *(int*)gc.roots[i]);
  // }
  // printf("\n");

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_reference_chains(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  int *a = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *b = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *c = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *a = 111;
  *b = 222;
  *c = 333;

  // reference chain: a -> b -> c
  simple_gc_add_root(&gc, a);
  simple_gc_add_reference(&gc, a, b);
  simple_gc_add_reference(&gc, b, c);

  // allocate filler objects
  for (int i = 0; i < 50; i++) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  // collect and remove filler, then force compaction
  simple_gc_collect(&gc);
  simple_gc_compact(&gc);

  // verify reference chain intact
  munit_assert_size(simple_gc_object_count(&gc), ==, 3);
  munit_assert_int(*a, ==, 111);
  munit_assert_int(*b, ==, 222);
  munit_assert_int(*c, ==, 333);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/compaction/basic", test_basic, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/compaction/preserves_data", test_preserves_data, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/compaction/reference_chains", test_reference_chains, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
