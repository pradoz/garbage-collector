#include "munit.h"
#include "simple_gc.h"
#include <string.h>

static MunitResult test_version(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    const char* version = simple_gc_version();
    munit_assert_not_null(version);
    munit_assert_string_equal(version, "0.1.0");

    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/version", test_version, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {
    "/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
