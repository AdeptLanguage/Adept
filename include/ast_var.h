
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
// parent scope.
typedef struct ast_var_scope_t {
    struct ast_var_scope_t *parent;
    ast_var_list_t *variables;
} ast_var_scope_t;

void ast_var_list_add_variables(ast_var_list_t *list, const char **names, ast_type_t *type, length_t length);

#endif // AST_VAR_TREE_H
