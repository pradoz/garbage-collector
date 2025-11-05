#include "gc_debug.h"
#include "simple_gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>


static uint64_t get_time_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t) ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint32_t get_thread_id_debug(void) {
  return (uint32_t) pthread_self();
}


bool gc_debug_enable(gc_t *gc) {
  if (!gc) return false;

  gc_debug_t *debug = (gc_debug_t*) calloc(1, sizeof(gc_debug_t));
  if (!debug) return false;

  debug->allocations = NULL;
  debug->alloc_count = 0;
  debug->next_alloc_id = 1;
  debug->track_stacks = false; // disable by default (expensive)
  debug->check_double_free = true;
  debug->check_use_after_free = true;

  pthread_mutex_init(&debug->lock, NULL);

  gc->debug = debug;
  return true;
}

void gc_debug_disable(gc_t *gc) {
  if (!gc || !gc->debug) return;

  gc_debug_t *debug = gc->debug;

  // free allocation info
  alloc_info_t *curr = debug->allocations;
  while (curr) {
    alloc_info_t *next = curr->next;
    free(curr);
    curr = next;
  }

  pthread_mutex_destroy(&debug->lock);
  free(debug);
  gc->debug = NULL;
}

bool gc_debug_is_enabled(gc_t *gc) {
  return (gc && gc->debug);
}

void gc_debug_track_alloc(gc_t *gc, void *ptr, size_t size, obj_type_t type,
        const char *file, int line, const char *func) {
  if (!gc || !gc->debug) return;

  gc_debug_t *debug = gc->debug;
  alloc_info_t *info = (alloc_info_t*) malloc(sizeof(alloc_info_t));
  if (!info) return;

  info->address = ptr;
  info->size = size;
  info->type = type;
  info->file = file;
  info->line = line;
  info->function = func;
  info->alloc_time = get_time_us();
  info->thread_id = get_thread_id_debug();
  info->freed = false;
  info->free_time = 0;

  pthread_mutex_lock(&debug->lock);

  info->alloc_id = debug->next_alloc_id++;
  info->next = debug->allocations;
  debug->allocations = info;
  debug->alloc_count++;

  pthread_mutex_unlock(&debug->lock);
}

void gc_debug_track_free(gc_t *gc, void *ptr) {
  if (!gc || !gc->debug || !ptr) return;

  gc_debug_t *debug = gc->debug;

  pthread_mutex_lock(&debug->lock);

  alloc_info_t *info = debug->allocations;
  while (info) {
    if (info->address == ptr) {
      if (debug->check_double_free && info->freed) {
        fprintf(stderr, "ERROR: Double free detected at %p\n", ptr);
        fprintf(stderr, "  Originally allocated at %s:%d in %s\n", info->file, info->line, info->function);
        fprintf(stderr, "  First freed at time %llu\n", (unsigned long long)info->free_time);
      }

      info->freed = true;
      info->free_time = get_time_us();
      break;
    }
    info = info->next;
  }

  pthread_mutex_unlock(&debug->lock);
}

gc_leak_report_t *gc_debug_find_leaks(gc_t *gc) {
  if (!gc || !gc->debug) return NULL;

  gc_debug_t *debug = gc->debug;
  gc_leak_report_t *report = (gc_leak_report_t*) calloc(1, sizeof(gc_leak_report_t));
  if (!report) return NULL;

  pthread_mutex_lock(&debug->lock);

  size_t leak_count = 0;
  size_t leaked_bytes = 0;

  alloc_info_t *info = debug->allocations;
  while (info) {
    if (!info->freed) {
      leak_count++;
      leaked_bytes += info->size;
    }
    info = info->next;
  }

  report->leaked_objects = leak_count;
  report->leaked_bytes = leaked_bytes;

  if (leak_count > 0) {
    report->leaks = (alloc_info_t**) malloc(sizeof(alloc_info_t*) * leak_count);
    if (report->leaks) {
      size_t index = 0;
      info = debug->allocations;
      while (info && index < leak_count) {
        if (!info->freed) {
          report->leaks[index++] = info;
        }
        info = info->next;
      }
    }
  } else {
    report->leaks = NULL;
  }

  pthread_mutex_unlock(&debug->lock);
  return report;
}

void gc_debug_free_leak_report(gc_leak_report_t *report) {
  if (!report) return;
  free(report->leaks);
  free(report);
}

alloc_info_t *gc_debug_find_alloc(gc_t *gc, void *ptr) {
  if (!gc || !gc->debug || !ptr) return NULL;

  gc_debug_t *debug = gc->debug;

  pthread_mutex_lock(&debug->lock);

  alloc_info_t *info = debug->allocations;
  while (info) {
    if (info->address == ptr) {
      pthread_mutex_unlock(&debug->lock);
      return info;
    }
    info = info->next;
  }

  pthread_mutex_unlock(&debug->lock);
  return NULL;
}

void gc_debug_print_leaks(gc_t *gc, FILE *out) {
  if (!gc || !out) return;

  gc_leak_report_t *report = gc_debug_find_leaks(gc);
  if (!report) return;

  fprintf(out, "\n=== Memory Leak Report ===\n");
  fprintf(out, "Leaked objects: %zu\n", report->leaked_objects);
  fprintf(out, "Leaked bytes:   %zu\n", report->leaked_bytes);
  fprintf(out, "\n");

  if (report->leaked_objects > 0) {
    fprintf(out, "Leak details:\n");
    for (size_t i = 0; i < report->leaked_objects; i++) {
      gc_debug_print_alloc_info(report->leaks[i], out);
      fprintf(out, "\n");
    }
  } else {
    fprintf(out, "No leaks detected!\n");
  }

  fprintf(out, "==========================\n\n");
  gc_debug_free_leak_report(report);
}

void gc_debug_print_alloc_info(alloc_info_t *info, FILE *out) {
  if (!info || !out) return;

  fprintf(out, "  Allocation #%llu at %p\n", (unsigned long long)info->alloc_id, info->address);
  fprintf(out, "    Size:     %zu bytes\n", info->size);
  fprintf(out, "    Type:     %d\n", info->type);
  fprintf(out, "    Location: %s:%d in %s()\n", info->file, info->line, info->function);
  fprintf(out, "    Time:     %llu us\n", (unsigned long long)info->alloc_time);
  fprintf(out, "    Thread:   %u\n", info->thread_id);
  fprintf(out, "    Status:   %s\n", info->freed ? "FREED" : "LIVE");

  if (info->freed) {
    uint64_t lifetime = info->free_time - info->alloc_time;
    fprintf(out, "    Lifetime: %llu us\n", (unsigned long long)lifetime);
  }
}

void gc_debug_dump_allocations(gc_t *gc, FILE *out) {
  if (!gc || !gc->debug || !out) return;

  gc_debug_t *debug = gc->debug;

  pthread_mutex_lock(&debug->lock);

  fprintf(out, "\n=== All Allocations ===\n");
  fprintf(out, "Total tracked: %zu\n\n", debug->alloc_count);

  alloc_info_t *info = debug->allocations;
  while (info) {
    gc_debug_print_alloc_info(info, out);
    fprintf(out, "\n");
    info = info->next;
  }

  fprintf(out, "=======================\n\n");

  pthread_mutex_unlock(&debug->lock);
}

bool gc_debug_validate_heap(gc_t *gc) {
  if (!gc) return false;

  // validate all object headers
  obj_header_t *header = gc->objects;
  while (header) {
    if (!gc_is_valid_header(header)) {
      fprintf(stderr, "ERROR: Invalid object header at %p\n", (void*) header);
      return false;
    }
    header = header->next;
  }

  // check that roots point to valid objects
  for (size_t i = 0; i < gc->root_count; ++i) {
    if (!simple_gc_find_header(gc, gc->roots[i])) {
      fprintf(stderr, "ERROR: Root %zu points to invalid object %p\n", i, gc->roots[i]);
      return false;
    }
  }

  return true;
}

bool gc_debug_check_pointer(gc_t *gc, void *ptr) {
  if (!gc || !ptr) return false;

  obj_header_t *header = simple_gc_find_header(gc, ptr);
  if (!header) return false;

  if (gc->debug) {
    pthread_mutex_lock(&gc->debug->lock);

    alloc_info_t *info = gc_debug_find_alloc(gc, ptr);
    if (info && info->freed && gc->debug->check_use_after_free) {
      fprintf(stderr, "ERROR: Use after free detected at %p\n", ptr);
      fprintf(stderr, "  Originally allocated at %s:%d\n", info->file, info->line);
      fprintf(stderr, "  Freed at time %llu\n", (unsigned long long)info->free_time);
      pthread_mutex_unlock(&gc->debug->lock);
      return false;
    }

    pthread_mutex_unlock(&gc->debug->lock);
  }

  return true;
}
