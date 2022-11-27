
#ifndef _ISAAC_UTIL_H
#define _ISAAC_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================== util.h ==================================
    Module for general memory-related utilities
    ----------------------------------------------------------------------------
*/

#include <stdbool.h>
#include <stdio.h>

#include "UTIL/ground.h"

// ---------------- expand ----------------
// Intelligently expands an array if it won't be able hold new elements
void expand(void **inout_memory, length_t unit_size, length_t length, length_t *inout_capacity,
    length_t amount, length_t default_capacity);

// ---------------- coexpand ----------------
// Intelligently expands two arrays if they won't be able hold new elements
void coexpand(void **inout_memory1, length_t unit_size1, void **inout_memory2, length_t unit_size2,
    length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity);

// ---------------- grow ----------------
// Forces growth of an array to support a certain capacity
inline void grow(void **inout_memory, length_t unit_size, length_t new_length){
    *inout_memory = realloc(*inout_memory, unit_size * new_length);
}

// ---------------- mallocandsprintf ----------------
// NOTE: Only supports using '%s', '%d', and '%%'
// Allocates enough memory to hold the result
// of a sprintf() and then runs sprintf() and
// returns the newly-allocated null-terminated string
strong_cstr_t mallocandsprintf(const char *format, ...);

// ---------------- find_insert_position ----------------
// Finds the position to insert an object into an object list
length_t find_insert_position(const void *items, length_t length, int (*compare)(const void*, const void*), const void *object, length_t size);

// ---------------- file_exists ----------------
// Returns whether a file exits
bool file_exists(weak_cstr_t filename);

// ---------------- file_text_contents ----------------
// Reads text contents of a file.
// When successful, 'out_contents' will be a newly allocated
// C-String, with 'out_length' being the number of characters.
// If 'append_newline', a newline will be added onto the end.
// Returns whether successful
bool file_text_contents(weak_cstr_t filename, strong_cstr_t *out_contents, length_t *out_length, bool append_newline);

// ---------------- file_binary_contents ----------------
// Reads binary contents of a file.
// When successful, 'out_contents' will be a newly allocated
// C-String, with 'out_length' being the number of bytes
// Returns whether successful
bool file_binary_contents(weak_cstr_t filename, strong_cstr_t *out_contents, length_t *out_length);

// ---------------- file_copy ----------------
// Copies a file from one place to another
// Returns FAILURE if unable to copy
errorcode_t file_copy(weak_cstr_t src_filename, weak_cstr_t dst_filename);

// ---------------- indent ----------------
// Writes 4-spaces 'indentation_level' times to a stream
void indent(FILE *file, length_t indentation_level);

// ---------------- length_min ----------------
// Returns the smaller of two length values
static inline length_t length_min(length_t a, length_t b){
    return a <= b ? a : b;
}

// ---------------- length_max ----------------
// Returns the larger of two length values
static inline length_t length_max(length_t a, length_t b){
    return a > b ? a : b;
}

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_UTIL_H
