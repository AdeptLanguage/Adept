
#ifndef _ISAAC_IR_GEN_FIND_SF_H
#define _ISAAC_IR_GEN_FIND_SF_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================= ir_gen_find_sf.h =============================
    Module for finding special procedures
    ----------------------------------------------------------------------------
*/

#include "AST/ast_type_lean.h"
#include "BRIDGE/funcpair.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/ground.h"

// ---------------- ir_gen_find_pass_func ----------------
// Finds the correct __pass__ function for a type
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_pass_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, optional_funcpair_t *result);

// ---------------- ir_gen_find_defer_func ----------------
// Finds the correct __defer__ function for a type
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_defer_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_funcpair_t *result);

// ---------------- ir_gen_find_assign_func ----------------
// Finds the correct __assign__ function for a type
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_assign_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_funcpair_t *result);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_FIND_SF_H
