#ifndef GC_PLATFORM_H
#define GC_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>

// TODO: other OS
#define GC_PLATFORM_POSIX 1


void *gc_platform_get_stack_bottom(void);
void *gc_platform_get_stack_pointer(void);

void gc_platform_save_registers(void);


#endif /* GC_PLATFORM_H */
