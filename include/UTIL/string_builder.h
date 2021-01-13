
#ifndef _ISAAC_STRING_BUILDER_H
#define _ISAAC_STRING_BUILDER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================ string_builder.h =============================
    Module for building strings from smaller strings
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

typedef struct {
    char *buffer;
    length_t length;
    length_t capacity;
} string_builder_t;

// ---------------- string_builder_init ----------------
// Initializes a string builder
void string_builder_init(string_builder_t *builder);

// ---------------- string_builder_finalize ----------------
// Destroys a string builder and returns the finalized concatenated string
strong_cstr_t string_builder_finalize(string_builder_t *builder);

// ---------------- string_builder_abandon ----------------
// Destroys a string builder without returning the work-in-progress string
void string_builder_abandon(string_builder_t *builder);

// ---------------- string_builder_append ----------------
// Appends a string to the string being built by a string builder
void string_builder_append(string_builder_t *builder, const char *portion);

// ---------------- string_builder_append ----------------
// Appends a string view to the string being built by a string builder
void string_builder_append_view(string_builder_t *builder, const char *portion, length_t portion_length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_STRING_BUILDER_H
