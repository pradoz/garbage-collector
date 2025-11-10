#include "gc_cardtable.h"
#include "simple_gc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


bool gc_cardtable_init(gc_cardtable_t *table, void *heap_start, size_t heap_size) {
  if (!table || !heap_start || heap_size == 0) return false;

  size_t num_cards = (heap_size + GC_CARD_SIZE - 1) >> GC_CARD_SHIFT;

  table->cards = (uint8_t*) calloc(num_cards, sizeof(uint8_t));
  if (!table->cards) return false;

  table->num_cards = num_cards;
  table->heap_start = heap_start;
  table->heap_end = (void*)((char*) heap_start + heap_size);
  table->dirty_count = 0;
  table->enabled = true;

  return true;
}

void gc_cardtable_destroy(gc_cardtable_t *table) {
  if (!table) return;

  free(table->cards);
  table->cards = NULL;
  table->num_cards = 0;
  table->enabled = false;
}

size_t gc_cardtable_addr_to_card(gc_cardtable_t *table, void *addr) {
  if (!table || !addr) return 0;
  if (addr < table->heap_start || addr >= table->heap_end) return 0;

  ptrdiff_t offset = (char*)addr - (char*)table->heap_start;
  return (size_t)(offset >> GC_CARD_SHIFT);
}

void *gc_cardtable_card_to_addr(gc_cardtable_t *table, size_t card_index) {
  if (!table || card_index >= table->num_cards) return NULL;

  size_t offset = card_index << GC_CARD_SHIFT;
  return ((void*) ((char*)table->heap_start + offset));
}

void gc_cardtable_mark_dirty(gc_cardtable_t *table, void *addr) {
  if (!table || !table->enabled || !addr) return;

  size_t c = gc_cardtable_addr_to_card(table, addr);
  if (c < table->num_cards) {
    if (table->cards[c] == GC_CARD_CLEAN) {
      table->dirty_count++;
    }
    table->cards[c] = GC_CARD_DIRTY;
  }
}

void gc_cardtable_mark_range_dirty(gc_cardtable_t *table, void *start, size_t size) {
  if (!table || !table->enabled || !start || size == 0) return;

  size_t start_card = gc_cardtable_addr_to_card(table, start);
  void *end = (void*)((char*)start + size - 1);
  size_t end_card = gc_cardtable_addr_to_card(table, end);

  for (size_t i = start_card; i <= end_card && i < table->num_cards ; ++i) {
    if (table->cards[i] == GC_CARD_CLEAN) {
      table->dirty_count++;
    }
    table->cards[i] = GC_CARD_DIRTY;
  }
}

bool gc_cardtable_is_dirty(gc_cardtable_t *table, void *addr) {
  if (!table || !addr) return false;

  size_t c = gc_cardtable_addr_to_card(table, addr);
  if (c >= table->num_cards) return false;

  return (table->cards[c] == GC_CARD_DIRTY);
}

void gc_cardtable_clear(gc_cardtable_t *table) {
  if (!table || !table->cards) return;

  memset(table->cards, GC_CARD_CLEAN, table->num_cards);
  table->dirty_count = 0;
}

void gc_cardtable_clear_card(gc_cardtable_t *table, size_t card_index) {
  if (!table || card_index >= table->num_cards) return;

  if (table->cards[card_index] == GC_CARD_DIRTY) {
    table->dirty_count--;
  }
  table->cards[card_index] = GC_CARD_CLEAN;
}

void gc_cardtable_scan_dirty(gc_t *gc, gc_cardtable_t *table, gc_card_scan_fn callback, void *user_data) {
  if (!gc || !table || !callback) return;

  for (size_t c = 0; c < table->num_cards; ++c) {
    if (table->cards[c] == GC_CARD_DIRTY) {
      void *start = gc_cardtable_card_to_addr(table, c);
      void *end = (void*)((char*) start + GC_CARD_SIZE);
      callback(gc, start, end, user_data);
      gc_cardtable_clear_card(table, c);
    }
  }
}

size_t gc_cardtable_dirty_count(gc_cardtable_t *table) {
  return (!table) ? 0 : table->dirty_count;
}

float gc_cardtable_dirty_ratio(gc_cardtable_t *table) {
  if (!table || table->num_cards == 0) return 0.0f;
  return (float)table->dirty_count / (float)table->num_cards;
}

void gc_cardtable_print_stats(gc_cardtable_t *table) {
  if (!table) return;

  printf("\n=== Card Table Statistics ===\n");
  printf("Total cards:   %zu\n", table->num_cards);
  printf("Dirty cards:   %zu\n", table->dirty_count);
  printf("Dirty ratio:   %.2f%%\n", gc_cardtable_dirty_ratio(table) * 100.0f);
  printf("Card size:     %d bytes\n", GC_CARD_SIZE);
  printf("Heap tracked:  %zu bytes\n", (size_t)((char *)table->heap_end - (char *)table->heap_start));
  printf("=============================\n\n");
}
