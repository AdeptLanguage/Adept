
#ifndef ASSEMBLE_H
#define ASSEMBLE_H

/*
    =============================== assemble.h ================================
    Module for generating intermediate-representation from an abstract
    syntax tree
    ---------------------------------------------------------------------------
*/

#include "ir.h"
#include "ast.h"
#include "ground.h"
#include "compiler.h"
#include "irbuilder.h"

// ---------------- assemble_type_mappings ----------------
int assemble(compiler_t *compiler, object_t *object);

// ---------------- assemble_functions_body ----------------
// Generates IR function skeletons for AST functions.
int assemble_functions(compiler_t *compiler, object_t *object);

// ---------------- assemble_functions_body ----------------
// Generates IR function bodies for AST functions.
// Assumes IR function skeletons were already generated.
int assemble_functions_body(compiler_t *compiler, object_t *object);

// ---------------- assemble_globals ----------------
// Generates IR globals from AST globals
int assemble_globals(compiler_t *compiler, object_t *object);

// ---------------- assemble_globals_init ----------------
// Generates IR instructions for initializing global variables
int assemble_globals_init(ir_builder_t *builder);

// ---------------- ir_func_mapping_cmp ----------------
// Compares two 'ir_func_mapping_t' structures.
// Used for qsort()
int ir_func_mapping_cmp(const void *a, const void *b);

// ---------------- ir_method_cmp ----------------
// Compares two 'ir_method_t' structures.
// Used for qsort()
int ir_method_cmp(const void *a, const void *b);

#endif // ASSEMBLE_H
