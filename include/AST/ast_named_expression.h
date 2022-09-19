
#ifndef _ISAAC_AST_NAMED_EXPRESSION_H
#define _ISAAC_AST_NAMED_EXPRESSION_H

/*
    ============================= ast_named_expression.h ==============================
    Module for abstract syntax tree named expressions
    ---------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"
#include "AST/ast_expr_lean.h"

// ---------------- ast_named_expression_t ----------------
// A named expression
typedef struct {
    strong_cstr_t name;
    ast_expr_t *expression;
    trait_t traits;
    source_t source;
} ast_named_expression_t;

// ---------------- ast_named_expression_create ----------------
// Creates a named expression
ast_named_expression_t ast_named_expression_create(strong_cstr_t name, ast_expr_t *value, trait_t traits, source_t source);

// ---------------- ast_named_expression_free ----------------
// Frees a named expression
void ast_named_expression_free(ast_named_expression_t *named_expression);

// ---------------- ast_named_expression_clone ----------------
// Creates a clone of a named expression
ast_named_expression_t ast_named_expression_clone(ast_named_expression_t *original);

// ---

// ---------------- ast_named_expression_list_t ----------------
// A list of named expressions
typedef listof(ast_named_expression_t, expressions) ast_named_expression_list_t;
#define ast_named_expression_list_append(LIST, VALUE) list_append((LIST), (VALUE), ast_named_expression_t)

// ---------------- ast_named_expression_list_free ----------------
// Frees a list of named expressions
void ast_named_expression_list_free(ast_named_expression_list_t *list);

// ---------------- ast_named_expression_list_find ----------------
// Returns a temporary pointer to the `ast_named_expression_t` that has the given name
// Returns NULL if no such element exists
ast_named_expression_t *ast_named_expression_list_find(ast_named_expression_list_t *sorted_list, const char *name);

// ---------------- ast_named_expression_list_find_index ----------------
// Returns the index to the `ast_named_expression_t` that has the given name
// Returns -1 if no such element exists
maybe_index_t ast_named_expression_list_find_index(ast_named_expression_list_t *sorted_list, const char *name);

// ---------------- ast_named_expression_list_insert_sorted ----------------
// Inserts a new `ast_named_expression_t` into a sorted `ast_named_expression_list_t`
void ast_named_expression_list_insert_sorted(ast_named_expression_list_t *sorted_list, ast_named_expression_t named_expression);

// ---------------- ast_named_expression_list_sort ----------------
// Sorts a list of AST named expressions
void ast_named_expression_list_sort(ast_named_expression_list_t *list);

// ---

// ---------------- ast_named_expressions_cmp ----------------
// Compares two 'ast_named_expression_t' structures.
// Used for qsort()
int ast_named_expressions_cmp(const void *a, const void *b);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_NAMED_EXPRESSION_H
