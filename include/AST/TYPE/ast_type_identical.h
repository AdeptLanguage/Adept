
#ifndef _ISAAC_AST_TYPE_IDENTICAL_H
#define _ISAAC_AST_TYPE_IDENTICAL_H

/*
    ========================== ast_type_identical ==============================
    Definitions for determining if two AST types are identical
    ----------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "AST/ast_type_lean.h"
#include "UTIL/ground.h"

// ---------------- ast_types_identical ----------------
// Returns whether two AST types are identical
bool ast_types_identical(const ast_type_t *a, const ast_type_t *b);

// ---------------- ast_type_lists_identical ----------------
// Returns whether two lists of AST types are identical
bool ast_type_lists_identical(const ast_type_t *a, const ast_type_t *b, length_t length);

// ---------------- ast_elem_identical ----------------
// Returns whether two AST elements are identical
bool ast_elem_identical(ast_elem_t *a, ast_elem_t *b);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_IDENTICAL_H
