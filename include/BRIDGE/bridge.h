
#ifndef _ISAAC_BRIDGE_H
#define _ISAAC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= bridge.h =================================
    Module for bridging the gap between Abstract Syntax Trees and
    Intermediate Representation. Used during the inference and assembly stages.
    ----------------------------------------------------------------------------
*/

#include <stdbool.h>

#include "AST/ast_type_lean.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#endif

#define BRIDGE_VAR_UNDEF        TRAIT_1 // Variable is to be uninitialized
#define BRIDGE_VAR_REFERENCE    TRAIT_2 // Variable is to be treated as a mutable reference
#define BRIDGE_VAR_POD          TRAIT_3 // Variable is to be treated as plain old data
#define BRIDGE_VAR_STATIC       TRAIT_4 // Variable is to static (global-like)

typedef struct {
    weak_cstr_t name;
    ast_type_t *ast_type;

    index_id_t id;           // ID of the variable within the function stack (only applies to non-static variables)
    index_id_t static_id;    // ID of the variable as a static variable (only applies to static variables)
    trait_t traits;
    source_t source;

    #ifndef ADEPT_INSIGHT_BUILD
    ir_type_t *ir_type;
    ir_value_t *optional_anon_global;
    #endif
} bridge_var_t;

// ---------------- bridge_var_list_t ----------------
// A list for tracking variables within a scope
// Actual names, ast_types and ir_types aren't owned
typedef listof(bridge_var_t, variables) bridge_var_list_t;
#define bridge_var_list_append(LIST, VALUE) list_append((LIST), (VALUE), bridge_var_t)

// ---------------- bridge_scope_ref_list_t ----------------
// A list of pointers to bridge scopes
typedef listof(struct bridge_scope_t*, scopes) bridge_scope_ref_list_t;
#define bridge_scope_ref_list_append(LIST, VALUE) list_append((LIST), (VALUE), struct bridge_scope_t*)

// ---------------- bridge_scope_t ----------------
// A variable scope that contains a list of variables
// within the scope as well as a reference to the
// parent and children scopes.
typedef struct bridge_scope_t {
    struct bridge_scope_t *parent;
    bridge_var_list_t list;

    // First variable id contained within this scope
    // or the child scope. (Used for finding by id)
    length_t first_var_id;

    // Last variable id contained within this scope
    // or the child scopes. (Used for finding by id)
    length_t following_var_id;

    bridge_scope_ref_list_t children;
} bridge_scope_t;

// ---------------- bridge_scope_init ----------------
// Initializes a bridge scope
void bridge_scope_init(bridge_scope_t *out_scope, bridge_scope_t *parent);

// ---------------- bridge_scope_free ----------------
// Frees a bridge scope
void bridge_scope_free(bridge_scope_t *scope);

// ---------------- bridge_scope_find_var ----------------
// Finds a variable within a bridge variable scope
bridge_var_t* bridge_scope_find_var(bridge_scope_t *scope, const char *name);

// ---------------- bridge_scope_find_var_by_id ----------------
// Finds a variable within a bridge variable scope by id
bridge_var_t *bridge_scope_find_var_by_id(bridge_scope_t *scope, length_t id);

// ---------------- bridge_scope_var_already_in_list ----------------
// Checks to see if a variable with that name was already declared
// within the variable list of the given scope.
// NOTE: THIS DOESN'T CHECK PARENT SCOPES, ONLY THE SCOPE GIVEN IS CHECKED
bool bridge_scope_var_already_in_list(bridge_scope_t *scope, const char *name);

// ---------------- bridge_scope_var_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the scope.
// (NOTE: Minimum distance of 3 to count as near enough)
const char* bridge_scope_var_nearest(bridge_scope_t *scope, const char *name);

// ---------------- bridge_var_list_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the bridge variable list.
// (NOTE: Minimum distance of 3 to count as near enough)
void bridge_var_list_nearest(bridge_var_list_t *list, const char *name, char **out_nearest_name, int *out_distance);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_BRIDGE_H
