
#ifndef _ISAAC_AST_TRANSLATE_H
#define _ISAAC_AST_TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast.h"
#include "AST/ast_type_lean.h"
#include "DRVR/compiler.h"
#include "UTIL/ground.h"

// ---------------- ast_translate_poly_parent_class ----------------
// Translates a polymorphic parent class type
// using contextual information from a specific usage
// EXAMPLE:
// For the concrete type usage:
//     point <float> Vector4
// The `<$T> Vector3f` in
//     class <$T> Vector4 extends <$T> Vector3 (w $T)
// will be copied, translated into context (`<float> Vector3f`), and returned.
// The resolved type will be stored in `out_type` if successful.
// USE CASE:
// This function is used to help translate compile-time polymorphism
// during vtable generation, as required classes are not guaranteed to do any
// IR generation.
errorcode_t ast_translate_poly_parent_class(
    compiler_t *compiler,
    ast_t *ast,
    ast_composite_t *the_child_class,
    const ast_type_t *the_child_class_concrete_usage, 
    ast_type_t *out_type
);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TRANSLATE_H
