
#ifndef INFER_H
#define INFER_H

/*
    ================================== infer.h =================================
    Module for infering generic expressions and resolving type aliases within
    abstract syntax trees
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"

// ---------------- infer_var_t ----------------
// Variable mapping used for inference stage
typedef struct {
    weak_cstr_t name;
    ast_type_t *type;
} infer_var_t;

// ---------------- infer_var_list_t ----------------
// Variable list used for inference stage
typedef struct {
    infer_var_t *variables;
    length_t length;
    length_t capacity;
} infer_var_list_t;

// ---------------- infer_var_scope_t ----------------
// Variable scope used for inference stage
typedef struct infer_var_scope_t {
    struct infer_var_scope_t *parent;
    infer_var_list_t list;
} infer_var_scope_t;

// ---------------- undetermined_expr_list_t ----------------
// A data structure for containing generic expressions that
// haven't been assigned a type yet.
// NOTE: Used on the local expression level
typedef struct {
    ast_expr_t **expressions;
    length_t expressions_length;
    length_t expressions_capacity;
    unsigned int solution;
} undetermined_expr_list_t;

// ---------------- infer ----------------
// Entry point to inference module
int infer(compiler_t *compiler, object_t *object);

// ---------------- infer_in_funcs ----------------
// Infers aliases and generics in a list of functions
int infer_in_funcs(compiler_t *compiler, object_t *object, ast_func_t *funcs, length_t funcs_length);

// ---------------- infer_in_stmts ----------------
// Infers aliases and generics in a list of statements
int infer_in_stmts(compiler_t *compiler, object_t *object, ast_func_t *func, ast_expr_t **statements, length_t statements_length, infer_var_scope_t *scope);

// ---------------- infer_expr ----------------
// Infers an expression from the root of it
int infer_expr(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type, infer_var_scope_t *scope);

// ---------------- infer_expr_inner ----------------
// Infers an inner expression within the root
int infer_expr_inner(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **expr, undetermined_expr_list_t *undetermined, infer_var_scope_t *scope);

// ---------------- undetermined_expr_list_give ----------------
// Gives a potential solution for an undetermined list
int undetermined_expr_list_give_solution(compiler_t *compiler, object_t *object, undetermined_expr_list_t *undetermined, unsigned int potential_solution_primitive);

// ---------------- undetermined_expr_list_give_generic ----------------
// Resolves the generic if the solution is already known,
// otherwise adds it to the list of undetermined generics
int undetermined_expr_list_give_generic(compiler_t *compiler, object_t *object, undetermined_expr_list_t *undetermined, ast_expr_t **expr);

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

// ---------------- infer_var_scope_init ----------------
// Initializes an inference variable scope
void infer_var_scope_init(infer_var_scope_t *out_scope, infer_var_scope_t *parent);

// ---------------- infer_var_scope_push ----------------
// Pushes a new an inference variable scope
void infer_var_scope_push(infer_var_scope_t **scope);

// ---------------- infer_var_scope_pop ----------------
// Pops an inference variable scope
void infer_var_scope_pop(infer_var_scope_t **scope);

// ---------------- infer_var_scope_free ----------------
// Frees an inference variable scope
void infer_var_scope_free(infer_var_scope_t *scope);

// ---------------- infer_var_scope_find ----------------
// Finds a variable mapping within an inference variable scope
infer_var_t* infer_var_scope_find(infer_var_scope_t *scope, const char *name);

// ---------------- infer_var_scope_add_variable ----------------
// Adds a variables to an inference variable scope
void infer_var_scope_add_variable(infer_var_scope_t *scope, weak_cstr_t name, ast_type_t *type);

// ---------------- infer_var_scope_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the scope.
// (NOTE: Minimum distance of 3 to count as near enough)
const char* infer_var_scope_nearest(infer_var_scope_t *scope, const char *name);

// ---------------- infer_var_scope_nearest_inner ----------------
// Inner recursive implementation of infer_var_scope_nearest
void infer_var_scope_nearest_inner(infer_var_scope_t *scope, const char *name, char **out_nearest_name, int *out_distance);

// ---------------- infer_var_list_nearest ----------------
// Finds the nearest variable name to the given variable name
// within the inference variable list.
// (NOTE: Minimum distance of 3 to count as near enough)
void infer_var_list_nearest(infer_var_list_t *list, const char *name, char **out_nearest_name, int *out_distance);

#endif // INFER_H
