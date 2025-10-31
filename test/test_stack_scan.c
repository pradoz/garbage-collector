#include "munit.h"
#include "simple_gc.h"
#include "gc_platform.h"
#include <time.h>
#include <stdio.h>


#define NO_INLINE __attribute__((noinline))
static int* NO_INLINE allocate_in_register(gc_t *gc) {
  register int *obj = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  if (obj) {
    *obj = 42;
  }
  return obj;
}


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
  gc.use_pools = false;

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

static MunitResult test_stack_platform_detection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  void *stack_bottom = gc_platform_get_stack_bottom();
  void *stack_ptr = gc_platform_get_stack_pointer();

  munit_assert_not_null(stack_bottom);
  munit_assert_not_null(stack_ptr);

  // stack grows downward
  munit_assert_true(stack_bottom > stack_ptr);

  return MUNIT_OK;
}

static MunitResult test_auto_init_stack(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  bool result = simple_gc_auto_init_stack(&gc);
  munit_assert_true(result);
  munit_assert_not_null(gc.stack_bottom);
  munit_assert_true(gc.auto_root_scan_enabled);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_fully_automatic_collection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  simple_gc_auto_init_stack(&gc);

  // allocate objects (no manual root management)
  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *obj2 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *obj1 = 100;
  *obj2 = 200;
  munit_assert_size(simple_gc_object_count(&gc), ==, 2);

  // run garbage collection; both pointers should survive
  simple_gc_collect(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 2);
  munit_assert_int(*obj1, ==, 100);
  munit_assert_int(*obj2, ==, 200);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_register_scanning(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  simple_gc_auto_init_stack(&gc);

  // store a pointer in register
  register int *obj = allocate_in_register(&gc);
  // allocate_in_register(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 1);

  // TODO: fix bug :(
  // run garbage collection
  simple_gc_collect(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 1);

  // object is still reachable
  munit_assert_int(*obj, ==, 42);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_stack_scan_performance(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1 * 1024 * 1024); // 1 MiB heap
  simple_gc_auto_init_stack(&gc);

  // allocate a lot of heckin' objects
  #define NUM_OBJECTS 10000
  int *objects[NUM_OBJECTS];

  for (int i = 0; i < NUM_OBJECTS; i++) {
    objects[i] = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *objects[i] = i;
  }

  munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJECTS);

  // benchmark GC
  clock_t start = clock();
  simple_gc_collect(&gc);
  clock_t end = clock();

  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

  // all objects should survive
  munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJECTS);

  // and all objects should remain the same
  for (int i = 0; i < NUM_OBJECTS; i++) {
    munit_assert_int(*objects[i], ==, i);
  }

  printf("\nCollection time for %d objects: %.6f seconds\n", NUM_OBJECTS, time_spent);

  simple_gc_destroy(&gc);
  return MUNIT_OK;

  #undef NUM_OBJECTS
}

// static MunitResult test_deep_stack_recursion(const MunitParameter params[], void *data) {
//   (void)params;
//   (void)data;
//
//   // TODO
//   // This test is not included in main suite by default
//   // It tests stack scanning with deep recursion
//
//   return MUNIT_SKIP;
// }

static MunitTest tests[] = {
    {"/gc_set_stack_bottom", test_gc_set_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_get_stack_bottom", test_gc_get_stack_bottom, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_auto_roots_toggle", test_gc_auto_roots_toggle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_heap_pointer_detection", test_heap_pointer_detection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_scan_basic", test_stack_scan_basic, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_scan_no_false_negatives", test_stack_scan_no_false_negatives, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_platform_detection", test_stack_platform_detection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_auto_init", test_auto_init_stack, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_auto_collect", test_fully_automatic_collection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_register_scanning", test_register_scanning, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_stack_scan_perf", test_stack_scan_performance, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
