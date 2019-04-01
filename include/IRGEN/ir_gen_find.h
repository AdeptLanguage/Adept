
#ifndef IR_GEN_FIND_H
#define IR_GEN_FIND_H

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
#include "IRGEN/ir_builder.h"

// ---------------- funcpair_t ----------------
// A container for function results
typedef struct {
    ast_func_t *ast_func;
    ir_func_t *ir_func;
    length_t ast_func_id;
    length_t ir_func_id;
} funcpair_t;

// ---------------- ir_gen_find_func ----------------
// Finds a function that exactly matches the given
// name and arguments. Result info stored 'result'
errorcode_t ir_gen_find_func(compiler_t *compiler, object_t *object, const char *name,
    ast_type_t *arg_types, length_t arg_types_length, funcpair_t *result);

// ---------------- ir_gen_find_func_named ----------------
// Finds a function that exactly matches the given name.
// Result info stored 'result'
errorcode_t ir_gen_find_func_named(compiler_t *compiler, object_t *object,
    const char *name, funcpair_t *result);

// ---------------- ir_gen_find_func_conforming ----------------
// Finds a function that has the given name and conforms.
// to the arguments given. Result info stored 'result'
// NOTE: Returns SUCCESS when a function was found,
//               FAILURE when a function wasn't found and
//               ALT_FAILURE when something goes wrong
errorcode_t ir_gen_find_func_conforming(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result);

// ---------------- ir_gen_find_method_conforming ----------------
// Finds a method that has the given name and conforms.
// to the arguments given. Result info stored 'result'
errorcode_t ir_gen_find_method_conforming(ir_builder_t *builder, const char *struct_name,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result);

// ---------------- find_beginning_of_func_group ----------------
// Searches for beginning of function group in a list of mappings
maybe_index_t find_beginning_of_func_group(ir_func_mapping_t *mappings, length_t length, const char *name);

// ---------------- find_beginning_of_method_group ----------------
// Searches for beginning of method group in a list of methods
maybe_index_t find_beginning_of_method_group(ir_method_t *methods, length_t length,
    const char *struct_name, const char *name);

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
successful_t func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
        ast_type_t *arg_type_list, length_t type_list_length);

// ---------------- func_args_polymorphable ----------------
// Returns whether the given types work with a polymorphic function template
// NOTE: 'out_catalog' may be NULL
bool func_args_polymorphable(ast_func_t *poly_template, ast_type_t *arg_types, length_t type_length,
    ast_type_var_catalog_t *out_catalog);

#endif // IR_GEN_FIND_H
