#include "munit.h"
#include "gc_visualizer.h"

static MunitResult test_default_config(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_viz_config_t config = gc_viz_default_config();
  munit_assert_true(config.show_addresses);
  munit_assert_true(config.use_colors);
  munit_assert_int(config.graph_width, ==, 50);
  munit_assert_not_null(config.output);

  return MUNIT_OK;
}


static MunitTest tests[] = {
    {"/default_config", test_default_config, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
