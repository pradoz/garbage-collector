#ifndef GC_CARDTABLE_H
#define GC_CARDTABLE_H


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct gc_context gc_t;


#define GC_CARD_SIZE 512 // bytes per card
#define GC_CARD_SHIFT 9  // log2(512)
#define GC_CARD_CLEAN 0
#define GC_CARD_DIRTY 1


typedef struct gc_cardtable {
  uint8_t *cards;
  size_t num_cards;
  void *heap_start;
  void *heap_end;
  size_t dirty_count;
  bool enabled;
} gc_cardtable_t;

typedef void (*gc_card_scan_fn)(gc_t *gc, void *card_start, void *card_end, void *user_data);


bool gc_cardtable_init(gc_cardtable_t *table, void *heap_start, size_t heap_size);
void gc_cardtable_destroy(gc_cardtable_t *table);

size_t gc_cardtable_addr_to_card(gc_cardtable_t *table, void *addr);
void *gc_cardtable_card_to_addr(gc_cardtable_t *table, size_t card_index);
void gc_cardtable_mark_dirty(gc_cardtable_t *table, void *addr);
void gc_cardtable_mark_range_dirty(gc_cardtable_t *table, void *start, size_t size);
bool gc_cardtable_is_dirty(gc_cardtable_t *table, void *addr);

void gc_cardtable_clear(gc_cardtable_t *table);
void gc_cardtable_clear_card(gc_cardtable_t *table, size_t card_index);

void gc_cardtable_scan_dirty(gc_t *gc, gc_cardtable_t *table, gc_card_scan_fn callback, void *user_data);

size_t gc_cardtable_dirty_count(gc_cardtable_t *table);
float gc_cardtable_dirty_ratio(gc_cardtable_t *table);
void gc_cardtable_print_stats(gc_cardtable_t *table);


#endif /* GC_CARDTABLE_H */
