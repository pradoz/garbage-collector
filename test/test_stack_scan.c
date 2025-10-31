#include "munit.h"
#include "simple_gc.h"

static MunitResult test_gc_set_stack_bottom(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  int stack_var;
  bool result;
  gc_t gc;
  simple_gc_init(&gc, 1024);

  // null things
  result = simple_gc_set_stack_bottom(NULL, &stack_var);
  munit_assert_false(result);
  result = simple_gc_set_stack_bottom(&gc, NULL);
  munit_assert_true(result);

  // set stack bottom
  result = simple_gc_set_stack_bottom(&gc, &stack_var);
  munit_assert_true(result);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_gc_get_stack_bottom(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  int stack_var;
  bool result;
  gc_t gc;
  simple_gc_init(&gc, 1024);

  // null things
  munit_assert_null(simple_gc_get_stack_bottom(NULL));

  // empty GC --> null stack bottom
  munit_assert_null(simple_gc_get_stack_bottom(&gc));

  // set stack bottom
  result = simple_gc_set_stack_bottom(&gc, &stack_var);
  munit_assert_true(result);

  // get stack bottom
  void *stack_bottom = simple_gc_get_stack_bottom(&gc);
  munit_assert_ptr_equal(stack_bottom, &stack_var);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_gc_auto_roots_toggle(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // null things
  munit_assert_false(simple_gc_enable_auto_roots(NULL, true));
  munit_assert_false(simple_gc_enable_auto_roots(NULL, false));

  // default value
  munit_assert_true(simple_gc_enable_auto_roots(&gc, false));
  munit_assert_false(gc.auto_root_scan_enabled);

  // enable
  munit_assert_true(simple_gc_enable_auto_roots(&gc, true));
  munit_assert_true(gc.auto_root_scan_enabled);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/gc_set_stack_bottom", test_gc_set_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_get_stack_bottom", test_gc_get_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_auto_roots_toggle", test_gc_auto_roots_toggle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
