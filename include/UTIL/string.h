
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"

// ---------------- strclone ----------------
// Clones a string, producing a duplicate
inline strong_cstr_t strclone(const char *src){
    length_t size = strlen(src) + 1;
    return memcpy(malloc(size), src, size);
}

// ---------------- strsclone ----------------
// Clones a strong string list
strong_cstr_t *strsclone(strong_cstr_t *list, length_t length);

// ---------------- free_strings ----------------
// Frees every string in a list, and then the list
void free_strings(strong_cstr_t *list, length_t length);

// ---------------- strong_cstr_empty_if_null ----------------
// Will heap-allocate an empty string in place of 'string'
// if 'string' is NULL
inline strong_cstr_t strong_cstr_empty_if_null(maybe_null_strong_cstr_t string){
    return string ? string : strclone("");
}

// ---------------- string_to_escaped_string ----------------
// Escapes the contents of a modern string so that
// special characters such as \n are transformed into \\n
// and surrounds the string with 'escaped_quote' if not '\0'.
// NOTE: 'escaped_quote' may be 0x00 to signify no quote character
// NOTE: 'surround' may be false to signify no surrounding quote
strong_cstr_t string_to_escaped_string(const char *array, length_t length, char escaped_quote, bool surround);

// ---------------- string_to_unescaped_string ----------------
// Unescapes the contents of a modern string so that
// special escape sequences such as \\n are transformed into \n
typedef struct { length_t relative_position; } string_unescape_error_t;
strong_cstr_t string_to_unescaped_string(const char *data, length_t length, length_t *out_length, string_unescape_error_t *out_error_cause);

// ---------------- string_needs_escaping ----------------
// (insight API only)
// Returns whether a string contains characters that need escaping
#ifdef ADEPT_INSIGHT_BUILD
bool string_needs_escaping(weak_cstr_t string, char escaped_quote);
#endif // ADEPT_INSIGHT_BUILD

// ---------------- string_count_character ----------------
// Returns the number of occurrences of 'character' in
// potentially unterminated string of characters
length_t string_count_character(const char *string, length_t length, char character);

// ---------------- string_starts_with ----------------
// Returns whether a string starts with another string
bool string_starts_with(weak_cstr_t original, weak_cstr_t stub);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_STRING_H */
