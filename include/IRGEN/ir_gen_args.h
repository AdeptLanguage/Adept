
#ifndef _ISAAC_IR_GEN_ARGS_H
#define _ISAAC_IR_GEN_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_gen_args.h ==============================
    Module for matching/conforming argument values and types
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "AST/ast_type.h"
#include "IRGEN/ir_builder.h"

// ---------------- func_args_match ----------------
// Returns whether a function's arguments match
// the arguments supplied.
successful_t func_args_match(ast_func_t *func, ast_type_t *type_list, length_t type_list_length);

// ---------------- func_args_conform ----------------
// Returns whether a function's arguments conform
// to the arguments supplied.
// NOTE: Just because this function returns true, does NOT mean that
//       the arity supplied meets the function's arity requirement
// NOTE: 'gives' may be NULL
successful_t func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
        ast_type_t *arg_type_list, length_t type_list_length, ast_type_t *gives, trait_t conform_mode);

// ---------------- func_args_polymorphable ----------------
// Returns whether the given types work with a polymorphic function template
// NOTE: 'out_catalog' is only returned if the function is polymorphable
// NOTE: 'out_catalog' may be NULL
// NOTE: Returns SUCCESS if true
// NOTE: Returns ALT_FAILURE if false
// NOTE: Returns FAILURE if couldn't fully resolve
// NOTE: 'gives' may be NULL
errorcode_t func_args_polymorphable(ir_builder_t *builder, ast_func_t *poly_template, ir_value_t **arg_value_list, ast_type_t *arg_types, length_t type_length, ast_poly_catalog_t *out_catalog, ast_type_t *gives, trait_t conform_mode);
errorcode_t func_args_polymorphable_no_conform(compiler_t *compiler, object_t *object, ast_func_t *poly_template, ast_type_t *arg_types, length_t type_list_length, ast_poly_catalog_t *out_catalog);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_ARGS_H
