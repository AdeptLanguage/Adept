
#ifndef _ISAAC_IR_GEN_FIND_H
#define _ISAAC_IR_GEN_FIND_H

/*
    ============================== ir_gen_find.h ==============================
    Module for locating intermediate representation data structures
    that reside in organized lists
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "IR/ir_proc_query.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "UTIL/func_pair.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_find_proc ----------------
// Generic way to find a procedure given a custom query
errorcode_t ir_gen_find_proc(ir_proc_query_t *query, optional_func_pair_t *result);

// ---------------- ir_gen_find_func_named ----------------
// Finds a function that exactly matches the given name.
// Result info stored 'result'
// Optionally, whether the function has a unique name is
// stored into 'out_is_unique'
errorcode_t ir_gen_find_func_named(object_t *object, weak_cstr_t name, bool *out_is_unique, func_pair_t *result, bool allow_polymorphic);

// ---------------- ir_gen_find_func_regular ----------------
// Finds a function that exactly matches the given
// name and arguments. Result info stored 'result'
// NOTE: optional_required_traits can be TRAIT_NONE
errorcode_t ir_gen_find_func_regular(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t function_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    trait_t traits_mask,
    trait_t traits_match,
    source_t from_source,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_find_func_conforming ----------------
// Finds a function that has the given name and conforms
// to the arguments given. Result info stored 'result'
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
// NOTE: 'optional_gives' may be NULL or have '.elements_length' be zero
//       to indicate no return matching
// NOTE: 'from_source' is used for error messages, it may be NULL_SOURCE if not applicable
errorcode_t ir_gen_find_func_conforming(
    ir_builder_t *builder,
    weak_cstr_t function_name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    source_t from_source,
    optional_func_pair_t *out_result
);

errorcode_t ir_gen_find_func_conforming_without_defaults(
    ir_builder_t *builder,
    weak_cstr_t function_name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    source_t from_source,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_find_method ----------------
// Find method without any conforming
errorcode_t ir_gen_find_method(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t method_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    source_t from_source,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_find_dispatchee ----------------
// Find dispatchee method without any conforming
errorcode_t ir_gen_find_dispatchee(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t method_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    source_t from_source,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_find_method_conforming ----------------
// Finds a method that has the given name and conforms
// to the arguments given. Result info stored 'result'
// NOTE: 'from_source' is used for error messages, it may be NULL_SOURCE if not applicable
errorcode_t ir_gen_find_method_conforming(
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *gives,
    source_t from_source,
    optional_func_pair_t *out_result
);

errorcode_t ir_gen_find_method_conforming_without_defaults(
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *gives,
    source_t from_source,
    optional_func_pair_t *out_result
);

// ---------------- ir_gen_find_singular_special_func ----------------
// Finds a special function (such as __variadic_array__)
// Sets 'out_ir_func_id' ONLY IF the IR function was found.
// Returns SUCCESS if found
// Returns FAILURE if not found
// Returns ALT_FAILURE if something went wrong
errorcode_t ir_gen_find_singular_special_func(compiler_t *compiler, object_t *object, weak_cstr_t func_name, func_id_t *out_ir_func_id);

#endif // _ISAAC_IR_GEN_FIND_H
