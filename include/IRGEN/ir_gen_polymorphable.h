
#ifndef _ISAAC_IR_GEN_POLYMORPHABLE_H
#define _ISAAC_IR_GEN_POLYMORPHABLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ========================== ir_gen_polymorphable.h ==========================
    Module for polymorphism checking
    ----------------------------------------------------------------------------
*/

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type_lean.h"

// ---------------- ir_gen_polymorphable ----------------
// Finds whether a concrete AST type is valid for a given polymorphic type
// NOTE: If 'permit_similar_primitives' is true, then trying to use a different trivially-convertible primitive type for a top-level simple polymorph type will not raise an error,
//       regardless of user-specified loose polymorphism
// NOTE: Returns SUCCESS if true
// NOTE: Returns FAILURE if false
// NOTE: Returns ALT_FAILURE if couldn't fully resolve
errorcode_t ir_gen_polymorphable(compiler_t *compiler, object_t *object, ast_type_t *polymorphic_type, ast_type_t *concrete_type, ast_poly_catalog_t *catalog, bool permit_similar_primitives);

// ---------------- ir_gen_does_extend ----------------
// Returns whether a concrete subject type extends a potential parent type
// Reads and writes to `catalog`
// NOTE: If this function returns true and the parent type is polymorphic,
// then the parent type can be resolved using the `catalog` supplied
// to get the concrete version of the parent class
bool ir_gen_does_extend(compiler_t *compiler, object_t *object, ast_type_t *subject_usage, ast_type_t *parent, ast_poly_catalog_t *catalog);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_POLYMORPHABLE_H
