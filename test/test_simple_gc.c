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

static MunitResult test_gc_new(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // vanilla case
  gc_t *gc = simple_gc_new(1024);
  munit_assert_not_null(gc);
  munit_assert_size(simple_gc_object_count(gc), ==, 0);
  munit_assert_size(simple_gc_heap_capacity(gc), ==, 1024);

  // clean up
  simple_gc_destroy(gc);
  free(gc);
  munit_assert_false(simple_gc_is_valid_header(NULL));

  // create w/ no capacity
  gc = simple_gc_new(0);
  munit_assert_null(gc);

  return MUNIT_OK;
}

static MunitResult test_gc_init(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;

  bool result = simple_gc_init(&gc, 1024);
  munit_assert_true(result);
  munit_assert_size(simple_gc_object_count(&gc), ==, 0);
  munit_assert_size(simple_gc_heap_capacity(&gc), ==, 1024);

  // clean up
  simple_gc_destroy(&gc);

  // null GC
  result = simple_gc_init(NULL, 1024);
  munit_assert_false(result);

  // no capacity
  result = simple_gc_init(&gc, 0);
  munit_assert_false(result);

  return MUNIT_OK;
}

static MunitResult test_gc_object_count(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 1024);

  munit_assert_size(simple_gc_object_count(&gc), ==, 0);
  munit_assert_size(simple_gc_object_count(NULL), ==, 0);
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_gc_heap_capacity(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // null GC
  munit_assert_size(simple_gc_heap_capacity(NULL), ==, 0);

  // allocate w/ capacity
  gc_t gc;
  simple_gc_init(&gc, 1024);
  munit_assert_size(simple_gc_heap_capacity(&gc), ==, 1024);

  // free
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_gc_heap_used(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  // null GC
  munit_assert_size(simple_gc_heap_used(NULL), ==, 0);

  // allocate empty GC
  gc_t gc;
  simple_gc_init(&gc, 1024);
  munit_assert_size(simple_gc_heap_used(&gc), ==, 0);

  // free
  simple_gc_destroy(&gc);

  return MUNIT_OK;
}

static MunitResult test_gc_alloc(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // memo initial usage
    size_t initial_used = simple_gc_heap_used(&gc);
    size_t expected_size = sizeof(obj_header_t) + sizeof(int);

    // allocate an integer
    int* obj = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    munit_assert_not_null(obj);
    munit_assert_size(simple_gc_object_count(&gc), ==, 1);

    // increase heap usage
    munit_assert_size(simple_gc_heap_used(&gc) - initial_used, ==, expected_size);

    // use allocated memory without segfaulting
    *obj = 42;
    munit_assert_int(*obj, ==, 42);

    // verify object header is accessible and initialized
    obj_header_t* header = simple_gc_find_header(&gc, obj);
    munit_assert_not_null(header);
    munit_assert_size(header->size, ==, sizeof(int));
    munit_assert_int(header->type, ==, OBJ_TYPE_PRIMITIVE);

    // tests allocation size scale
    char* big_obj = (char*)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 100);
    munit_assert_not_null(big_obj);

    // write to the whole allocated regionthis
    // NOTE: this will crash if not enough memory was allocated
    for (size_t i = 0; i < 100; i++) {
        big_obj[i] = (char)i;
    }

    // verify memory integrity
    for (size_t i = 0; i < 100; i++) {
        munit_assert_int(big_obj[i], ==, (char)i);
    }

    // allocation should fail with null GC
    void* result = simple_gc_alloc(NULL, OBJ_TYPE_PRIMITIVE, sizeof(int));
    munit_assert_null(result);

    // allocate more no capacity
    result = simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, 0);
    munit_assert_null(result);

    // allocate more than available capacity
    result = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 2000);
    munit_assert_null(result);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_alloc_boundary(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // allocate an array with a known pattern
    size_t arr_size = 32;
    int* arr = (int*)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, arr_size * sizeof(int));
    munit_assert_not_null(arr);

    // fill with a recognizable pattern
    for (size_t i = 0; i < arr_size; i++) {
        arr[i] = 0xDEADBEEF + (int)i;
    }

    // verify all elements match the expected pattern
    for (size_t i = 0; i < arr_size; i++) {
        munit_assert_int(arr[i], ==, 0xDEADBEEF + (int)i);
    }

    // verify the header size is correct
    obj_header_t* header = simple_gc_find_header(&gc, arr);
    munit_assert_not_null(header);
    munit_assert_size(header->size, ==, arr_size * sizeof(int));

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_alloc_stress(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    size_t capacity = 1024 * 1024; // 1MB
    simple_gc_init(&gc, capacity);

    // allocate objects until nearly full
    void* objects[100];
    size_t count = 0;
    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};

    for (size_t i = 0; i < 100; i++) {
        size_t size = sizes[i % (sizeof(sizes)/sizeof(sizes[0]))];
        objects[count] = simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, size);

        if (objects[count] == NULL) {
            break; // heap is full
        }

        // fill allocated memory with a pattern
        memset(objects[count], (int)i, size);
        count++;
    }

    // verify allocated objects are accessible and contain the expected pattern
    for (size_t i = 0; i < count; i++) {
        size_t size = sizes[i % (sizeof(sizes) / sizeof(sizes[0]))];
        unsigned char* ptr = (unsigned char*)objects[i];

        for (size_t j = 0; j < size; j++) {
            munit_assert_int(ptr[j], ==, (unsigned char)i);
        }
    }

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_find_header(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // allocate an integer
    int* obj = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    // find the object header
    obj_header_t* header = simple_gc_find_header(&gc, obj);
    munit_assert_not_null(header);
    munit_assert_int(header->type, ==, OBJ_TYPE_PRIMITIVE);
    munit_assert_size(header->size, ==, sizeof(int));

    // null GC
    header = simple_gc_find_header(NULL, obj);
    munit_assert_null(header);

    // null object ptr
    header = simple_gc_find_header(&gc, NULL);
    munit_assert_null(header);

    // invalid object pointer
    int nope;
    header = simple_gc_find_header(&gc, &nope);
    munit_assert_null(header);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_is_root(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // add objects to root array
    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    bool result = simple_gc_add_root(&gc, obj1);
    munit_assert_true(result);

    // obj1 is the root
    result = simple_gc_is_root(&gc, obj1);
    munit_assert_true(result);

    // obj2 is _not_ the root
    result = simple_gc_is_root(&gc, obj2);
    munit_assert_false(result);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_root_management(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // add objects to root array
    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    bool result = simple_gc_add_root(&gc, obj1);
    munit_assert_true(result);
    result = simple_gc_add_root(&gc, obj2);
    munit_assert_true(result);

    // null GC
    result = simple_gc_add_root(NULL, obj1);
    munit_assert_false(result);

    // null ptr
    result = simple_gc_add_root(&gc, NULL);
    munit_assert_false(result);

    // invalid ptr
    int nope;
    result = simple_gc_add_root(&gc, &nope);
    munit_assert_false(result);

    // remove object from root array
    result = simple_gc_remove_root(&gc, obj1);
    munit_assert_true(result);

    // remove again, expect failure
    result = simple_gc_remove_root(&gc, obj1);
    munit_assert_false(result);

    // null GC
    result = simple_gc_remove_root(NULL, obj2);
    munit_assert_false(result);

    // null ptr
    result = simple_gc_remove_root(&gc, NULL);
    munit_assert_false(result);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_mark(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    // allocate objects
    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    // initially, all objects should be unmarked
    obj_header_t* header1 = simple_gc_find_header(&gc, obj1);
    obj_header_t* header2 = simple_gc_find_header(&gc, obj2);
    obj_header_t* header3 = simple_gc_find_header(&gc, obj3);
    munit_assert_false(header1->marked);
    munit_assert_false(header2->marked);
    munit_assert_false(header3->marked);

    // mark obj1 and obj2
    simple_gc_mark(&gc, obj1);
    simple_gc_mark(&gc, obj2);

    // only obj1 and obj2 should be marked
    munit_assert_true(header1->marked);
    munit_assert_true(header2->marked);
    munit_assert_false(header3->marked);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_mark_roots(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    // add obj1 and obj3 to root array
    simple_gc_add_root(&gc, obj1);
    simple_gc_add_root(&gc, obj3);

    simple_gc_mark_roots(&gc);

    // verify obj1 and obj3 are marked
    obj_header_t* header1 = simple_gc_find_header(&gc, obj1);
    obj_header_t* header2 = simple_gc_find_header(&gc, obj2);
    obj_header_t* header3 = simple_gc_find_header(&gc, obj3);

    munit_assert_true(header1->marked);
    munit_assert_false(header2->marked);
    munit_assert_true(header3->marked);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_sweep(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    munit_assert_size(simple_gc_object_count(&gc), ==, 3);

    // mark obj1 and obj3
    obj_header_t* header1 = simple_gc_find_header(&gc, obj1);
    obj_header_t* header3 = simple_gc_find_header(&gc, obj3);
    simple_gc_mark(&gc, obj1);
    simple_gc_mark(&gc, obj3);

    // only obj1 and obj2 should be marked
    munit_assert_true(header1->marked);
    munit_assert_true(header3->marked);

    // sweep should remove obj2
    simple_gc_sweep(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 2);

    // obj1 and obj3 still exist and are unmarked
    header1 = simple_gc_find_header(&gc, obj1);
    header3 = simple_gc_find_header(&gc, obj3);

    munit_assert_not_null(header1);
    munit_assert_not_null(header3);
    munit_assert_false(header1->marked);
    munit_assert_false(header3->marked);

    // obj2 should be swept
    munit_assert_null(simple_gc_find_header(&gc, obj2));

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_collect(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    // add obj1 to root array
    simple_gc_add_root(&gc, obj1);
    munit_assert_size(simple_gc_object_count(&gc), ==, 3);

    simple_gc_collect(&gc);

    // obj2 and obj3 should be collected
    munit_assert_size(simple_gc_object_count(&gc), ==, 1);
    munit_assert_not_null(simple_gc_find_header(&gc, obj1));
    munit_assert_null(simple_gc_find_header(&gc, obj2));
    munit_assert_null(simple_gc_find_header(&gc, obj3));

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_add_reference(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    int* obj1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* obj2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    bool result = simple_gc_add_reference(&gc, obj1, obj2);
    munit_assert_true(result);

    // NULL GC
    result = simple_gc_add_reference(NULL, obj1, obj2);
    munit_assert_false(result);

    // NULL from pointer
    result = simple_gc_add_reference(&gc, NULL, obj2);
    munit_assert_false(result);

    // NULL to pointer
    result = simple_gc_add_reference(&gc, obj1, NULL);
    munit_assert_false(result);

    // invalid pointers; not managed by GC
    int not_gc_obj;
    result = simple_gc_add_reference(&gc, &not_gc_obj, obj2);
    munit_assert_false(result);
    result = simple_gc_add_reference(&gc, obj1, &not_gc_obj);
    munit_assert_false(result);

    // remove reference between existing objects
    result = simple_gc_remove_reference(&gc, obj1, obj2);
    munit_assert_true(result);

    // remove non-existent reference
    result = simple_gc_remove_reference(&gc, obj2, obj1);
    munit_assert_false(result);

    // free
    simple_gc_destroy(&gc);
    return MUNIT_OK;
}

static MunitResult test_gc_array_references(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    int** array = (int**)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 3 * sizeof(int*));
    int* elem1 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* elem2 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    int* elem3 = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

    *elem1 = 111;
    *elem2 = 222;
    *elem3 = 333;
    array[0] = elem1;
    array[1] = elem2;
    array[2] = elem3;

    simple_gc_add_reference(&gc, array, elem1);
    simple_gc_add_reference(&gc, array, elem2);
    simple_gc_add_reference(&gc, array, elem3);

    // add array as root
    simple_gc_add_root(&gc, array);
    munit_assert_size(simple_gc_object_count(&gc), ==, 4);

    // run garbage collection
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 4);
    munit_assert_int(*elem1, ==, 111);
    munit_assert_int(*elem2, ==, 222);
    munit_assert_int(*elem3, ==, 333);

    // remove array from roots; should also remove references
    simple_gc_remove_root(&gc, array);
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);

    // free
    simple_gc_destroy(&gc);
    return MUNIT_OK;
}

typedef struct {
    int id;
    double* value;
    char* name;
} TestStruct;

static MunitResult test_gc_struct_references(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    TestStruct* test_obj = (TestStruct*)simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, sizeof(TestStruct));
    double* value = (double*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(double));
    char* name = (char*)simple_gc_alloc(&gc, OBJ_TYPE_ARRAY, 20 * sizeof(char));

    test_obj->id = 42;
    test_obj->value = value;
    test_obj->name = name;

    *value = 3.14159;
    strcpy(name, "Test Object");

    simple_gc_add_reference(&gc, test_obj, value);
    simple_gc_add_reference(&gc, test_obj, name);
    simple_gc_add_root(&gc, test_obj);
    munit_assert_size(simple_gc_object_count(&gc), ==, 3);

    // run garbage collection
    simple_gc_collect(&gc);

    // all objects should still exist because TestStruct references them
    munit_assert_size(simple_gc_object_count(&gc), ==, 3);
    munit_assert_int(test_obj->id, ==, 42);
    munit_assert_double(*test_obj->value, ==, 3.14159);
    munit_assert_string_equal(test_obj->name, "Test Object");

    // remove one reference then run GC
    simple_gc_remove_reference(&gc, test_obj, name);
    simple_gc_collect(&gc);

    // object should be collected, but its value should still exist
    munit_assert_size(simple_gc_object_count(&gc), ==, 2);

    // remove TestStruct from roots then run GC; all objects should be collected
    simple_gc_remove_root(&gc, test_obj);
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);

    // free
    simple_gc_destroy(&gc);
    return MUNIT_OK;
}

static MunitResult test_gc_complex_reference_graph(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    void* a = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* b = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* c = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* d = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* e = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    munit_assert_size(simple_gc_object_count(&gc), ==, 5);

    // graph representation:
    // a -> b -> d
    //  \     /
    //   -> c -> e
    simple_gc_add_root(&gc, a);
    simple_gc_add_reference(&gc, a, b);
    simple_gc_add_reference(&gc, a, c);
    simple_gc_add_reference(&gc, b, d);
    simple_gc_add_reference(&gc, c, d);
    simple_gc_add_reference(&gc, c, e);

    // run garbage collection
    simple_gc_collect(&gc);

    // no iniital collection (everything has a reference)
    munit_assert_size(simple_gc_object_count(&gc), ==, 5);
    munit_assert_not_null(simple_gc_find_header(&gc, a));
    munit_assert_not_null(simple_gc_find_header(&gc, b));
    munit_assert_not_null(simple_gc_find_header(&gc, c));
    munit_assert_not_null(simple_gc_find_header(&gc, d));
    munit_assert_not_null(simple_gc_find_header(&gc, e));

    // break reference chain from c -> e
    simple_gc_remove_reference(&gc, c, e);

    // run garbage collection
    simple_gc_collect(&gc);

    // e is no longer referenced and should be collected
    munit_assert_size(simple_gc_object_count(&gc), ==, 4);
    munit_assert_not_null(simple_gc_find_header(&gc, a));
    munit_assert_not_null(simple_gc_find_header(&gc, b));
    munit_assert_not_null(simple_gc_find_header(&gc, c));
    munit_assert_not_null(simple_gc_find_header(&gc, d));
    munit_assert_null(simple_gc_find_header(&gc, e));

    // remove the root
    simple_gc_remove_root(&gc, a);

    // run garbage collection
    simple_gc_collect(&gc);

    // everything should be collected
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);

    // free
    simple_gc_destroy(&gc);

    return MUNIT_OK;
}

static MunitResult test_gc_cyclic_references(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;

    gc_t gc;
    simple_gc_init(&gc, 1024);

    void* a = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* b = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    void* c = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    munit_assert_size(simple_gc_object_count(&gc), ==, 3);

    /* cyclic graph:
     *      A -> B -> C
     *      ^        /
     *       \      /
     *         ----
    */
    simple_gc_add_reference(&gc, a, b);
    simple_gc_add_reference(&gc, b, c);
    simple_gc_add_reference(&gc, c, a);

    // no roots; everything should be collected
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);

    // recreate cycle with root a
    a = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    b = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    c = simple_gc_alloc(&gc, OBJ_TYPE_STRUCT, 16);
    simple_gc_add_reference(&gc, a, b);
    simple_gc_add_reference(&gc, b, c);
    simple_gc_add_reference(&gc, c, a);
    simple_gc_add_root(&gc, a);

    // no iniital collection (everything has a reference)
    simple_gc_collect(&gc);
    munit_assert_size(simple_gc_object_count(&gc), ==, 3);
    munit_assert_not_null(simple_gc_find_header(&gc, a));
    munit_assert_not_null(simple_gc_find_header(&gc, b));
    munit_assert_not_null(simple_gc_find_header(&gc, c));

    // remove the root
    simple_gc_remove_root(&gc, a);

    // run garbage collection
    simple_gc_collect(&gc);

    // everything should be collected
    munit_assert_size(simple_gc_object_count(&gc), ==, 0);

    // free
    simple_gc_destroy(&gc);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/version", test_version, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/init_header", test_init_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/is_valid_header", test_is_valid_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_new", test_gc_new, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_init", test_gc_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_object_count", test_gc_object_count, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_heap_capacity", test_gc_heap_capacity, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_heap_used", test_gc_heap_used, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_alloc", test_gc_alloc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_alloc_boundary", test_gc_alloc_boundary, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_alloc_stress", test_gc_alloc_stress, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_find_header", test_gc_find_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_is_root", test_gc_is_root, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_root_management", test_gc_root_management, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_mark", test_gc_mark, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_mark_roots", test_gc_mark_roots, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_sweep", test_gc_sweep, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_collect", test_gc_collect, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_add_reference", test_gc_add_reference, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_array_references", test_gc_array_references, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_struct_references", test_gc_struct_references, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_complex_reference_graph", test_gc_complex_reference_graph, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/gc_cyclic_references", test_gc_cyclic_references, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/simple_gc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
