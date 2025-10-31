#include "simple_gc.h"
#include "gc_visualizer.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int id;
  char *name;
} Person;

void create_temporary_objects(gc_t *gc) {
  // these objects will die when function returns
  int *temp1 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));
  int *temp2 = (int *)simple_gc_alloc(gc, OBJ_TYPE_PRIMITIVE, sizeof(int));

  *temp1 = 999;
  *temp2 = 888;

  printf("Created temporary objects: %d, %d\n", *temp1, *temp2);
  printf("Object count: %zu\n", simple_gc_object_count(gc));
}

int main(void) {
  printf("=== Automatic Stack Scanning Demo ===\n\n");

  // create GC with automatic stack scanning
  gc_t *gc = simple_gc_new_auto(4096);
  if (!gc) {
    fprintf(stderr, "Failed to create GC\n");
    return 1;
  }

  printf("GC initialized with automatic stack scanning\n");
  printf("Capacity: %zu bytes\n\n", simple_gc_heap_capacity(gc));

  // allocate persistent (on stack in main) objects
  Person *alice = (Person *)simple_gc_alloc(gc, OBJ_TYPE_STRUCT, sizeof(Person));
  Person *bob = (Person *)simple_gc_alloc(gc, OBJ_TYPE_STRUCT, sizeof(Person));

  alice->id = 1;
  alice->name = (char *)simple_gc_alloc(gc, OBJ_TYPE_ARRAY, 20);
  snprintf(alice->name, 20, "Alice");

  bob->id = 2;
  bob->name = (char *)simple_gc_alloc(gc, OBJ_TYPE_ARRAY, 20);
  snprintf(bob->name, 20, "Bob");

  // add references to persistent objects
  simple_gc_add_reference(gc, alice, alice->name);
  simple_gc_add_reference(gc, bob, bob->name);

  printf("Created persistent objects:\n");
  printf("  Alice (id=%d, name=%s)\n", alice->id, alice->name);
  printf("  Bob (id=%d, name=%s)\n", bob->id, bob->name);
  printf("Object count: %zu\n\n", simple_gc_object_count(gc));

  // create temporary objects in a function
  printf("Creating temporary objects...\n");
  create_temporary_objects(gc);
  printf("\n");

  // garbage collection cleans up temporary objects
  printf("Running garbage collection...\n");
  simple_gc_collect(gc);
  printf("Object count after GC: %zu\n", simple_gc_object_count(gc));
  printf("(Temporary objects were collected)\n\n");

  // persistent objects still reachable
  printf("Persistent objects still alive:\n");
  printf("  Alice (id=%d, name=%s)\n", alice->id, alice->name);
  printf("  Bob (id=%d, name=%s)\n", bob->id, bob->name);

  // free the things!
  simple_gc_destroy(gc);
  free(gc);

  printf("\n=== Demo Complete ===\n");
  return 0;
}
