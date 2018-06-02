
#ifndef INFERENCE_H
#define INFERENCE_H

/*
    ================================ inference.h ===============================
    Module for infering generic expressions and resolving type aliases within
    abstract syntax trees
    ----------------------------------------------------------------------------
*/

#include "ground.h"
#include "object.h"
#include "compiler.h"

// ---------------- undetermined_type_list_t ----------------
// A data structure for containing expressions that have the
// possiblity of being determined by a non-generic expression
// (used on the local expression level)
typedef struct {
    ast_expr_t **expressions;
    length_t expressions_length;
    length_t expressions_capacity;
    unsigned int determined;
    bool prevent_more_generics;
} undetermined_type_list_t;

// ---------------- infer ----------------
// Entry point to inference module
int infer(compiler_t *compiler, object_t *object);

// ---------------- infer_in_funcs ----------------
// Infers aliases and generics in a list of functions
int infer_in_funcs(compiler_t *compiler, object_t *object, ast_func_t *funcs, length_t funcs_length);

// ---------------- infer_in_stmts ----------------
// Infers aliases and generics in a list of statements
int infer_in_stmts(compiler_t *compiler, object_t *object, ast_func_t *func, ast_expr_t **statements, length_t statements_length);

// ---------------- infer_expr ----------------
// Infers an expression from the root of it
int infer_expr(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type);

// ---------------- infer_expr_inner ----------------
// Infers an inner expression within the root
int infer_expr_inner(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **expr, undetermined_type_list_t *undetermined);

// ---------------- determine_undetermined ----------------
// Determines generics for an 'undetermined_type_list_t'
int determine_undetermined(compiler_t *compiler, object_t *object, undetermined_type_list_t *undetermined, unsigned int assigned_type);

// ---------------- resolve_generics ----------------
// Attempts to convert a list of generic expressions
// to a primitive of the given expression ID
int resolve_generics(compiler_t *compiler, object_t *object, ast_expr_t **expressions, length_t expressions_length, unsigned int assigned_type);

// ---------------- generics_primitive_type ----------------
// Generates a suitable primitive expression ID for the
// list of generic expressions. Used when the root expression
// is composed of entirely generic values.
unsigned int generics_primitive_type(ast_expr_t **expressions, length_t expressions_length);

// ---------------- ast_primitive_from_ast_type ----------------
// Attempts to convert an AST type to a suitable primitive
// expression ID
unsigned int ast_primitive_from_ast_type(ast_type_t *type);

// ---------------- infer_type_aliases ----------------
// Infers all the aliases within a type
int infer_type_aliases(compiler_t *compiler, object_t *object, ast_type_t *type);

#endif // INFERENCE_H
