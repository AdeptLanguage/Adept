
#ifndef _ISAAC_IR_GEN_VTREE_H
#define _ISAAC_IR_GEN_VTREE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_gen_vtree.h ==============================
    Module used to generate virtual dispatch trees
    ----------------------------------------------------------------------------
*/

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IRGEN/ir_vtree.h"
#include "UTIL/func_pair.h"
#include "UTIL/ground.h"

// ---------------- virtual_addition_t ----------------
// Represents a delayed virtual method addition into a vtree structure
typedef struct {
    vtree_t *vtree;
    ir_func_endpoint_t endpoint;
} virtual_addition_t;

// ---------------- virtual_addition_list_t ----------------
// A list of delayed virtual method additions
typedef listof(virtual_addition_t, additions) virtual_addition_list_t;

// ---------------- virtual_addition_list_append ----------------
// Appends a 'virtual_addition_t' value to a 'virtual_addition_list_t'
#define virtual_addition_list_append(LIST, VALUE) list_append((LIST), (VALUE), virtual_addition_t);

// ---------------- virtual_addition_list_free ----------------
// Fully frees a 'virtual_addition_list_t'
#define virtual_addition_list_free(LIST) free((LIST)->additions);

// ---------------- ir_gen_vtree_overrides ----------------
// Searches for, generates, and fills in any overrides for a child vtree.
// After that, will fill in default implementation for any of own virtual methods.
// Will recursively call itself to handle children of child.
errorcode_t ir_gen_vtree_overrides(
    compiler_t *compiler,
    object_t *object,
    vtree_t *start,
    int depth_left
);

// ---------------- ir_gen_vtree_search_for_single_override ----------------
// Searches for a suitable method to override the method 'ast_func_id' with.
// Search output will be stored in 'out_result' if successful.
// Returns SUCCESS even when no suitable override was found.
// Returns FAILURE if a serious error occurred
errorcode_t ir_gen_vtree_search_for_single_override(
    compiler_t *compiler,
    object_t *object,
    const ast_type_t *child_subject_type,
    func_id_t ast_func_id,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_vtree_link_up_nodes ----------------
// Fills in 'parent' pointer field for vtrees after/including 'starting_index'
// in 'vtree_list' using existing vtrees when possible or appending new vtrees when parents aren't
// the in list yet
errorcode_t ir_gen_vtree_link_up_nodes(
    compiler_t *compiler,
    object_t *object,
    vtree_list_t *vtree_list,
    length_t starting_index
);

// ---------------- ir_gen_vtree_inject_addition_for_descendants ----------------
// TL;DR - Handles a virtual method addition for descendants of a vtree
// 
// Will search for a suitable override for the supplied virtual method addition
// for each child of the given vtree.
// Will then inject override into table of child at 'insertion_point' if exists
// otherwise default implementation 'addition.endpoint'.
// Will recursively apply to children of children.
// NOTE: If a child is found to have a suitable override,
// then their descendant's default implementation will be that override.
errorcode_t ir_gen_vtree_inject_addition_for_descendants(
    compiler_t *compiler,
    object_t *object,
    virtual_addition_t addition,
    length_t insertion_point,
    vtree_t *vtree
);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_VTREE_H
