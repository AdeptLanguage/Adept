
#ifndef _ISAAC_STRING_LIST_H
#define _ISAAC_STRING_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== string_list.h ==============================
    String list definitions
    ---------------------------------------------------------------------------
*/

#include "UTIL/list.h"
#include "UTIL/ground.h"
#include "UTIL/hash.h"

// ---------------- strong_cstr_list_t ----------------
typedef listof(strong_cstr_t, items) strong_cstr_list_t;

// ---------------- weak_cstr_list_t ----------------
typedef listof(weak_cstr_t, items) weak_cstr_list_t;

// ---------------- cstr_list_append ----------------
#define cstr_list_append(LIST, VALUE) list_append((LIST), (VALUE), char*)
#define strong_cstr_list_append(LIST, VALUE) cstr_list_append((LIST), (VALUE))

// ---------------- strong_cstr_list_free ----------------
void strong_cstr_list_free(strong_cstr_list_t *list);

// ---------------- strong_cstr_list_clone ----------------
strong_cstr_list_t strong_cstr_list_clone(const strong_cstr_list_t *list);

// ---------------- strong_cstr_list_bsearch ----------------
maybe_index_t strong_cstr_list_bsearch(const strong_cstr_list_t *list, const char *item);

// ---------------- strong_cstr_list_sort ----------------
void strong_cstr_list_sort(strong_cstr_list_t *list);

// ---------------- strong_cstr_list_presorted_remove_duplicates ----------------
void strong_cstr_list_presorted_remove_duplicates(strong_cstr_list_t *list);

// ---------------- strong_cstr_list_presorted_has_duplicates ----------------
const char *strong_cstr_list_presorted_has_duplicates(strong_cstr_list_t *list);

// ---------------- strong_cstr_list_equals ----------------
bool strong_cstr_list_equals(strong_cstr_list_t *a, strong_cstr_list_t *b);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_STRING_LIST_H
