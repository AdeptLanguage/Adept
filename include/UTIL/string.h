
#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"

// ---------------- strclone ----------------
// Clones a string, producing a duplicate
strong_cstr_t strclone(const char *src);

// ---------------- strsclone ----------------
// Clones a strong string list
strong_cstr_t *strsclone(strong_cstr_t *list, length_t length);

// ---------------- free_strings ----------------
// Frees every string in a list, and then the list
void free_strings(strong_cstr_t *list, length_t length);

// ---------------- strong_cstr_empty_if_null ----------------
// Will heap-allocate an empty string in place of 'string'
// if 'string' is NULL
strong_cstr_t strong_cstr_empty_if_null(strong_cstr_t string);

// ---------------- string_to_escaped_string ----------------
// Escapes the contents of a modern string so that
// special characters such as \n are transfromed into \\n
// and surrounds the string with 'escaped_quote' if not '\0'.
// NOTE: 'escaped_quote' may be 0x00 to signify no surrounding quote character
strong_cstr_t string_to_escaped_string(char *array, length_t length, char escaped_quote);

// ---------------- string_needs_escaping ----------------
// (insight API only)
// Returns whether a string contains characters that need escaping
#ifdef ADEPT_INSIGHT_BUILD
bool string_needs_escaping(weak_cstr_t string, char escaped_quote);
#endif // ADEPT_INSIGHT_BUILD

// ---------------- string_count_character ----------------
// Returns the number of occurrences of 'character' in
// potentially unterminated string of characters
length_t string_count_character(char string[], length_t length, char character);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_STRING_H */
