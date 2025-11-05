#include "munit.h"
#include "simple_gc.h"
#include "gc_trace.h"
#include "gc_debug.h"
#include <stdio.h>


static MunitResult test_begin_end(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  // start
  bool result = gc_trace_begin(&gc, "test_trace.txt", GC_TRACE_FORMAT_TEXT);
  munit_assert_true(result);
  munit_assert_not_null(gc.trace);

  // end
  gc_trace_end(&gc);
  munit_assert_null(gc.trace);

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_allocation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);
  gc_trace_begin(&gc, "test_alloc.txt", GC_TRACE_FORMAT_TEXT);

  for (int i = 0; i < 10; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  gc_trace_stats_t stats;
  gc_trace_get_stats(&gc, &stats);

  munit_assert_size(stats.alloc_count, ==, 10);
  munit_assert_size(stats.total_allocated, ==, 10 * sizeof(int));

  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_collection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  gc_trace_begin(&gc, "test_collection.txt", GC_TRACE_FORMAT_TEXT);

  // allocate
  int *root = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, root);
  for (int i = 0; i < 5; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  // collect
  simple_gc_collect(&gc);

  // assert stats
  gc_trace_stats_t stats;
  gc_trace_get_stats(&gc, &stats);
  munit_assert_size(stats.collection_count, ==, 1);
  munit_assert_double(stats.avg_gc_pause_ms, >, 0.0);

  // print to stdout
  gc_trace_print_stats(&gc, stdout);

  // free
  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_format_json(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  gc_trace_begin(&gc, "test_json.json", GC_TRACE_FORMAT_JSON);

  int *root = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, root);

  for (int i = 0; i < 100; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  simple_gc_collect(&gc);

  // free
  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  // file should be created
  FILE *f = fopen("test_json.json", "r");
  munit_assert_not_null(f);
  fclose(f);

  return MUNIT_OK;
}

static MunitResult test_format_chrome(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  gc_trace_begin(&gc, "test_chrome.json", GC_TRACE_FORMAT_CHROME);

  int *root = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, root);

  for (int i = 0; i < 100; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  simple_gc_collect(&gc);

  // free
  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  // file should be created
  FILE *f = fopen("test_chrome.json", "r");
  munit_assert_not_null(f);
  fclose(f);

  printf("\n=== Chrome Trace ===\n");
  printf("View test_chrome.json in chrome://tracing\n");
  printf("====================\n\n");

  return MUNIT_OK;
}

static MunitResult test_all_events(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  gc_trace_begin(&gc, "test_all_events.txt", GC_TRACE_FORMAT_TEXT);

  int *root = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  simple_gc_add_root(&gc, root);

  for (int i = 0; i < 10; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  // collection (will trigger mark/sweep events)
  simple_gc_collect(&gc);

  // root operations
  simple_gc_remove_root(&gc, root);

  // stats
  gc_trace_stats_t stats;
  gc_trace_get_stats(&gc, &stats);

  munit_assert_size(stats.alloc_count, ==, 11);
  munit_assert_size(stats.collection_count, ==, 1);
  munit_assert_size(stats.total_events, >, 11); // allocs + mark/sweep events

  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_buffer_overflow(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  gc_trace_config_t config = gc_trace_default_config();
  config.buffer_size = 10; // Small buffer
  config.output = fopen("test_overflow.txt", "w");

  gc.trace = gc_trace_create(&config);
  munit_assert_not_null(gc.trace);

  // allocate more than buffer size
  for (int i = 0; i < 20; ++i) {
    simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  }

  // should auto-flush
  gc_trace_stats_t stats;
  gc_trace_get_stats(&gc, &stats);
  munit_assert_size(stats.alloc_count, ==, 20);

  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_timing_accuracy(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  gc_trace_begin(&gc, "test_timing.txt", GC_TRACE_FORMAT_TEXT);

  // perform work
  for (int i = 0; i < 100; ++i) {
    void *obj = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    simple_gc_add_root(&gc, obj);
  }

  simple_gc_collect(&gc);

  gc_trace_stats_t stats;
  gc_trace_get_stats(&gc, &stats);

  // should have timing data
  munit_assert_double(stats.total_gc_time_ms, >, 0.0);
  munit_assert_double(stats.avg_gc_pause_ms, >, 0.0);
  munit_assert_double(stats.max_gc_pause_ms, >=, stats.avg_gc_pause_ms);

  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/begin_end", test_begin_end, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/allocation", test_allocation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/collection", test_collection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/format/json", test_format_json, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/format/chrome", test_format_chrome, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/test_all_events", test_all_events, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/test_buffer_overflow", test_buffer_overflow, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/test_timing_accuracy", test_timing_accuracy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/trace", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
