
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

// ---------------- strong_cstr_list_t ----------------
typedef listof(strong_cstr_t, items) strong_cstr_list_t;

// ---------------- weak_cstr_list_t ----------------
typedef listof(weak_cstr_t, items) weak_cstr_list_t;

// ---------------- cstr_list_append ----------------
#define cstr_list_append(LIST, VALUE) list_append((LIST), (VALUE), char*)
#define strong_cstr_list_append(LIST, VALUE) cstr_list_append((LIST), (VALUE))

// ---------------- strong_cstr_list_free ----------------
void strong_cstr_list_free(strong_cstr_list_t *list);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_STRING_LIST_H
