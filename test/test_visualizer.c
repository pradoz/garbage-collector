#include "munit.h"
#include "simple_gc.h"
#include "gc_visualizer.h"
#include <string.h>

// // helpers to capture output since visualizer writes to stdout
// typedef struct {
//   char buffer[4096];
//   size_t pos;
// } string_buf_t;
//
// static size_t sbuf_write(void *ptr, size_t size, size_t nmemb, void *stream) {
//   string_buf_t *buf = (string_buf_t *)stream;
//   size_t to_write = size * nmemb;
//
//   if (buf->pos + to_write >= sizeof(buf->buffer)) {
//     to_write = sizeof(buf->buffer) - buf->pos - 1;
//   }
//
//   memcpy(buf->buffer + buf->pos, ptr, to_write);
//   buf->pos += to_write;
//   buf->buffer[buf->pos] = '\0';
//
//   return nmemb;
// }


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

static MunitResult test_viz_reference_graph(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;

  // null things
  gc_viz_reference_graph(NULL, &config);
  gc_viz_reference_graph(&gc, NULL);

  // empty graph
  gc_viz_reference_graph(&gc, &config);

  // with objects
  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
  void *obj2 = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
  void *obj3 = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);

  simple_gc_add_reference(&gc, obj1, obj2);
  simple_gc_add_reference(&gc, obj1, obj3);
  simple_gc_add_reference(&gc, obj2, obj3);

  gc_viz_reference_graph(&gc, &config);

  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_stats_dashboard(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;

  // null things
  gc_viz_stats_dashboard(NULL, &config);
  gc_viz_stats_dashboard(&gc, NULL);

  // empty GC
  gc_viz_stats_dashboard(&gc, &config);

  // with objects
  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *obj2 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 100);
  simple_gc_add_root(&gc, obj1);
  simple_gc_add_reference(&gc, obj1, obj2);
  gc_viz_stats_dashboard(&gc, &config);

  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_full_state(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;

  // null things
  gc_viz_full_state(NULL, &config);
  gc_viz_full_state(&gc, NULL);

  // empty GC
  gc_viz_full_state(&gc, &config);

  // with objects
  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *obj2 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 50);
  simple_gc_add_root(&gc, obj1);
  simple_gc_add_reference(&gc, obj1, obj2);
  gc_viz_full_state(&gc, &config);

  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_snapshot(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // null things
  gc_snapshot_t *snap_null = gc_viz_snapshot(NULL);
  munit_assert_null(snap_null);
  gc_viz_free_snapshot(NULL);

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // empty
  gc_snapshot_t *snap1 = gc_viz_snapshot(&gc);
  munit_assert_not_null(snap1);
  munit_assert_size(snap1->object_count, ==, 0);
  munit_assert_size(snap1->heap_used, ==, 0);

  // with objects
  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *obj2 = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 100);

  gc_snapshot_t *snap2 = gc_viz_snapshot(&gc);
  munit_assert_not_null(snap2);
  munit_assert_size(snap2->object_count, ==, 2);
  munit_assert_true(snap2->heap_used > 0);
  munit_assert_not_null(snap2->object_ptrs);
  munit_assert_not_null(snap2->marked_states);

  // verify object ptrs
  bool found1 = false, found2 = false;
  for (size_t i = 0; i < snap2->object_count; i++) {
    if (snap2->object_ptrs[i] == obj1) found1 = true;
    if (snap2->object_ptrs[i] == obj2) found2 = true;
  }
  munit_assert_true(found1);
  munit_assert_true(found2);

  // free
  gc_viz_free_snapshot(snap1);
  gc_viz_free_snapshot(snap2);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_snapshot_marked_states(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  void *obj2 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  // mark obj1
  simple_gc_mark(&gc, obj1);

  gc_snapshot_t *snap = gc_viz_snapshot(&gc);
  munit_assert_not_null(snap);

  // check marked states
  for (size_t i = 0; i < snap->object_count; i++) {
    if (snap->object_ptrs[i] == obj1) {
      munit_assert_true(snap->marked_states[i]);
    } else if (snap->object_ptrs[i] == obj2) {
      munit_assert_false(snap->marked_states[i]);
    }
  }

  // free
  gc_viz_free_snapshot(snap);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_viz_diff(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_viz_config_t config = gc_viz_default_config();
  config.use_colors = false;

  void *obj1 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 50);
  simple_gc_add_root(&gc, obj1);

  gc_snapshot_t *before = gc_viz_snapshot(&gc);

  // collect; object should be removed
  simple_gc_collect(&gc);

  gc_snapshot_t *after = gc_viz_snapshot(&gc);

  // snapshots should be different
  munit_assert_size(before->object_count, ==, 2);
  munit_assert_size(after->object_count, ==, 1);

  gc_viz_diff(before, after, &config);

  // null things
  gc_viz_diff(NULL, after, &config);
  gc_viz_diff(before, NULL, &config);
  gc_viz_diff(before, after, NULL);

  // free
  gc_viz_free_snapshot(before);
  gc_viz_free_snapshot(after);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/viz_default_config", test_viz_default_config, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_type_string", test_viz_type_string, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_heap_bar", test_viz_heap_bar, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_object_list", test_viz_object_list, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_reference_graph", test_viz_reference_graph, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_stats_dashboard", test_viz_stats_dashboard, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_full_state", test_viz_full_state, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_snapshot", test_viz_snapshot, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_snapshot_marked", test_viz_snapshot_marked_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/viz_diff", test_viz_diff, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
