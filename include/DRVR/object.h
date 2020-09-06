
#ifndef _ISAAC_OBJECT_H
#define _ISAAC_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= object.h =================================
    Structure for containing information and data structures for a given file
    ---------------------------------------------------------------------------
*/

#include "AST/ast.h"
#include "LEX/token.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "IR/ir.h"
#endif

// Possible compilation stages a file can be in.
// Used to determine what information to free on destruction.
#define COMPILATION_STAGE_NONE      0x00
#define COMPILATION_STAGE_FILENAME  0x01
#define COMPILATION_STAGE_TOKENLIST 0x02
#define COMPILATION_STAGE_AST       0x03
#define COMPILATION_STAGE_IR_MODULE 0x04

// ------------------ object_t ------------------
// Struct that contains data associated with a
// a file that has been processed by the compiler
typedef struct object {
    strong_cstr_t filename;      // Filename
    strong_cstr_t full_filename; // Absolute filename (used for testing duplicate imports)
    strong_cstr_t buffer;        // Text buffer
    tokenlist_t tokenlist;       // Token list
    ast_t ast;                   // Abstract syntax tree

    #ifndef ADEPT_INSIGHT_BUILD
    ir_module_t ir_module;       // Intermediate-Representation module
    #endif

    int compilation_stage;       // Compilation stage
    length_t index;              // Index in object array (in compiler_t)
    trait_t traits;              // Object traits

    // Default standard library to import from (per object version)
    // If NULL, then use ADEPT_VERSION_STRING
    maybe_null_weak_cstr_t default_stblib;
} object_t;

// Possible traits for object_t
#define OBJECT_NONE    TRAIT_NONE
#define OBJECT_PACKAGE TRAIT_1    // Is an imported package

void object_init_ast(object_t *object, unsigned int cross_compile_for);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_OBJECT_H
