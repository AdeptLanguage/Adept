
#ifndef _ISAAC_AST_TYPE_SET_H
#define _ISAAC_AST_TYPE_SET_H

#include "AST/ast_type_lean.h"
#include "UTIL/set.h"
#include "UTIL/ground.h"

// ---------------- ast_type_set_t ----------------
// A set collection of AST types
typedef struct { set_t impl; } ast_type_set_t;

void ast_type_set_init(ast_type_set_t *set, length_t num_buckets);
bool ast_type_set_insert(ast_type_set_t *set, ast_type_t *type);
void ast_type_set_traverse(ast_type_set_t *set, void (*run_func)(ast_type_t*));
void ast_type_set_free(ast_type_set_t *set);

#endif // _ISAAC_AST_TYPE_SET_H
