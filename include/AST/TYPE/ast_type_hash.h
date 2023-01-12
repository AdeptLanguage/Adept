
#ifndef _ISAAC_AST_TYPE_HASH_H
#define _ISAAC_AST_TYPE_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================ ast_type_hash.h ============================
    Definitions for hashing AST types
    --------------------------------------------------------------------------
*/

#include "UTIL/hash.h"
#include "UTIL/ground.h"
#include "AST/ast_type_lean.h"

// ---------------- ast_type_hash ----------------
// Hashes an AST type
hash_t ast_type_hash(const ast_type_t *type);

// ---------------- ast_types_hash ----------------
// Hashes a collection of AST types
hash_t ast_types_hash(const ast_type_t *types, length_t length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_HASH_H
