
#ifndef AST_VAR_TREE_H
#define AST_VAR_TREE_H

/*
    ============================= ast_var.h =============================
    Module for managing abstract syntax tree variables
    ---------------------------------------------------------------------------
*/

#include "ast_type.h"

// ---------------- ast_var_list_t ----------------
// A list containing all the variables declared
// within a variable scope
typedef struct {
    // Each name and each type are references to existing values.
    // The variable list does not have ownership of them.
    const char **names;
    ast_type_t **types;
    length_t length;
    length_t capacity;
} ast_var_list_t;

// ---------------- ast_var_scope_t ----------------
// A variable scope that contains a list of variables
// within the scope as well as a reference to the
// parent and children scopes.
typedef struct ast_var_scope_t {
    struct ast_var_scope_t *parent;
    ast_var_list_t variables;

    struct ast_var_scope_t **children;
    length_t children_length;
    length_t children_capacity;
} ast_var_scope_t;

// ---------------- ast_var_scope_init ----------------
// Initializes a variable scope
void ast_var_scope_init(ast_var_scope_t *out_var_scope, ast_var_scope_t *parent);

// ---------------- ast_var_scope_free ----------------
// Free a variable scope and all of its children
void ast_var_scope_free(ast_var_scope_t *scope);

// ---------------- ast_var_list_add_variables ----------------
// Adds variables into an ast_var_list_t
void ast_var_list_add_variables(ast_var_list_t *list, const char **names, ast_type_t *type, length_t length);

// ---------------- ast_var_list_nearest----------------
// Returns the variable in the list with the name closest
// to the name supplied. If none are found within
// a reasonable amount, NULL is returned.
const char* ast_var_list_nearest(ast_var_list_t *var_list, char* name);

#endif // AST_VAR_TREE_H
