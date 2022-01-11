
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

// ---------------- string_builder_finalize_with_length ----------------
// Destroys a string builder and returns the finalized concatenated string
// with length information attached
strong_lenstr_t string_builder_finalize_with_length(string_builder_t *builder);

// ---------------- string_builder_abandon ----------------
// Destroys a string builder without returning the work-in-progress string
void string_builder_abandon(string_builder_t *builder);

// ---------------- string_builder_append ----------------
// Appends a string to the string being built by a string builder
void string_builder_append(string_builder_t *builder, const char *portion);

// ---------------- string_builder_append_view ----------------
// Appends a string view to the string being built by a string builder
void string_builder_append_view(string_builder_t *builder, const char *portion, length_t portion_length);

// ---------------- string_builder_append_char ----------------
// Appends a character to the string being built by a string builder
void string_builder_append_char(string_builder_t *builder, char character);

// ---------------- string_builder_append_int ----------------
// Appends an int to the string being built by a string builder
void string_builder_append_int(string_builder_t *builder, int integer);

// ---------------- string_builder_append_quoted ----------------
// Surrounds a string with double quotes and appends it to
// the string being built by a string builder.
// NOTE: Does NOT escape 'portion'
void string_builder_append_quoted(string_builder_t *builder, const char *portion);
void string_builder_append2_quoted(string_builder_t *builder, const char *part_a, const char *part_b);
void string_builder_append3_quoted(string_builder_t *builder, const char *part_a, const char *part_b, const char *part_c);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_STRING_BUILDER_H
