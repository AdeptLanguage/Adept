
#ifndef _ISAAC_AST_EXPR_LEAN_H
#define _ISAAC_AST_EXPR_LEAN_H

/*
    ============================ ast_expr_lean.h =============================
    Module for lean definitions required for abstract syntax tree expressions
    --------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "UTIL/list.h"

#define DERIVE_AST_EXPR struct { \
    unsigned int id;  /* What type of expression */ \
    source_t source;  /* Where in source code */ \
}

// ---------------- ast_expr_t ----------------
// General purpose struct for an expression
typedef struct {
    DERIVE_AST_EXPR;
} ast_expr_t;

// ---------------- ast_expr_list_t ----------------
// List structure for holding statements/expressions
typedef listof(ast_expr_t*, statements) ast_expr_list_t;

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_EXPR_LEAN_H
