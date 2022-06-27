
#ifndef _ISAAC_GROUND_H
#define _ISAAC_GROUND_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= ground.h =================================
    Module containing basic #includes and type definitions
    ----------------------------------------------------------------------------
*/

// Release information
#define ADEPT_VERSION_STRING    "2.7"
#define ADEPT_VERSION_NUMBER    2.7
#define ADEPT_VERSION_MAJOR     2
#define ADEPT_VERSION_MINOR     7
#define ADEPT_VERSION_RELEASE   0
#define ADEPT_VERSION_QUALIFIER ""

// Previous stable version string
// (only used by preview builds to ignore "new update" false positives)
#define ADEPT_PREVIOUS_STABLE_VERSION_STRING "2.6"

#define ADEPT_PACKAGE_MANAGER_SPEC_VERSION 1

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>

#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#include "UTIL/memory.h"

#ifdef ADEPT_INSIGHT_BUILD
#include "UTIL/__insight_overloads.h"
#endif

// ---------------- errorcode_t ----------------
#define SUCCESS     0
#define FAILURE     1
#define ALT_FAILURE 2
typedef int errorcode_t;

// ---------------- successful_t ----------------
#define SUCCESSFUL   true
#define UNSUCCESSFUL false
typedef bool successful_t;

// ---------------- strong_cstr_t ----------------
typedef char *strong_cstr_t;            // Owned C-String
typedef char *maybe_null_strong_cstr_t; // Maybe NULL Variant

// ---------------- weak_cstr_t ----------------
typedef char *weak_cstr_t;            // Unowned C-String
typedef char *maybe_null_weak_cstr_t; // Maybe NULL Variant

// ---------------- length_t ----------------
typedef size_t length_t;

// ---------------- source_t ----------------
typedef struct {
    length_t index;
    length_t stride;
    length_t object_index;
} source_t;

#define NULL_SOURCE (source_t){0, 0, 0}

// ---------------- index_id_t ----------------
// Used as a general-purpose ID
#define MAX_INDEX_ID     0xFFFFFFFE
#define INVALID_INDEX_ID 0xFFFFFFFF
typedef uint32_t index_id_t;

// ---------------- func_id_t ----------------
// Used as an ID to refer to functions
#define MAX_FUNC_ID     MAX_INDEX_ID
#define INVALID_FUNC_ID INVALID_INDEX_ID
typedef index_id_t func_id_t;

// ---------------- maybe_index_t ----------------
typedef long long maybe_index_t;

// ---------------- troolean ----------------
// 3-state value
typedef unsigned char troolean;

#define TROOLEAN_TRUE 1
#define TROOLEAN_FALSE 0
#define TROOLEAN_UNKNOWN 2

// ---------------- optional ----------------
#define optional(TYPE) struct { bool has; TYPE value; }

// ---------------- lenstr_t and friends ----------------
// C-String with cached length
typedef struct {
    char *cstr;
    length_t length;
} lenstr_t, strong_lenstr_t, weak_lenstr_t;

inline bool lenstreq(lenstr_t a, lenstr_t b){
    return a.length == b.length && memcmp(a.cstr, b.cstr, a.length) == 0;
}

inline int lenstrcmp(lenstr_t a, lenstr_t b){
    if(a.length == b.length){
        return memcmp(a.cstr, b.cstr, a.length);
    } else {
        return (a.length < b.length) ? 1 : -1;
    }
}

inline lenstr_t cstr_to_lenstr(char *cstr){
    return (lenstr_t){ .cstr = cstr, .length = strlen(cstr) };
}

// ---------------- SOURCE_IS_NULL ----------------
// Whether or not a 'source_t' is NULL_SOURCE
#define SOURCE_IS_NULL(_src) (_src.index == 0 && _src.object_index == 0 && _src.stride == 0)

// ---------------- typecast ----------------
// Syntactic sugar for when traditional (TYPE) VALUE
// casting is confusing.
// e.g.    ast_expr_str(((ast_expr_math_t*) expr)->a)
// becomes ast_expr_str(typecast(ast_expr_math_t*, expr)->a)
#define typecast(TYPE, VALUE) ((TYPE)(VALUE))

// ---------------- streq ----------------
// Returns whether two null-termianted strings are equal
// Equivalent to 'strcmp(STRING, VALUE) == 0)'
#define streq(STRING, VALUE) (strcmp((STRING), (VALUE)) == 0)

// ---------------- bsearch_insertion ----------------
// Like bsearch, except will return the last checked pivot (or NULL)
void *bsearch_insertion(const void *key, const void *base, size_t num, size_t size, int (*cmp)(const void *, const void*));

// ---------------- NUM_ITEMS ----------------
#define NUM_ITEMS(array) (sizeof (array) / sizeof *(array))

// ---------------- special characters ----------------
#ifdef __APPLE__
#define USE_UTF8_INSTEAD_OF_EXTENDED_ASCII_FOR_SPECIAL_CHARACTERS
#endif

#ifdef USE_UTF8_INSTEAD_OF_EXTENDED_ASCII_FOR_SPECIAL_CHARACTERS
#define BOX_DRAWING_UP_RIGHT "\xE2\x94\x94"
#else
#define BOX_DRAWING_UP_RIGHT "\xC0"
#endif // USE_UTF8_INSTEAD_OF_EXTENDED_ASCII_FOR_SPECIAL_CHARACTERS

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_GROUND_H
