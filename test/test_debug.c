#include "munit.h"
#include "simple_gc.h"
#include "gc_trace.h"
#include "gc_debug.h"
#include <stdio.h>


static MunitResult test_enable(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // disabled by default
  munit_assert_false(gc_debug_is_enabled(&gc));

  // enable
  bool result = gc_debug_enable(&gc);
  munit_assert_true(result);
  munit_assert_true(gc_debug_is_enabled(&gc));
  munit_assert_null(gc.debug->allocations);
  munit_assert_int(gc.debug->alloc_count, ==, 0);
  munit_assert_int(gc.debug->next_alloc_id, ==, 1);
  munit_assert_false(gc.debug->track_stacks);
  munit_assert_true(gc.debug->check_double_free);
  munit_assert_true(gc.debug->check_use_after_free);

  // disable
  gc_debug_disable(&gc);
  munit_assert_false(gc_debug_is_enabled(&gc));

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_track_allocations(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_debug_enable(&gc);

  // allocate with debug info
  int *obj1 = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  munit_assert_not_null(obj1);
  munit_assert_not_null(obj2);

  // find allocation info
  alloc_info_t *info1 = gc_debug_find_alloc(&gc, obj1);
  munit_assert_not_null(info1);
  munit_assert_ptr_equal(info1->address, obj1);
  munit_assert_size(info1->size, ==, sizeof(int));
  munit_assert_false(info1->freed);

  gc_debug_dump_allocations(&gc, stdout);

  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_leak_detection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc.use_pools = false;
  gc_debug_enable(&gc);

  // allocate without adding as root (will leak)
  for (int i = 0; i < 5; i++) {
    GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  // before collection, all 5 are leaked (not freed)
  gc_leak_report_t *report = gc_debug_find_leaks(&gc);
  munit_assert_not_null(report);
  munit_assert_size(report->leaked_objects, ==, 5);
  munit_assert_size(report->leaked_bytes, ==, 5 * sizeof(int));

  // run collection, should free all 5 (no roots)
  simple_gc_collect(&gc);

  // after collection, no leaks
  report = gc_debug_find_leaks(&gc);
  munit_assert_not_null(report);
  munit_assert_size(report->leaked_objects, ==, 0);
  munit_assert_size(report->leaked_bytes, ==, 0);

  gc_debug_print_leaks(&gc, stdout);

  // free
  gc_debug_free_leak_report(report);
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_no_leaks(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_debug_enable(&gc);

  // allocate with roots
  for (int i = 0; i < 5; i++) {
    void *obj = GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_add_root(&gc, obj);
  }

  // collect
  simple_gc_collect(&gc);

  // before destruction, roots are still allocated (not freed)
  gc_leak_report_t *report = gc_debug_find_leaks(&gc);
  munit_assert_not_null(report);
  munit_assert_size(report->leaked_objects, ==, 5);

  // free
  gc_debug_free_leak_report(report);
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_validate_heap(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_debug_enable(&gc);

  for (int i = 0; i < 10; i++) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (i % 2 == 0) {
      simple_gc_add_root(&gc, obj);
    }
  }

  bool valid = gc_debug_validate_heap(&gc);
  munit_assert_true(valid);

  // free
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_double_free_detection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc.use_pools = false;
  gc_debug_enable(&gc);

  int *obj = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  munit_assert_not_null(obj);

  // manually track free
  gc_debug_track_free(&gc, obj);

  // try to track free again, should detect double free
  gc_debug_track_free(&gc, obj);

  alloc_info_t *info = gc_debug_find_alloc(&gc, obj);
  munit_assert_not_null(info);
  munit_assert_true(info->freed);

  // free
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_allocation_lifetime(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc.use_pools = false;
  gc_debug_enable(&gc);

  int *obj = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  munit_assert_not_null(obj);

  alloc_info_t *info = gc_debug_find_alloc(&gc, obj);
  munit_assert_not_null(info);
  munit_assert_false(info->freed);
  munit_assert_true(info->alloc_time > 0);

  // track free
  gc_debug_track_free(&gc, obj);

  munit_assert_true(info->freed);
  munit_assert_true(info->free_time >= info->alloc_time);

  // free
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_check_pointer_validity(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc.use_pools = false;
  gc_debug_enable(&gc);

  int *obj = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // valid pointer
  munit_assert_true(gc_debug_check_pointer(&gc, obj));

  // invalid pointer
  int stack_var;
  munit_assert_false(gc_debug_check_pointer(&gc, &stack_var));

  // free
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}
static MunitTest tests[] = {
  {"/enable", test_enable, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/track_allocations", test_track_allocations, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/leak_detection", test_leak_detection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/no_leaks", test_no_leaks, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/validate_heap", test_validate_heap, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/double_free_detection", test_double_free_detection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/allocation_lifetime", test_allocation_lifetime, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pointer_validity", test_check_pointer_validity, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/debug", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
