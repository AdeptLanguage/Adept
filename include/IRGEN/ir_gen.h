
#ifndef _ISAAC_IR_GEN_H
#define _ISAAC_IR_GEN_H

/*
    ================================ ir_gen.h =================================
    Module for generating intermediate-representation from an abstract
    syntax tree
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_type_mappings ----------------
errorcode_t ir_gen(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_functions_body ----------------
// Generates IR function skeletons for AST functions.
errorcode_t ir_gen_functions(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_func_template ----------------
// Generates empty template IR function
errorcode_t ir_gen_func_template(compiler_t *compiler, object_t *object, weak_cstr_t name, source_t from_source, funcid_t *out_ir_func_id);

// ---------------- ir_gen_func_head ----------------
// Generates IR function skeleton for an AST function.
errorcode_t ir_gen_func_head(compiler_t *compiler, object_t *object, ast_func_t *ast_func,
    funcid_t ast_func_id, ir_func_endpoint_t *optional_out_new_endpoint);

// ---------------- ir_gen_functions_body ----------------
// Generates IR function bodies for AST functions.
// Assumes IR function skeletons were already generated.
errorcode_t ir_gen_functions_body(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_functions_body_statements ----------------
// Generates the required intermediate representation for
// statements inside an AST function. Internally it
// creates an 'ir_builder_t' and calls 'ir_gen_stmts'
errorcode_t ir_gen_functions_body_statements(compiler_t *compiler, object_t *object, funcid_t ast_func_id, funcid_t ir_func_id);

// ---------------- ir_gen_job_list ----------------
// Generates initial IR job list to kickstart the main
// IR function bodies generation
errorcode_t ir_gen_job_list(object_t *object);

// ---------------- ir_gen_globals ----------------
// Generates IR globals from AST globals
errorcode_t ir_gen_globals(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_globals_init ----------------
// Generates IR instructions for initializing global variables
errorcode_t ir_gen_globals_init(ir_builder_t *builder);

// ---------------- ir_gen_special_global ----------------
// Generates initializers for special global variables
errorcode_t ir_gen_special_global(compiler_t *compiler, object_t *object, ast_global_t *ast_global, length_t global_variable_id);
errorcode_t ir_gen_special_globals(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_fill_in_rtti ----------------
// Fills in generated IR runtime type information data
errorcode_t ir_gen_fill_in_rtti(object_t *object);

// ---------------- ir_gen_ast_definition_string ----------------
// Makes a string containing the textual definition of an AST function using an IR memory pool
weak_cstr_t ir_gen_ast_definition_string(ir_pool_t *pool, ast_func_t *ast_func);

// ---------------- ir_gen_do_builtin_warn_bad_printf_format ----------------
// Warns if incorrect arguments to printf format
errorcode_t ir_gen_do_builtin_warn_bad_printf_format(ir_builder_t *compiler, funcpair_t pair, ast_type_t *ast_types, ir_value_t **ir_values, source_t source, length_t variadic_length);

// ---------------- ir_gen_do_builtin_warn_bad_printf_format ----------------
// Prints error for incorrect arguments to printf format
void bad_printf_format(compiler_t *compiler, source_t source, ast_type_t *given_type, weak_cstr_t expected, int variadic_argument_number, int size_modifier, char specifier, bool is_semimatch);

// ---------------- get_numeric_ending ----------------
// Gets suffix for an integer
weak_cstr_t get_numeric_ending(length_t integer);

#endif // _ISAAC_IR_GEN_H
