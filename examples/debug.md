## Debug & Trace Usage

### Example 1: Trace GC Activity

```c
#include "simple_gc.h"
#include "gc_trace.h"

int main(void) {
  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  // start tracing to text file
  gc_trace_begin(&gc, "gc_trace.txt", GC_TRACE_FORMAT_TEXT);

  // some program
  for (int i = 0; i < 1000; i++) {
    int *obj = (int*)simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    if (i % 100 == 0) {
      simple_gc_add_root(&gc, obj);
    }
  }

  simple_gc_collect(&gc);

  // print stats
  gc_trace_print_stats(&gc, stdout);

  // clean up
  gc_trace_end(&gc);
  simple_gc_destroy(&gc);
  return 0;
}
```

### Example 2: Find Memory Leaks

```c
#include "simple_gc.h"
#include "gc_debug.h"

int main(void) {
  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  // enable debugging
  gc_debug_enable(&gc);

  // allocate with source location tracking
  int *leak1 = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *leak2 = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *root = (int*)GC_ALLOC_DEBUG(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  simple_gc_add_root(&gc, root);  // root won't leak

  // check for leaks
  gc_debug_print_leaks(&gc, stdout);

  // clean up
  gc_debug_disable(&gc);
  simple_gc_destroy(&gc);
  return 0;
}
```

### Example 3: Chrome Tracing Visualization

```c
#include "simple_gc.h"
#include "gc_trace.h"

int main(void) {
  gc_t gc;
  simple_gc_init(&gc, 1024 * 1024);

  // trace to chrome format
  gc_trace_begin(&gc, "gc_chrome.json", GC_TRACE_FORMAT_CHROME);

  // some program
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 100; j++) {
      simple_gc_alloc(&gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
    }
    simple_gc_collect(&gc);
  }

  // clean up
  gc_trace_end(&gc);
  simple_gc_destroy(&gc);

  printf("Open gc_chrome.json in chrome://tracing\n");
  return 0;
}
```
