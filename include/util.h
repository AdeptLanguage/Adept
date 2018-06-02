
#ifndef UTIL_H
#define UTIL_H

/*
    ================================== util.h ==================================
    Module for general memory-related utilities
    ----------------------------------------------------------------------------
*/

#include "ground.h"

// ---------------- expand ----------------
// Expands an array if it won't be able hold new elements
void expand(void **inout_memory, length_t unit_size, length_t length, length_t *inout_capacity,
    length_t amount, length_t default_capacity);

// ---------------- coexpand ----------------
// Expands two array if they won't be able hold new elements
void coexpand(void **inout_memory1, length_t unit_size1, void **inout_memory2, length_t unit_size2,
    length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity);

// ---------------- strclone ----------------
// Clones a string, producing a duplicate
char* strclone(const char *src);

#endif // UTIL_H
