#include "munit.h"
#include "gc_cardtable.h"
#include "simple_gc.h"
#include <stdio.h>

static MunitResult test_init(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;

  bool result = gc_cardtable_init(&table, heap, sizeof(heap));
  munit_assert_true(result);
  munit_assert_not_null(table.cards);
  munit_assert_size(table.num_cards, ==, (4096 + GC_CARD_SIZE - 1) / GC_CARD_SIZE);
  munit_assert_ptr_equal(table.heap_start, heap);
  munit_assert_true(table.enabled);

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_addr_to_card(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // first byte -> card 0
  size_t card = gc_cardtable_addr_to_card(&table, heap);
  munit_assert_size(card, ==, 0);

  // byte 512 -> card 1
  card = gc_cardtable_addr_to_card(&table, heap + 512);
  munit_assert_size(card, ==, 1);

  // byte 1024 -> card 2
  card = gc_cardtable_addr_to_card(&table, heap + 1024);
  munit_assert_size(card, ==, 2);

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_mark_dirty(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // initially clean
  munit_assert_false(gc_cardtable_is_dirty(&table, heap));
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 0);

  // mark dirty
  gc_cardtable_mark_dirty(&table, heap);
  munit_assert_true(gc_cardtable_is_dirty(&table, heap));
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 1);

  // mark same card again - should not increase count
  gc_cardtable_mark_dirty(&table, heap + 100);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 1);

  // mark different card
  gc_cardtable_mark_dirty(&table, heap + 512);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 2);

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_mark_range(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // mark range spanning multiple cards
  gc_cardtable_mark_range_dirty(&table, heap, 1024);

  // should mark cards 0 and 1
  munit_assert_true(gc_cardtable_is_dirty(&table, heap));
  munit_assert_true(gc_cardtable_is_dirty(&table, heap + 512));
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 2);

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_clear(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // mark several cards dirty
  gc_cardtable_mark_dirty(&table, heap);
  gc_cardtable_mark_dirty(&table, heap + 512);
  gc_cardtable_mark_dirty(&table, heap + 1024);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 3);

  // clear all
  gc_cardtable_clear(&table);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 0);
  munit_assert_false(gc_cardtable_is_dirty(&table, heap));

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_clear_single(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  gc_cardtable_mark_dirty(&table, heap);
  gc_cardtable_mark_dirty(&table, heap + 512);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 2);

  // clear single card
  gc_cardtable_clear_card(&table, 0);
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 1);
  munit_assert_false(gc_cardtable_is_dirty(&table, heap));
  munit_assert_true(gc_cardtable_is_dirty(&table, heap + 512));

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static MunitResult test_statistics(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // mark half the cards dirty
  for (size_t i = 0; i < 4; i++) {
    gc_cardtable_mark_dirty(&table, heap + (i * 512));
  }

  float ratio = gc_cardtable_dirty_ratio(&table);
  munit_assert_double(ratio, >, 0.0f);
  munit_assert_double(ratio, <=, 1.0f);

  gc_cardtable_print_stats(&table);

  gc_cardtable_destroy(&table);
  return MUNIT_OK;
}

static void scan_callback(gc_t *gc, void *card_start, void *card_end, void *user_data) {
  (void)gc;
  (void)card_start;
  (void)card_end;

  int *count = (int *)user_data;
  (*count)++;
}

static MunitResult test_scan_dirty(const MunitParameter params[], void *data) {
  (void)params;
  (void)data;

  gc_t gc;
  simple_gc_init(&gc, 4096);

  char heap[4096];
  gc_cardtable_t table;
  gc_cardtable_init(&table, heap, sizeof(heap));

  // mark some cards dirty
  gc_cardtable_mark_dirty(&table, heap);
  gc_cardtable_mark_dirty(&table, heap + 1024);
  gc_cardtable_mark_dirty(&table, heap + 2048);

  int scan_count = 0;
  gc_cardtable_scan_dirty(&gc, &table, scan_callback, &scan_count);

  // should have scanned 3 dirty cards
  munit_assert_int(scan_count, ==, 3);

  // cards should be clean after scan
  munit_assert_size(gc_cardtable_dirty_count(&table), ==, 0);

  gc_cardtable_destroy(&table);
  simple_gc_destroy(&gc);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/init", test_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/addr_to_card", test_addr_to_card, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark_dirty", test_mark_dirty, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/mark_range", test_mark_range, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/clear", test_clear, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/clear_single", test_clear_single, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/statistics", test_statistics, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/scan_dirty", test_scan_dirty, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite suite = {"/cardtable", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
