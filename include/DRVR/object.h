
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
#include "UTIL/tmpbuf.h"

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
    length_t buffer_length;      // Length of text buffer
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
    maybe_null_weak_cstr_t default_stdlib;
    maybe_null_strong_cstr_t current_namespace;
    length_t current_namespace_length;

    weak_lenstr_t *using_namespaces;
    length_t using_namespaces_length;
    length_t using_namespaces_capacity;
} object_t;

// Possible traits for object_t
#define OBJECT_NONE    TRAIT_NONE
#define OBJECT_PACKAGE TRAIT_1    // Is an imported package

// ------------------ object_init_ast ------------------
// Initializes the AST portion of an object_t
void object_init_ast(object_t *object, unsigned int cross_compile_for);

// ------------------ object_struct_find ------------------
// Finds a struct with the given name from the accessible namespaces
// NOTE: If 'override_main_ast' is NULL, then 'object->ast' is used as the main AST
// NOTE: If 'out_requires_namespace' isn't NULL, then it will be set to whether the
// struct found requires a namespace to be accessed
ast_struct_t *object_struct_find(ast_t *override_main_ast, object_t *object, tmpbuf_t *tmpbuf, const char *name, bool *out_requires_namespace);

// ------------------ object_polymorphic_struct_find ------------------
// Finds a polymorphic struct with the given name from the accessible namespaces
// NOTE: If 'override_main_ast' is NULL, then 'object->ast' is used as the main AST
// NOTE: If 'out_requires_namespace' isn't NULL, then it will be set to whether the
// struct found requires a namespace to be accessed
ast_polymorphic_struct_t *object_polymorphic_struct_find(ast_t *override_main_ast, object_t *object, tmpbuf_t *tmpbuf, const char *name, bool *out_requires_namespace);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_OBJECT_H
