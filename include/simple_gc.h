#ifndef SIMPLE_GC_H
#define SIMPLE_GC_H

#include <stddef.h>

/**
 * Simple Garbage Collector
 *
 * A basic mark-and-sweep garbage collector implementation.
 */

/* Version information */
#define SIMPLE_GC_VERSION_MAJOR 0
#define SIMPLE_GC_VERSION_MINOR 1
#define SIMPLE_GC_VERSION_PATCH 0

/**
 * Get the version string of the garbage collector
 *
 * @return A string representing the version
 */
const char* simple_gc_version(void);

#endif /* SIMPLE_GC_H */
