#include "munit.h"
#include "simple_gc.h"
#include "gc_visualizer.h"
#include <string.h>


static MunitResult test_viz_default_config(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_viz_config_t config = gc_viz_default_config();
  munit_assert_true(config.show_addresses);
  munit_assert_true(config.use_colors);
  munit_assert_int(config.graph_width, ==, 50);
  munit_assert_not_null(config.output);

  return MUNIT_OK;
}

static MunitResult test_viz_type_string(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  munit_assert_string_equal(gc_viz_type_string(OBJ_TYPE_PRIMITIVE), "PRIM");
  munit_assert_string_equal(gc_viz_type_string(OBJ_TYPE_ARRAY), "ARRAY");
  munit_assert_string_equal(gc_viz_type_string(OBJ_TYPE_STRUCT), "STRUCT");
  munit_assert_string_equal(gc_viz_type_string(OBJ_TYPE_UNKNOWN), "UNKNOWN");

  return MUNIT_OK;
}

static MunitResult test_viz_heap_bar(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;
  config.graph_width = 10;

  // null things
  gc_viz_heap_bar(NULL, &config);
  gc_viz_heap_bar(&gc, NULL);

  // empty GC
  gc_viz_heap_bar(&gc, &config);

  // allocate objects
  simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 100);

  // visual inspection - just make sure it doesn't crash
  gc_viz_heap_bar(&gc, &config);

  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_object_list(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;

  // null things
  gc_viz_object_list(NULL, &config);
  gc_viz_object_list(&gc, NULL);

  // empty GC
  gc_viz_object_list(&gc, &config);

  // with objects
  int *obj1 = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  (int *)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 50);
  simple_gc_add_root(&gc, obj1);
  gc_viz_object_list(&gc, &config);

  simple_gc_destroy(&gc);

  return MUNIT_OK;
}



static MunitTest tests[] = {
    {"/viz_default_config", test_viz_default_config, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_type_string", test_viz_type_string, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_heap_bar", test_viz_heap_bar, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_object_list", test_viz_object_list, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
