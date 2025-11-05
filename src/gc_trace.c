#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "gc_trace.h"
#include "simple_gc.h"


// internal trace context
struct gc_trace_context {
  gc_trace_config_t config;
  gc_trace_event_t *events;
  size_t event_count;
  size_t event_capacity;

  uint64_t start_time_ns;
  pthread_mutex_t lock;

  gc_trace_stats_t stats;
};


static uint64_t get_timestamp_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t) clock() * 1000ULL;
}

static uint32_t get_thread_id(void) {
  return (uint32_t) pthread_self();
}

gc_trace_config_t gc_trace_default_config(void) {
  gc_trace_config_t config = {
    .enabled = true,
    .format = GC_TRACE_FORMAT_TEXT,
    .output = NULL,
    .buffer_size = 1024,
    .trace_allocs = true,
    .trace_collections = true,
    .trace_pressure = true,
    .trace_promotions = true,
  };
  return config;
}

gc_trace_t *gc_trace_create(const gc_trace_config_t *config) {
  if (!config) return NULL;

  gc_trace_t *trace = (gc_trace_t*) calloc(1, sizeof(gc_trace_t));
  if (!trace) return NULL;

  trace->config = *config;
  trace->event_capacity = config->buffer_size;
  trace->events = (gc_trace_event_t*) malloc(sizeof(gc_trace_event_t) * trace->event_capacity);

  if (!trace->events) {
    free(trace);
    return NULL;
  }

  trace->event_count = 0;
  trace->start_time_ns = get_timestamp_ns();
  pthread_mutex_init(&trace->lock, NULL);

  memset(&trace->stats, 0, sizeof(gc_trace_stats_t));
  return trace;
}

void gc_trace_destroy(gc_trace_t *trace) {
  if (!trace) return;

  pthread_mutex_destroy(&trace->lock);
  free(trace->events);

  if (trace->config.output && trace->config.output != stdout && trace->config.output != stderr) {
    fclose(trace->config.output);
  }

  free(trace);
}

bool gc_trace_begin(gc_t *gc, const char *filename, gc_trace_format_t format) {
  if (!gc || !filename) return false;

  gc_trace_config_t config = gc_trace_default_config();
  config.format = format;
  config.output = fopen(filename, "w");

  if (!config.output) return false;

  gc->trace = gc_trace_create(&config);

  if (!gc->trace) {
    fclose(config.output);
    return false;
  }

  if (format == GC_TRACE_FORMAT_JSON) {
    fprintf(config.output, "{\n  \"events\": [\n");
  } else if (format == GC_TRACE_FORMAT_CHROME) {
    fprintf(config.output, "[\n");
  }

  return true;
}

void gc_trace_end(gc_t *gc) {
  if (!gc || !gc->trace) return;

  gc_trace_flush(gc);

  if (gc->trace->config.format == GC_TRACE_FORMAT_JSON) {
    fprintf(gc->trace->config.output, "\n  ]\n}[\n");
  } else if (gc->trace->config.format == GC_TRACE_FORMAT_CHROME) {
    fprintf(gc->trace->config.output, "\n]\n");
  }

  gc_trace_destroy(gc->trace);
  gc->trace = NULL;
}

static const char *event_type_to_string(gc_event_type_t type) {
  switch (type) {
    case GC_EVENT_ALLOC: return "alloc";
    case GC_EVENT_FREE: return "free";
    case GC_EVENT_COLLECT_START: return "collect_start";
    case GC_EVENT_COLLECT_END: return "collect_end";
    case GC_EVENT_MARK_START: return "mark_start";
    case GC_EVENT_MARK_END: return "mark_end";
    case GC_EVENT_SWEEP_START: return "sweep_start";
    case GC_EVENT_SWEEP_END: return "sweep_end";
    case GC_EVENT_COMPACT_START: return "compact_start";
    case GC_EVENT_COMPACT_END: return "compact_end";
    case GC_EVENT_PRESSURE_CHANGE: return "pressure_change";
    case GC_EVENT_PROMOTION: return "promotion";
    case GC_EVENT_ROOT_ADD: return "root_add";
    case GC_EVENT_ROOT_REMOVE: return "root_remove";
    default: return "unknown";
  }
}

static void write_event_text(FILE *out, const gc_trace_event_t *event) {
  double time_ms = event->timestamp_ns / 1000000.0;

  fprintf(out, "[%10.3f ms] [thread %u] %s",
      time_ms, event->thread_id, event_type_to_string(event->type));

  switch (event->type) {
    case GC_EVENT_ALLOC:
      fprintf(out, " addr=%p size=%zu type=%d at %s:%d\n",
          event->data.alloc.address,
          event->data.alloc.size,
          event->data.alloc.obj_type,
          event->data.alloc.file ? event->data.alloc.file : "?",
          event->data.alloc.line);
      break;

    case GC_EVENT_FREE:
      fprintf(out, " addr=%p size=%zu\n",
          event->data.free.address,
          event->data.free.size);
      break;

    case GC_EVENT_COLLECT_START:
      fprintf(out, " type=%s objects=%zu bytes=%zu\n",
          event->data.collect_start.type,
          event->data.collect_start.objects_before,
          event->data.collect_start.bytes_before);
      break;

    case GC_EVENT_COLLECT_END:
      fprintf(out, " objects=%zu bytes=%zu collected=%zu promoted=%zu duration=%.3f ms\n",
          event->data.collect_end.objects_after,
          event->data.collect_end.bytes_after,
          event->data.collect_end.collected,
          event->data.collect_end.promoted,
          event->data.collect_end.duration_ms);
      break;

    case GC_EVENT_PRESSURE_CHANGE:
      fprintf(out, " level=%d\n",
          event->data.pressure.pressure_level);
      break;

    case GC_EVENT_PROMOTION:
      fprintf(out, " addr=%p gen=%d->%d\n",
          event->data.promotion.address,
          event->data.promotion.old_gen,
          event->data.promotion.new_gen);
      break;

    default:
      fprintf(out, "\n");
      break;
  }
}

static void write_event_json(FILE *out, const gc_trace_event_t *event, bool first) {
  if (!first) fprintf(out, ",\n");

  fprintf(out, "    {\n");
  fprintf(out, "      \"type\": \"%s\",\n", event_type_to_string(event->type));
  fprintf(out, "      \"timestamp_ns\": %llu,\n", (unsigned long long) event->timestamp_ns);
  fprintf(out, "      \"thread_id\": %u,\n", event->thread_id);

  switch (event->type) {
    case GC_EVENT_ALLOC:
      fprintf(out, ",\n      \"address\": \"%p\",\n", event->data.alloc.address);
      fprintf(out, "      \"size\": %zu,\n", event->data.alloc.size);
      fprintf(out, "      \"obj_type\": %d,\n", event->data.alloc.obj_type);
      fprintf(out, "      \"file\": \"%s\",\n",
              event->data.alloc.file ? event->data.alloc.file : "unknown");
      fprintf(out, "      \"line\": %d", event->data.alloc.line);
      break;

    case GC_EVENT_COLLECT_END:
      fprintf(out, ",\n      \"objects_after\": %zu,\n",
              event->data.collect_end.objects_after);
      fprintf(out, "      \"collected\": %zu,\n",
              event->data.collect_end.collected);
      fprintf(out, "      \"duration_ms\": %.3f",
              event->data.collect_end.duration_ms);
      break;

    default:
      break;
  }

  fprintf(out, "\n    }");
}

// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
static void write_event_chrome(FILE *out, const gc_trace_event_t *event, bool first) {
  if (!first) fprintf(out, ",\n");

  // chrome tracing format
  fprintf(out, "  {");
  fprintf(out, "\"name\":\"%s\",", event_type_to_string(event->type));
  fprintf(out, "\"cat\":\"gc\",");

  // phases: B=begin, E=end, X=complete, i=instant
  char phase = 'i';
  if (event->type == GC_EVENT_COLLECT_START
      || event->type == GC_EVENT_MARK_START
      || event->type == GC_EVENT_SWEEP_START
      || event->type == GC_EVENT_COMPACT_START) {
    phase = 'B';
  } else if (event->type == GC_EVENT_COLLECT_END
      || event->type == GC_EVENT_MARK_END
      || event->type == GC_EVENT_SWEEP_END
      || event->type == GC_EVENT_COMPACT_END) {
    phase = 'E';
  }

  fprintf(out, "\"ph\":\"%c\",", phase);
  fprintf(out, "\"ts\":%llu,", (unsigned long long)(event->timestamp_ns / 1000));
  fprintf(out, "\"pid\":1,");
  fprintf(out, "\"tid\":%u", event->thread_id);

  // args for some events
  if (event->type == GC_EVENT_ALLOC) {
    fprintf(out, ",\"args\":{\"size\":%zu,\"addr\":\"%p\"}",
            event->data.alloc.size, event->data.alloc.address);
  } else if (event->type == GC_EVENT_COLLECT_END) {
    fprintf(out, ",\"args\":{\"collected\":%zu,\"duration_ms\":%.3f}",
            event->data.collect_end.collected,
            event->data.collect_end.duration_ms);
  }

  fprintf(out, "}");
}

static void gc_trace_flush_internal(gc_t *gc, bool locked) {
  if (!gc || !gc->trace) return;

  gc_trace_t *trace = gc->trace;

  if (!locked) {
    pthread_mutex_lock(&trace->lock);
  }

  for (size_t i = 0; i < trace->event_count; ++i) {
    gc_trace_event_t *event = &trace->events[i];

    switch (trace->config.format) {
      case GC_TRACE_FORMAT_TEXT:
        write_event_text(trace->config.output, event);
        break;

      case GC_TRACE_FORMAT_JSON:
        write_event_json(trace->config.output, event, i == 0);
        break;

      case GC_TRACE_FORMAT_CHROME:
        write_event_chrome(trace->config.output, event, i == 0);
        break;

      default: break;
    }
  }

  fflush(trace->config.output);
  trace->event_count = 0;

  if (!locked) {
    pthread_mutex_unlock(&trace->lock);
  }
}

void gc_trace_flush(gc_t *gc) {
  gc_trace_flush_internal(gc, false);
}

void gc_trace_event(gc_t *gc, const gc_trace_event_t *event) {
  if (!gc || !gc->trace || !event) return;

  gc_trace_t *trace = gc->trace;
  if (!trace->config.enabled) return;

  if ((event->type == GC_EVENT_COLLECT_START || event->type == GC_EVENT_COLLECT_END)
      && !trace->config.trace_collections) return;

  if ((event->type == GC_EVENT_ALLOC || event->type == GC_EVENT_FREE)
      && !trace->config.trace_allocs) return;

  pthread_mutex_lock(&trace->lock);

  gc_trace_event_t e = *event;
  e.timestamp_ns = get_timestamp_ns() - trace->start_time_ns;
  e.thread_id = get_thread_id();

  trace->stats.total_events++;

  switch (e.type) {
    case GC_EVENT_ALLOC:
      trace->stats.alloc_count++;
      trace->stats.total_allocated += e.data.alloc.size;
      break;

    case GC_EVENT_FREE:
      trace->stats.free_count++;
      trace->stats.total_freed += e.data.free.size;
      break;

    case GC_EVENT_COLLECT_END:
      trace->stats.collection_count++;
      trace->stats.total_gc_time_ms += e.data.collect_end.duration_ms;
      if (e.data.collect_end.duration_ms > trace->stats.max_gc_pause_ms) {
        trace->stats.max_gc_pause_ms = e.data.collect_end.duration_ms;
      }
      trace->stats.avg_gc_pause_ms = trace->stats.total_gc_time_ms / trace->stats.collection_count;
      break;

    case GC_EVENT_PROMOTION:
      trace->stats.promotion_count++;
      break;

    default: break;
  }

  // add event to buffer
  if (trace->event_count >= trace->event_capacity) {
    gc_trace_flush_internal(gc, true); // already locked
  }

  trace->events[trace->event_count++] = e;
  pthread_mutex_unlock(&trace->lock);
}

void gc_trace_get_stats(gc_t *gc, gc_trace_stats_t *stats) {
  if (!gc || !gc->trace || !stats) return;

  pthread_mutex_lock(&gc->trace->lock);
  *stats = gc->trace->stats;
  stats->objects_leaked = stats->alloc_count - stats->free_count;
  stats->peak_memory = stats->total_allocated - stats->total_freed;
  pthread_mutex_unlock(&gc->trace->lock);
}

void gc_trace_print_stats(gc_t *gc, FILE *out) {
  if (!gc || !gc->trace || !out) return;

  gc_trace_stats_t stats;
  gc_trace_get_stats(gc, &stats);

  fprintf(out, "\n=== GC Trace Statistics ===\n");
  fprintf(out, "Total events:      %zu\n", stats.total_events);
  fprintf(out, "Allocations:       %zu\n", stats.alloc_count);
  fprintf(out, "Frees:             %zu\n", stats.free_count);
  fprintf(out, "Collections:       %zu\n", stats.collection_count);
  fprintf(out, "Promotions:        %zu\n", stats.promotion_count);
  fprintf(out, "\n");
  fprintf(out, "Total allocated:   %zu bytes\n", stats.total_allocated);
  fprintf(out, "Total freed:       %zu bytes\n", stats.total_freed);
  fprintf(out, "Peak memory:       %zu bytes\n", stats.peak_memory);
  fprintf(out, "Objects leaked:    %zu\n", stats.objects_leaked);
  fprintf(out, "\n");
  fprintf(out, "Total GC time:     %.3f ms\n", stats.total_gc_time_ms);
  fprintf(out, "Average pause:     %.3f ms\n", stats.avg_gc_pause_ms);
  fprintf(out, "Max pause:         %.3f ms\n", stats.max_gc_pause_ms);
  fprintf(out, "===========================\n\n");
}
