
#ifndef _ISAAC_IR_GEN_FIND_H
#define _ISAAC_IR_GEN_FIND_H

/*
    ============================== ir_gen_find.h ==============================
    Module for locating intermediate representation data structures
    that reside in organized lists
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "BRIDGE/funcpair.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_find_func ----------------
// Finds a function that exactly matches the given
// name and arguments. Result info stored 'result'
// NOTE: optional_required_traits can be TRAIT_NONE
errorcode_t ir_gen_find_func(compiler_t *compiler, object_t *object, ir_job_list_t *job_list, const char *name,
    ast_type_t *arg_types, length_t arg_types_length, trait_t mask, trait_t req_traits, funcpair_t *result);

errorcode_t ir_gen_find_func_inner(compiler_t *compiler, object_t *object, ir_job_list_t *job_list, const char *name,
    ast_type_t *arg_types, length_t arg_types_length, trait_t mask, trait_t req_traits, funcpair_t *result);

// ---------------- ir_gen_find_func_named ----------------
// Finds a function that exactly matches the given name.
// Result info stored 'result'
// Optionally, whether the function has a unique name is
// stored into 'out_is_unique'
errorcode_t ir_gen_find_func_named(compiler_t *compiler, object_t *object, const char *name, bool *out_is_unique, funcpair_t *result);

errorcode_t ir_gen_find_func_named_inner(object_t *object, const char *name, bool *out_is_unique, funcpair_t *result);

// ---------------- ir_gen_find_func_conforming ----------------
// Finds a function that has the given name and conforms
// to the arguments given. Result info stored 'result'
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_func_conforming(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result);

errorcode_t ir_gen_find_func_conforming_inner(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result);

errorcode_t ir_gen_find_func_conforming_to(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result, trait_t conform_mode);


// ---------------- ir_gen_find_pass_func ----------------
// Finds the correct __pass__ function for a type
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_pass_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, funcpair_t *result);

// ---------------- ir_gen_find_defer_func ----------------
// Finds the correct __defer__ function for a type
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_defer_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, funcpair_t *result);

// ---------------- ir_gen_find_method_conforming ----------------
// Finds a method that has the given name and conforms
// to the arguments given. Result info stored 'result'
errorcode_t ir_gen_find_method_conforming(ir_builder_t *builder, const char *struct_name,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result);

errorcode_t ir_gen_find_method_conforming_to(ir_builder_t *builder, const char *struct_name,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result, trait_t conform_mode);

// ---------------- ir_gen_find_generic_base_method_conforming ----------------
// Finds a method that has the matches a generic base and conforms
// to the arguments given. Result info stored 'result'
errorcode_t ir_gen_find_generic_base_method_conforming(ir_builder_t *builder, const char *generic_base,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result);

errorcode_t ir_gen_find_generic_base_method_conforming_to(ir_builder_t *builder, const char *generic_base,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result, trait_t conform_mode);

// ---------------- find_beginning_of_func_group ----------------
// Searches for beginning of function group in a list of mappings
maybe_index_t find_beginning_of_func_group(ir_func_mapping_t *mappings, length_t length, const char *name);

// ---------------- find_beginning_of_method_group ----------------
// Searches for beginning of method group in a list of methods
maybe_index_t find_beginning_of_method_group(ir_method_t *methods, length_t length,
    const char *struct_name, const char *name);

// ---------------- find_beginning_of_generic_base_method_group ----------------
// Searches for beginning of method group in a list of generic base methods
maybe_index_t find_beginning_of_generic_base_method_group(ir_generic_base_method_t *methods, length_t length,
    const char *generic_base, const char *name);

// ---------------- find_beginning_of_poly_func_group ----------------
// Searches for beginning of function group in a list of mappings
maybe_index_t find_beginning_of_poly_func_group(ast_polymorphic_func_t *polymorphic_funcs, length_t polymorphic_funcs_length, const char *name);

// ---------------- func_args_match ----------------
// Returns whether a function's arguments match
// the arguments supplied.
successful_t func_args_match(ast_func_t *func, ast_type_t *type_list, length_t type_list_length);

// ---------------- func_args_conform ----------------
// Returns whether a function's arguments conform
// to the arguments supplied.
// NOTE: Just because this function returns true, does NOT mean that
//       the arity supplied meets the function's arity requirement
successful_t func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
        ast_type_t *arg_type_list, length_t type_list_length, trait_t conform_mode);

// ---------------- func_args_polymorphable ----------------
// Returns whether the given types work with a polymorphic function template
// NOTE: 'out_catalog' is only returned if the function is polymorphable
// NOTE: 'out_catalog' may be NULL
// NOTE: Returns SUCCESS if true
// NOTE: Returns ALT_FAILURE if false
// NOTE: Returns FAILURE if couldn't fully resolve
errorcode_t func_args_polymorphable(ir_builder_t *builder, ast_func_t *poly_template, ir_value_t **arg_value_list, ast_type_t *arg_types,
        length_t type_length, ast_type_var_catalog_t *out_catalog, trait_t conform_mode);

// ---------------- ast_type_has_polymorph ----------------
// Finds whether a concrete AST type is valid for a given polymorphic type
// NOTE: Returns SUCCESS if true
// NOTE: Returns FAILURE if false
// NOTE: Returns ALT_FAILURE if couldn't fully resolve
errorcode_t arg_type_polymorphable(ir_builder_t *builder, const ast_type_t *polymorphic_type, const ast_type_t *concrete_type, ast_type_var_catalog_t *catalog);

// ---------------- ir_gen_find_special_func ----------------
// Finds a special function (such as __variadic_array__ or __initializer_list__)
// Sets 'out_ir_func_id' ONLY IF the IR function was found.
// Returns SUCCESS if found
// Returns FAILURE if not found
// Returns ALT_FAILURE if something went wrong
errorcode_t ir_gen_find_special_func(compiler_t *compiler, object_t *object, weak_cstr_t func_name, length_t *out_ir_func_id);

#endif // _ISAAC_IR_GEN_FIND_H
