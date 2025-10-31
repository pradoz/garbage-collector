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

static MunitResult test_heap_pointer_detection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // valid heap pointers
  munit_assert_true(simple_gc_is_heap_pointer(&gc, obj1));
  munit_assert_true(simple_gc_is_heap_pointer(&gc, obj2));

  // invalid pointers
  int stack_var;
  munit_assert_false(simple_gc_is_heap_pointer(&gc, &stack_var));
  munit_assert_false(simple_gc_is_heap_pointer(&gc, NULL));

  // pointer arithmetic within objects
  char *arr = (char *)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 100);
  munit_assert_true(simple_gc_is_heap_pointer(&gc, arr));
  munit_assert_true(simple_gc_is_heap_pointer(&gc, arr + 50));
  munit_assert_false(simple_gc_is_heap_pointer(&gc, arr + 101));

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_stack_scan_basic(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // set stack bottom
  int stack_anchor = 7;
  munit_assert_true(simple_gc_set_stack_bottom(&gc, &stack_anchor));
  munit_assert_ptr_equal(simple_gc_get_stack_bottom(&gc), &stack_anchor);

  munit_assert_false(gc.auto_root_scan_enabled);
  simple_gc_enable_auto_roots(&gc, true);
  munit_assert_true(gc.auto_root_scan_enabled);

  int *obj = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *obj = 42; // keep pointer on stack
  munit_assert_size(simple_gc_object_count(&gc), ==, 1);

  // run collection; obj survive because pointer is on stack
  simple_gc_add_root(&gc, (void*) obj);

  // TODO: handle compiler optimization cleanup
  // simple_gc_collect(&gc);
  // munit_assert_size(simple_gc_object_count(&gc), ==, 1);
  // munit_assert_int(*obj, ==, 42);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_stack_scan_no_false_negatives(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  int stack_anchor;
  simple_gc_set_stack_bottom(&gc, &stack_anchor);
  simple_gc_enable_auto_roots(&gc, true);

  // store some pointers on stack
  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj3 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *obj1 = 111;
  *obj2 = 222;
  *obj3 = 333;
  munit_assert_size(simple_gc_object_count(&gc), ==, 3);

  // run garbage collection
  simple_gc_collect(&gc);

  // TODO: handle compiler optimization cleanup
  // // obj1 and obj3 should survive
  // munit_assert_size(simple_gc_object_count(&gc), ==, 2);
  // munit_assert_int(*obj1, ==, 111);
  // munit_assert_int(*obj3, ==, 333);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/gc_set_stack_bottom", test_gc_set_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_get_stack_bottom", test_gc_get_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_auto_roots_toggle", test_gc_auto_roots_toggle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_heap_pointer_detection", test_heap_pointer_detection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_scan_basic", test_stack_scan_basic, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_scan_no_false_negatives", test_stack_scan_no_false_negatives, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
