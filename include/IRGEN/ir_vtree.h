
#ifndef _ISAAC_IR_VTREE_H
#define _ISAAC_IR_VTREE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ ir_vtree.h ================================
    Module for trees used to generate virtual dispatch tables
    ----------------------------------------------------------------------------
*/

#include "AST/ast_type_lean.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_value.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"

struct vtree;

// ---------------- vtree_list_t ----------------
// List of pointers to heap allocated vtree_t objects.
// Objects inside the list maybe point to others in the list,
// even between additions, since they are all separately heap allocated.
typedef listof(struct vtree*, vtrees) vtree_list_t;

// ---------------- vtree_t ----------------
// Tree used to help generate virtual dispatch tables
typedef struct vtree {
    struct vtree *parent;
    ast_type_t signature;
    ir_func_endpoint_list_t virtuals;
    ir_func_endpoint_list_t table;
    vtree_list_t children;
    ir_value_t *finalized_table;
} vtree_t;


// ---------------- vtree_free_fully ----------------
// Frees a vtree, including the heap-allocated memory pointed to by 'vtree'
void vtree_free_fully(vtree_t *vtree);

// ---------------- vtree_append_virtual ----------------
// Appends a virtual endpoint to a vtree node
void vtree_append_virtual(vtree_t *vtree, ir_func_endpoint_t endpoint);

// ---------------- vtree_free_list ----------------
// Fully frees a vtree_list_t, including heap-allocated elements and the array itself
void vtree_list_free(vtree_list_t *vtree_list);

// ---------------- vtree_free_list ----------------
// Appends a heap allocated vtree_t to a vtree_list_t
#define vtree_list_append(LIST, VALUE) list_append((LIST), (VALUE), vtree_t*)

// ---------------- vtree_list_find_or_append ----------------
// Finds a vtree in a vtree list that has the given signature type,
// If none exists, a new vtree will be created and inserted.
// Will always return a vtree with a matching signature.
vtree_t *vtree_list_find_or_append(vtree_list_t *vtree_list, const ast_type_t *signature);

// ---------------- vtree_list_find ----------------
// Finds a vtree in a vtree list that has the given signature
vtree_t *vtree_list_find(vtree_list_t *vtree_list, const ast_type_t *signature);

// ---------------- vtree_print ----------------
// Prints a vtree
void vtree_print(vtree_t *root, length_t indentation);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_VTREE_H
