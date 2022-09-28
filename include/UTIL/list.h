
#ifndef _ISAAC_LIST_H
#define _ISAAC_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================== list.h ==================================
    Module for abstract lists
    ----------------------------------------------------------------------------
*/

#include <stdlib.h>

#include "UTIL/ground.h"

// ---------------- listof ----------------
// Generic list struct
// Usage:
// > typedef listof(ItemType, items_field_name) ItemTypeList;
#define listof(TYPE, FIELD_NAME) struct { TYPE *FIELD_NAME; length_t length; length_t capacity; }

// ---------------- list_append----------------
// Appends an item to a list.
// How to derive for specific list:
// > #define ast_cases_append(LIST, VALUE) list_append((LIST), (VALUE), ast_case_t)
#define list_append(LIST, VALUE, ELEMENT_TYPE) *list_append_new((LIST), ELEMENT_TYPE) = VALUE

// ---------------- list_append_new ----------------
// Returns a pointer to a new item.
// How to derive for specific list:
// > #define ast_cases_append_new(LIST) list_append_new(LIST, ast_case_t)
#define list_append_new(LIST, ELEMENT_TYPE) ((ELEMENT_TYPE*) list_append_new_impl((LIST), sizeof(ELEMENT_TYPE)))

// ---------------- list_append_new_impl ----------------
// Returns a pointer to a new item
void *list_append_new_impl(void *list_struct, length_t sizeof_element);

// ---------------- list_qsort ----------------
// Sort a list
void list_qsort(void *list_struct, length_t sizeof_element, int (*cmp)(const void*, const void*));

// ---------------- list_create ----------------
// Creates a list
#define list_create(LIST_TYPE, FIELD_TYPE, CAPACITY) ((LIST_TYPE){malloc(sizeof(FIELD_TYPE) * (CAPACITY)), 0, (CAPACITY)})

// ---------------- list_last_unchecked ----------------
// Returns a temporary pointer to the last item in a list
// If no such element exists, the behavior is undefined
#define list_last_unchecked(LIST, FIELD_TYPE) *((FIELD_TYPE*) list_last_unchecked_ptr_impl((LIST), sizeof(FIELD_TYPE)))
void *list_last_unchecked_ptr_impl(void *list_struct, length_t sizeof_element);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_LIST_H
