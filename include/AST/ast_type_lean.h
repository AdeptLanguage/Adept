
#ifndef _ISAAC_AST_TYPE_LEAN_H
#define _ISAAC_AST_TYPE_LEAN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================= ast_type_lean.h ============================
    Module for lean definitions required for abstract syntax tree types
    --------------------------------------------------------------------------
*/

#include "UTIL/ground.h"

// ---------------- ast_elem_t ----------------
// General purpose struct for a type element
typedef struct {
    unsigned int id;
    source_t source;
} ast_elem_t;

// ---------------- ast_type_t ----------------
// Struct containing AST type information
typedef struct {
    ast_elem_t **elements;
    length_t elements_length;
    source_t source;
} ast_type_t;

// ---------------- AST_TYPE_NONE ----------------
// AST type that represents the absence of a valid type
#define AST_TYPE_NONE ((ast_type_t){0})
#define AST_TYPE_IS_NONE(ast_type_val) ((ast_type_val).elements_length == 0)

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_LEAN_H
