#include "gc_platform.h"


#include <pthread.h>
#include <unistd.h>


void *gc_platform_get_stack_bottom(void) {
  pthread_t self = pthread_self();
  pthread_attr_t attr;

  void *stack_addr;
  size_t stack_size;
  pthread_getattr_np(self, &attr);

  if (pthread_attr_getstack(&attr, &stack_addr, &stack_size) == 0) {
    pthread_attr_destroy(&attr);
    // stack bottom == highest address
    return (void*) ((char*) stack_addr + stack_size);
  }

  pthread_attr_destroy(&attr);
  return NULL;

}

void *gc_platform_get_stack_pointer(void) {
  // approximate using a local variable
  void *sp;
  void *ptr = &sp;
  return ptr;
}
