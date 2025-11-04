#include "munit.h"
#include "gc_large.h"
#include "simple_gc.h"


static MunitResult test_large_create_block(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *block = gc_large_create_block(OBJ_TYPE_ARRAY, 512);
  munit_assert_not_null(block);
  munit_assert_not_null(block->memory);
  munit_assert_not_null(block->header);
  munit_assert_true(block->in_use);
  munit_assert_size(block->size, ==, 512);

  gc_large_free_block(block);

  // too small
  block = gc_large_create_block(OBJ_TYPE_ARRAY, 128);
  munit_assert_null(block);

  // too large
  block = gc_large_create_block(OBJ_TYPE_ARRAY, 5000);
  munit_assert_null(block);

  return MUNIT_OK;
}

static MunitResult test_large_best_fit(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *block1 = gc_large_create_block(OBJ_TYPE_ARRAY, 512);
  large_block_t *block2 = gc_large_create_block(OBJ_TYPE_ARRAY, 1024);
  large_block_t *block3 = gc_large_create_block(OBJ_TYPE_ARRAY, 768);

  // block1 -> block2 -> block3 -> NULL
  block1->next = block2;
  block2->next = block3;
  block3->next = NULL;

  // mark blocks free
  block1->in_use = false;
  block2->in_use = false;
  block3->in_use = false;

  // find best fit for 600 bytes; should pick 768, not 1024
  large_block_t *best = gc_large_find_best_fit(block1, 600);
  munit_assert_not_null(best);
  munit_assert_size(best->size, ==, 768);

  // find best fit for 900 bytes; should pick 1024
  best = gc_large_find_best_fit(block1, 900);
  munit_assert_not_null(best);
  munit_assert_size(best->size, ==, 1024);

  // find best fit for 2000 bytes; should fail
  best = gc_large_find_best_fit(block1, 2000);
  munit_assert_null(best);

  // free blocks
  gc_large_free_block(block1);
  gc_large_free_block(block2);
  gc_large_free_block(block3);

  return MUNIT_OK;
}

static MunitResult test_large_alloc(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *blocks = NULL;
  size_t block_count = 0;

  // allocate large object
  void *obj = gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 512);
  munit_assert_not_null(obj);
  munit_assert_size(block_count, ==, 1);
  munit_assert_not_null(blocks);

  // allocate another large object
  void *obj2 = gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 768);
  munit_assert_not_null(obj2);
  munit_assert_size(block_count, ==, 2);

  // free large objects
  gc_large_destroy_all(blocks);

  return MUNIT_OK;
}

static MunitResult test_large_reuse(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *blocks = NULL;
  size_t block_count = 0;

  // allocate
  void *obj = gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 512);
  munit_assert_not_null(obj);
  munit_assert_size(block_count, ==, 1);

  // mark as free
  blocks->in_use = false;

  // allocate again; should reuse freed block
  void *obj2 = gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 400);
  munit_assert_not_null(obj2);
  munit_assert_size(block_count, ==, 1); // no new block
  munit_assert_true(blocks->in_use);

  gc_large_destroy_all(blocks);

  return MUNIT_OK;
}

static MunitResult test_large_find_header(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *blocks = NULL;
  size_t block_count = 0;

  void *obj = gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 512);
  munit_assert_not_null(obj);

  obj_header_t *header = gc_large_find_header(blocks, obj);
  munit_assert_not_null(header);
  munit_assert_size(header->size, ==, 512);
  munit_assert_int(header->type, ==, OBJ_TYPE_ARRAY);

  // invalid pointer
  int external;
  header = gc_large_find_header(blocks, &external);
  munit_assert_null(header);

  // free
  gc_large_destroy_all(blocks);

  return MUNIT_OK;
}

static MunitResult test_huge_create_object(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // too small
  huge_object_t *huge = gc_huge_create_object(OBJ_TYPE_ARRAY, 1024);
  munit_assert_null(huge);

  // valid huge object
  huge = gc_huge_create_object(OBJ_TYPE_ARRAY, 8192);
  munit_assert_not_null(huge);
  munit_assert_not_null(huge->memory);
  munit_assert_not_null(huge->header);
  munit_assert_size(huge->size, >=, 8192);

  // free
  gc_huge_free_object(huge);

  return MUNIT_OK;
}

static MunitResult test_huge_alloc(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  huge_object_t *objects = NULL;
  size_t object_count = 0;

  // allocate huge object
  void *obj = gc_huge_alloc(&objects, &object_count, OBJ_TYPE_ARRAY, 8192);
  munit_assert_not_null(obj);
  munit_assert_size(object_count, ==, 1);
  munit_assert_not_null(objects);

  // write to memory; should not crash
  char *data_ptr = (char*)obj;
  for (int i = 0; i < 8192; i++) {
    data_ptr[i] = (char)i;
  }

  // verify objects
  for (int i = 0; i < 8192; i++) {
    munit_assert_int(data_ptr[i], ==, (char)i);
  }

  // free
  gc_huge_destroy_all(objects);

  return MUNIT_OK;
}

static MunitResult test_huge_find_header(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  huge_object_t *objects = NULL;
  size_t object_count = 0;

  void *obj = gc_huge_alloc(&objects, &object_count, OBJ_TYPE_ARRAY, 8192);
  munit_assert_not_null(obj);

  obj_header_t *header = gc_huge_find_header(objects, obj);
  munit_assert_not_null(header);
  munit_assert_size(header->size, ==, 8192);
  munit_assert_int(header->type, ==, OBJ_TYPE_ARRAY);

  // free
  gc_huge_destroy_all(objects);

  return MUNIT_OK;
}

static MunitResult test_large_statistics(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  large_block_t *blocks = NULL;
  size_t block_count = 0;

  // allocate large blocks
  gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 512);
  gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 768);
  gc_large_alloc(&blocks, &block_count, OBJ_TYPE_ARRAY, 1024);

  munit_assert_size(gc_large_count_blocks(blocks), ==, 3);
  munit_assert_size(gc_large_count_in_use(blocks), ==, 3);

  size_t total = gc_large_total_memory(blocks);
  munit_assert_size(total, >, 0);

  // mark one block ass free
  blocks->in_use = false;
  munit_assert_size(gc_large_count_in_use(blocks), ==, 2);

  // free
  gc_large_destroy_all(blocks);

  return MUNIT_OK;
}

static MunitResult test_huge_statistics(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  huge_object_t *objects = NULL;
  size_t object_count = 0;

  gc_huge_alloc(&objects, &object_count, OBJ_TYPE_ARRAY, 8192);
  gc_huge_alloc(&objects, &object_count, OBJ_TYPE_ARRAY, 16384);

  munit_assert_size(gc_huge_count_objects(objects), ==, 2);

  size_t total = gc_huge_total_memory(objects);
  munit_assert_size(total, >, 0);

  // free
  gc_huge_destroy_all(objects);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/large/create_block", test_large_create_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large/best_fit", test_large_best_fit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large/alloc", test_large_alloc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large/reuse", test_large_reuse, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large/find_header", test_large_find_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/huge/create_object", test_huge_create_object, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/huge/alloc", test_huge_alloc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/huge/find_header", test_huge_find_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large/statistics", test_large_statistics, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/huge/statistics", test_huge_statistics, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/gc_large", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
