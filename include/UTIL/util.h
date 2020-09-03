
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

#include "UTIL/ground.h"

// -------------------- UTIL_USE_REALLOC --------------------
// Specifies whether or not memory utils should use realloc()
#ifndef TRACK_MEMORY_USAGE
#define UTIL_USE_REALLOC
#endif
// ----------------------------------------------------------

// ---------------- expand ----------------
// Expands an array if it won't be able hold new elements
void expand(void **inout_memory, length_t unit_size, length_t length, length_t *inout_capacity,
    length_t amount, length_t default_capacity);

// ---------------- coexpand ----------------
// Expands two arrays if they won't be able hold new elements
void coexpand(void **inout_memory1, length_t unit_size1, void **inout_memory2, length_t unit_size2,
    length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity);

// ---------------- grow ----------------
// Forces growth of an array to a certain length
#ifdef UTIL_USE_REALLOC
#define grow(inout_memory, unit_size, old_length, new_length) grow_impl(inout_memory, unit_size, new_length)
void grow_impl(void **inout_memory, length_t unit_size, length_t new_length);
#else
#define grow(inout_memory, unit_size, old_length, new_length) grow_impl(inout_memory, unit_size, old_length, new_length)
void grow_impl(void **inout_memory, length_t unit_size, length_t old_length, length_t new_length);
#endif

// ---------------- strclone ----------------
// Clones a string, producing a duplicate
strong_cstr_t strclone(const char *src);

// ---------------- freestrs ----------------
// Frees every string in an array, and then the array
void freestrs(strong_cstr_t *array, length_t length);

// ---------------- mallocandsprintf ----------------
// NOTE: Only supports using '%s' and '%%'
// Allocates an array capable of holding the result
// of a sprintf() and then runs sprintf() and
// returns the byte array
strong_cstr_t mallocandsprintf(const char *format, ...);

// ---------------- string_to_escaped_string ----------------
// Escapes the contents of a modern string so that
// special characters such as \n are transfromed into \\n
// and surrounds the string with double quotes
strong_cstr_t string_to_escaped_string(char *array, length_t length);

// ---------------- string_count_character ----------------
// Returns the number of occurances of 'character' in modern
// string 'string' of 'length'
length_t string_count_character(weak_cstr_t string, length_t length, char character);

// ---------------- find_insert_position ----------------
// Finds the position to insert an object into an object list
length_t find_insert_position(void *array, length_t length, int(*compare)(const void*, const void*), void *object_reference, length_t object_size);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_UTIL_H
