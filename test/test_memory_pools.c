#include "munit.h"
#include "simple_gc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


extern const size_t GC_SIZE_CLASS_SIZES[];


static MunitResult test_size_class_selection(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // exact match
  munit_assert_int(gc_size_to_class(8), ==, 0);
  munit_assert_int(gc_size_to_class(16), ==, 1);
  munit_assert_int(gc_size_to_class(32), ==, 2);
  munit_assert_int(gc_size_to_class(64), ==, 3);
  munit_assert_int(gc_size_to_class(128), ==, 4);
  munit_assert_int(gc_size_to_class(256), ==, 5);

  // rounding up
  munit_assert_int(gc_size_to_class(7), ==, 0);
  munit_assert_int(gc_size_to_class(11), ==, 1);
  munit_assert_int(gc_size_to_class(22), ==, 2);
  munit_assert_int(gc_size_to_class(63), ==, 3);
  munit_assert_int(gc_size_to_class(127), ==, 4);
  munit_assert_int(gc_size_to_class(129), ==, 5);

  // boundary
  munit_assert_int(gc_size_to_class(256), ==, 5);
  munit_assert_int(gc_size_to_class(257), ==, -1); // too large
  munit_assert_int(gc_size_to_class(0), ==, 0);

  return MUNIT_OK;
}


static MunitResult test_pool_initialization(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  bool result = simple_gc_init(&gc, 4096);

  munit_assert_true(result);
  munit_assert_true(gc.use_pools);

  // verify size classes are initialized
  for (int i = 0; i < GC_NUM_SIZE_CLASSES; i++) {
    munit_assert_size(gc.size_classes[i].size, ==, GC_SIZE_CLASS_SIZES[i]);
    munit_assert_size(gc.size_classes[i].total_used, ==, 0);
    munit_assert_null(gc.size_classes[i].blocks);
  }

  // free
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_cleanup(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);
  simple_gc_destroy(&gc); // should not crash

  return MUNIT_OK;
}

static MunitResult test_pool_block_creation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  size_t slot_size = sizeof(obj_header_t) + 16;
  size_t capacity = 10;

  pool_block_t *block = gc_create_pool_block(slot_size, capacity);

  munit_assert_not_null(block);
  munit_assert_not_null(block->memory);
  munit_assert_not_null(block->free_list);
  munit_assert_size(block->capacity, ==, capacity);
  munit_assert_size(block->used, ==, 0);

  // count free list entries
  size_t count = 0;
  free_node_t *node = block->free_list;
  while (node && count < capacity + 1) {  // +1 to detect loops
    count++;
    node = node->next;
  }
  munit_assert_size(count, ==, capacity);

  // free
  gc_free_pool_block(block);
  return MUNIT_OK;
}

static MunitResult test_pool_allocation_basic(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  // allocate small object - should use memory pools
  int *obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  munit_assert_not_null(obj1);
  *obj1 = 42;

  // allocate small object - should use memory pools
  int *obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  munit_assert_not_null(obj2);
  *obj2 = 100;

  munit_assert_size(simple_gc_object_count(&gc), ==, 2);
  munit_assert_int(*obj1, ==, 42);
  munit_assert_int(*obj2, ==, 100);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_size_classes(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 8192);

  // allocate objects of different sizes
  void *obj8 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 8);
  void *obj16 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 16);
  void *obj32 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 32);
  void *obj64 = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 64);

  munit_assert_not_null(obj8);
  munit_assert_not_null(obj16);
  munit_assert_not_null(obj32);
  munit_assert_not_null(obj64);

  munit_assert_size(simple_gc_object_count(&gc), ==, 4);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_large_object_allocation(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 8192);

  // allocate large object (> 256 bytes)
  char *large = (char*)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  munit_assert_not_null(large);

  // fill with recognizable pattern
  for (int i = 0; i < 512; i++) {
    large[i] = (char)i;
  }

  // verify recognizable pattern
  for (int i = 0; i < 512; i++) {
    munit_assert_int(large[i], ==, (char)i);
  }

  munit_assert_size(gc.large_object_count, ==, 1);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_expansion(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);  // 1MB

  // allocate many objects to trigger pool expansion
  #define NUM_OBJS 1000
  int *objs[NUM_OBJS];

  for (int i = 0; i < NUM_OBJS; i++) {
    objs[i] = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    munit_assert_not_null(objs[i]);
    *objs[i] = i;
  }

  // verify objects values
  for (int i = 0; i < NUM_OBJS; i++) {
    munit_assert_int(*objs[i], ==, i);
  }

  munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJS);

  // check that pool expanded and has multiple blocks
  size_class_t *sc = gc_get_size_class(&gc, sizeof(int));
  munit_assert_not_null(sc);

  int block_count = 0;
  pool_block_t *block = sc->blocks;
  while (block) {
    block_count++;
    block = block->next;
  }

  munit_assert_int(block_count, >, 1);

  simple_gc_destroy(&gc);
  return MUNIT_OK;

  #undef NUM_OBJS
}

static MunitResult test_pool_mark_and_sweep(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  int* obj1 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int* obj2 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int* obj3 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *obj1 = 111;
  *obj2 = 222;
  *obj3 = 333;
  munit_assert_size(simple_gc_object_count(&gc), ==, 3);

  // set obj1 and obj3 as roots
  simple_gc_add_root(&gc, obj1);
  simple_gc_add_root(&gc, obj3);

  // run garbage collection, obj2 should be freed
  simple_gc_collect(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 2);
  munit_assert_int(*obj1, ==, 111);
  munit_assert_int(*obj3, ==, 333);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_reference_chains(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  int* root = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int* obj1 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int* obj2 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  *root = 1;
  *obj1 = 2;
  *obj2 = 3;

  // reference chain: root -> obj1 -> obj2
  // set obj1 and obj3 as roots
  simple_gc_add_reference(&gc, root, obj1);
  simple_gc_add_reference(&gc, obj1, obj2);
  simple_gc_add_root(&gc, root);
  munit_assert_size(simple_gc_object_count(&gc), ==, 3);

  // run garbage collection, all objects should survive
  simple_gc_collect(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 3);
  munit_assert_int(*root, ==, 1);
  munit_assert_int(*obj1, ==, 2);
  munit_assert_int(*obj2, ==, 3);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_pool_reuse(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  // allocate and collect repeatedly
  const int CYCLE_COUNT = 100;
  for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
    int *obj = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *obj = cycle;
    munit_assert_size(simple_gc_object_count(&gc), ==, 1);
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);
  }

  // check for reused memory
  size_class_t *sc = gc_get_size_class(&gc, sizeof(int));
  munit_assert_not_null(sc);
  munit_assert_size(sc->total_allocated, ==, CYCLE_COUNT);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_mixed_and_large_objects(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 8192);

  int *small1 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  char *large1 = (char*) simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
  int *small2 = (int*) simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  char *large2 = (char*) simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);

  *small1 = 1;
  *small2 = 2;
  strncpy(large1, "foo", 512);
  strncpy(large2, "bar", 512);
  munit_assert_size(simple_gc_object_count(&gc), ==, 4);
  munit_assert_size(gc.large_object_count, ==, 2);

  // add small objects as roots
  simple_gc_add_root(&gc, small1);
  simple_gc_add_root(&gc, small2);

  // run garbage collection - large objects should be freed
  simple_gc_collect(&gc);
  munit_assert_size(simple_gc_object_count(&gc), ==, 2);
  munit_assert_size(gc.large_object_count, ==, 0);

  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitResult test_sweep_small_objects(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);  // 1MiB heap

  #define NUM_OBJS 100
  int *objects[NUM_OBJS];

  // allocate NUM_OBJS small objects
  for (int i = 0; i < NUM_OBJS; i++) {
    objects[i] = (int *)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    *objects[i] = i;
  }

  munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJS);

  // root every 10th object
  for (int i = 0; i < NUM_OBJS; i += 10) {
    simple_gc_add_root(&gc, objects[i]);
  }
  munit_assert_size(gc.root_count, ==, 10);

  // run garbage collectiong
  simple_gc_collect(&gc);

  // should have 10 objects remaining
  munit_assert_size(simple_gc_object_count(&gc), ==, 10);
  munit_assert_size(gc.large_object_count, ==, 0);

  // verify objects survived
  for (int i = 0; i < NUM_OBJS; i += 10) {
    munit_assert_int(*objects[i], ==, i);
  }

  simple_gc_destroy(&gc);
  #undef NUM_OBJS
  return MUNIT_OK;
}

static MunitResult test_sweep_large_objects(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  #define NUM_OBJS 50
  char *objects[NUM_OBJS];

  // allocate NUM_OBJS large objects
  for (int i = 0; i < NUM_OBJS; i++) {
    objects[i] = (char *)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 512);
    snprintf(objects[i], 512, "Object %d", i);
  }

  munit_assert_size(simple_gc_object_count(&gc), ==, NUM_OBJS);
  munit_assert_size(gc.large_object_count, ==, NUM_OBJS);

  // root every 10th object
  for (int i = 0; i < NUM_OBJS; i += 5) {
    simple_gc_add_root(&gc, objects[i]);
  }

  munit_assert_size(gc.root_count, ==, 10);

  // run garbage collection
  simple_gc_collect(&gc);

  // should have 10 objects remaining
  munit_assert_size(simple_gc_object_count(&gc), ==, 10);
  munit_assert_size(gc.large_object_count, ==, 10);

  // verify objects survived
  for (int i = 0; i < NUM_OBJS; i += 5) {
    char expected[512];
    snprintf(expected, 512, "Object %d", i);
    munit_assert_string_equal(objects[i], expected);
  }

  simple_gc_destroy(&gc);
  #undef NUM_OBJS
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/size_class_selection", test_size_class_selection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/initialization", test_pool_initialization, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/cleanup", test_pool_cleanup, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/block_creation", test_pool_block_creation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/allocation_basic", test_pool_allocation_basic, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/size_classes", test_pool_size_classes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/large_object_allocation", test_large_object_allocation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/expansion", test_pool_expansion, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark_and_sweep", test_pool_mark_and_sweep, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/reference_chains", test_pool_reference_chains, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/pool_reuse", test_pool_reuse, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mixed_and_large_objects", test_mixed_and_large_objects, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/sweep_small_objects", test_sweep_small_objects, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/sweep_large_objects", test_sweep_large_objects, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  // {"/performance", test_pools_performance, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}

