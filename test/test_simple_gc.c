#include "munit.h"
#include "simple_gc.h"

static MunitResult test_version(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  const char *version = simple_gc_version();
  munit_assert_not_null(version);
  munit_assert_string_equal(version, "0.1.0");

  return MUNIT_OK;
}

static MunitResult test_init_header(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // vanilla case
  obj_header_t header;
  bool result = simple_gc_init_header(&header, OBJ_TYPE_PRIMITIVE, 4);
  munit_assert_true(result);
  munit_assert_int(header.type, ==, OBJ_TYPE_PRIMITIVE);
  munit_assert_size(header.size, ==, 4);
  munit_assert_false(header.marked);
  munit_assert_null(header.next);

  // NULL header w/ non-zero size
  result = simple_gc_init_header(NULL, OBJ_TYPE_PRIMITIVE, 32);
  munit_assert_false(result);

  // non-NULL header w/ size == 0
  result = simple_gc_init_header(&header, OBJ_TYPE_PRIMITIVE, 0);
  munit_assert_false(result);

  return MUNIT_OK;
}

static MunitResult test_is_valid_header(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // valid header
  obj_header_t header;
  simple_gc_init_header(&header, OBJ_TYPE_PRIMITIVE, 8);
  munit_assert_true(simple_gc_is_valid_header(&header));

  // NULL header
  munit_assert_false(simple_gc_is_valid_header(NULL));

  // invalid type
  header.type = -1;
  munit_assert_false(simple_gc_is_valid_header(&header));

  // empty header
  header.type = OBJ_TYPE_PRIMITIVE;
  header.size = 0;
  munit_assert_false(simple_gc_is_valid_header(&header));

  return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/version", test_version, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/init_header", test_init_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/is_valid_header", test_is_valid_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
