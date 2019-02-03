
#ifndef BRIDGE_H
#define BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= bridge.h =================================
    Module for bridging the gap between Abstract Syntax Trees and
    Intermediate Representation. Used during the inference and assembly stages.
    ----------------------------------------------------------------------------
*/

#include "AST/ast_type.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "IR/ir_type.h"
#endif

#define BRIDGE_VAR_UNDEF        TRAIT_1 // Variable is to be uninitialized
#define BRIDGE_VAR_REFERENCE    TRAIT_2 // Variable is to be treated as a mutable reference
#define BRIDGE_VAR_POD          TRAIT_3 // Variable is to be treated as plain old data

typedef struct {
    weak_cstr_t name;     // name of the variable
    ast_type_t *ast_type; // AST type of the variable

    #ifndef ADEPT_INSIGHT_BUILD
    ir_type_t *ir_type;   // IR type of the variable
    #endif

    length_t id;          // ID of the variable within the function stack
    trait_t traits;       // traits of the variable
} bridge_var_t;

// ---------------- bridge_var_list_t ----------------
// A list for tracking variables within a scope
typedef struct {
    // Actual names, ast_types and ir_types aren't owned
    bridge_var_t *variables;
    length_t length;
    length_t capacity;
} bridge_var_list_t;

// ---------------- bridge_var_scope_t ----------------
// A variable scope that contains a list of variables
// within the scope as well as a reference to the
// parent and children scopes.
typedef struct bridge_var_scope_t {
    struct bridge_var_scope_t *parent;
    bridge_var_list_t list;

    // First variable id contained within this scope
    // or the child scope. (Used for finding by id)
    length_t first_var_id;

    // Last variable id contained within this scope
    // or the child scopes. (Used for finding by id)
    length_t following_var_id;

    struct bridge_var_scope_t **children;
    length_t children_length;
    length_t children_capacity;
} bridge_var_scope_t;

// ---------------- brige_var_scope_init ----------------
// Initializes a bridge variable scope
void bridge_var_scope_init(bridge_var_scope_t *out_scope, bridge_var_scope_t *parent);

// ---------------- bridge_var_scope_free ----------------
// Frees a bridge variable scope
void bridge_var_scope_free(bridge_var_scope_t *scope);

// ---------------- bridge_var_scope_find_var ----------------
// Finds a variable within a bridge variable scope
bridge_var_t* bridge_var_scope_find_var(bridge_var_scope_t *scope, const char *name);

// ---------------- bridge_var_scope_find_var_by_id ----------------
// Finds a variable within a bridge variable scope by id
bridge_var_t *bridge_var_scope_find_var_by_id(bridge_var_scope_t *scope, length_t id);

// ---------------- bridge_var_scope_already_in_list ----------------
// Checks to see if a variable with that name was already declared
// within the variable list of the given scope.
// NOTE: THIS DOESN'T CHCEK PARENT SCOPES, ONLY THE SCOPE GIVEN IS CHECKED
bool bridge_var_scope_already_in_list(bridge_var_scope_t *scope, const char *name);

// ---------------- bridge_var_scope_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the scope.
// (NOTE: Minimum distance of 3 to count as near enough)
const char* bridge_var_scope_nearest(bridge_var_scope_t *scope, const char *name);

// ---------------- bridge_var_scope_nearest_inner ----------------
// Inner recursive implementation of bridge_var_scope_nearest
void bridge_var_scope_nearest_inner(bridge_var_scope_t *scope, const char *name, char **out_nearest_name, int *out_distance);

// ---------------- bridge_var_list_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the bridge variable list.
// (NOTE: Minimum distance of 3 to count as near enough)
void bridge_var_list_nearest(bridge_var_list_t *list, const char *name, char **out_nearest_name, int *out_distance);

#ifdef __cplusplus
}
#endif

#endif // BRIDGE_H
