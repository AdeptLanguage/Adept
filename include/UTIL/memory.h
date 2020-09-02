
#ifndef _ISAAC_MEMORY_H
#define _ISAAC_MEMORY_H

#ifndef ADEPT_INSIGHT_BUILD

/*
    ================================= memory.h =================================
    Module for memory analysis

    NOTE: Allows for easy heap memory tracking. Used for testing for memory leaks etc.
    ----------------------------------------------------------------------------
*/

// -------------------------------------------------
// #define TRACK_MEMORY_USAGE // Track memory usage?
// -------------------------------------------------

// Always enable memory tracking for debug builds
#ifdef ENABLE_DEBUG_FEATURES
#define TRACK_MEMORY_USAGE
#endif // ENABLE_DEBUG_FEATURES

// ================= OPTIONS =================
// Track memory allocation file and line numbers?
#define TRACK_MEMORY_FILE_AND_LINE
// Stay quiet unless a memory leak is found?
#define TRACK_MEMORY_QUIET_UNLESS_LEAK
// -------------------------------------------

#include <stdlib.h>
#include <stdbool.h>

#ifdef TRACK_MEMORY_USAGE // Special stuff if we are tracking heap allocations

// ---------------- memblock_t ----------------
// A block of memory allocated
typedef struct {
    void *pointer;
    size_t size;
    bool freed;

    #ifdef TRACK_MEMORY_FILE_AND_LINE
    const char *filename;
    int line_number;
    #endif // TRACK_MEMORY_FILE_AND_LINE

} memblock_t;

// ---------------- global_memory_size ----------------
// Total number of bytes currently allocated
extern size_t global_memory_size;

// ---------------- global_memblocks ----------------
// Array of memory allocations
extern memblock_t *global_memblocks;

// ---------------- global_memblocks_length ----------------
// Length of the 'global_memblocks' array
extern size_t global_memblocks_length;

// ---------------- global_memblocks_capacity ----------------
// Capacity of the 'global_memblocks' array
extern size_t global_memblocks_capacity;

// ---------------- global_memblocks_sorted ----------------
// Whether or not the memory allocation list has been sorted
extern bool global_memblocks_sorted;

// ---------------- memory_init ----------------
// Initializes memory tracking
void memory_init();

// ---------------- memory_track_external_allocation ----------------
// Tracks allocations not made by memory_alloc
void memory_track_external_allocation(void *pointer, size_t size, const char *optional_filename, size_t optional_line);

// ---------------- memory_scan ----------------
// Scans memory allocations for unfreed memory
void memory_scan();

// ---------------- memory_sort ----------------
// Sorts memory allocation blocks (for faster freeing)
void memory_sort();

#ifdef TRACK_MEMORY_FILE_AND_LINE

#define malloc(a) memory_alloc(a, __FILE__, __LINE__)
void* memory_alloc(size_t size, const char *file, int line);
#define free(a) memory_free(a, __FILE__, __LINE__)
void memory_free(void* data, const char *file, int line);
void memory_free_fast(void* data, const char *file, int line);

#else

#define malloc(a) memory_alloc(a)
void* memory_alloc(size_t size);
#define free(a) memory_free(a)
void memory_free(void* data);
void memory_free_fast(void* data);

#endif // TRACK_MEMORY_FILE_AND_LINE

#define calloc(amount, size) memset(malloc(amount * size), 0, amount * size)

#endif // TRACK_MEMORY_USAGE

#endif // ADEPT_INSIGHT_BUILD

#endif // _ISAAC_MEMORY_H
