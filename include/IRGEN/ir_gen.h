
#ifndef IR_GEN_H
#define IR_GEN_H

/*
    ================================ ir_gen.h =================================
    Module for generating intermediate-representation from an abstract
    syntax tree
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_type_mappings ----------------
errorcode_t ir_gen(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_functions_body ----------------
// Generates IR function skeletons for AST functions.
errorcode_t ir_gen_functions(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_func_head ----------------
// Generates IR function skeleton for an AST function.
errorcode_t ir_gen_func_head(compiler_t *compiler, object_t *object, ast_func_t *ast_func, length_t ast_func_id);

// ---------------- ir_gen_functions_body ----------------
// Generates IR function bodies for AST functions.
// Assumes IR function skeletons were already generated.
errorcode_t ir_gen_functions_body(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_globals ----------------
// Generates IR globals from AST globals
errorcode_t ir_gen_globals(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_globals_init ----------------
// Generates IR instructions for initializing global variables
errorcode_t ir_gen_globals_init(ir_builder_t *builder);

// ---------------- ir_gen_special_global ----------------
// Generates IR instructions for initializing special global variables
errorcode_t ir_gen_special_global(ir_builder_t *builder, ast_global_t *ast_global, length_t global_variable_id);

// ---------------- ir_func_mapping_cmp ----------------
// Compares two 'ir_func_mapping_t' structures.
// Used for qsort()
int ir_func_mapping_cmp(const void *a, const void *b);

// ---------------- ir_method_cmp ----------------
// Compares two 'ir_method_t' structures.
// Used for qsort()
int ir_method_cmp(const void *a, const void *b);

// ---------------- ir_generic_base_method_cmp ----------------
// Compares two 'ir_generic_base_method_t' structures.
// Used for qsort()
int ir_generic_base_method_cmp(const void *a, const void *b);

#endif // IR_GEN_H
