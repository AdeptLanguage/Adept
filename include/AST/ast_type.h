
#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "UTIL/ground.h"
#include "UTIL/trait.h"

/*
    =============================== ast_type.h ===============================
    Module for manipulating abstract syntax tree types

    NOTE: Type elements appear in ast_type_t in the same order they are
    in the actual source code. For example the type **ubyte would be represented
    as [PTR] [PTR] [BASE] in the ast_type_t elements array
    ---------------------------------------------------------------------------
*/

// Possible AST type elements
#define AST_ELEM_NONE          0x00
#define AST_ELEM_BASE          0x01
#define AST_ELEM_POINTER       0x02
#define AST_ELEM_ARRAY         0x03
#define AST_ELEM_FIXED_ARRAY   0x04
#define AST_ELEM_GENERIC_INT   0x05
#define AST_ELEM_GENERIC_FLOAT 0x06
#define AST_ELEM_FUNC          0x07

// Possible data flow patterns
#define FLOW_NONE  0x00
#define FLOW_IN    0x01
#define FLOW_OUT   0x02
#define FLOW_INOUT 0x03

// ---------------- ast_elem_t ----------------
// General purpose struct for a type element
typedef struct {
    unsigned int id;
    source_t source;
} ast_elem_t;

// ---------------- ast_type_t ----------------
// Struct containing AST type information
typedef struct {
    ast_elem_t **elements;
    length_t elements_length;
    source_t source;
} ast_type_t;

// ---------------- ast_elem_base_t ----------------
// Type element for base structure or primitive
typedef struct {
    unsigned int id;
    source_t source;
    char *base;
} ast_elem_base_t;

// ---------------- ast_elem_pointer_t ----------------
// Type element for a pointer
typedef struct {
    unsigned int id;
    source_t source;
} ast_elem_pointer_t;

// ---------------- ast_elem_array_t ----------------
// Type element for a array
typedef struct {
    unsigned int id;
    source_t source;
} ast_elem_array_t;

// ---------------- ast_elem_fixed_array_t ----------------
// Type element for a fixed array
typedef struct {
    unsigned int id;
    source_t source;
    length_t length;
} ast_elem_fixed_array_t;

// ---------------- ast_elem_func_t ----------------
// Type element for a function pointer
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t *arg_types;
    char *arg_flows;
    length_t arity;
    ast_type_t *return_type;
    trait_t traits; // Uses AST_FUNC_* traits
    bool ownership; // Whether or not we own our members
} ast_elem_func_t;

// ---------------- ast_unnamed_arg_t ----------------
// Data structure for an unnamed argument
typedef struct {
    ast_type_t type;
    source_t source;
    char flow; // in | out | inout
} ast_unnamed_arg_t;

// ---------------- ast_type_clone ----------------
// Clones an AST type, producing a duplicate
ast_type_t ast_type_clone(const ast_type_t *type);

// ---------------- ast_type_free ----------------
// Frees data within an AST type
void ast_type_free(ast_type_t *type);

// ---------------- ast_type_free_fully ----------------
// Frees data within an AST type and the container
void ast_type_free_fully(ast_type_t *type);

// ---------------- ast_types_free ----------------
// Calls 'ast_type_free' on each type in the list
void ast_types_free(ast_type_t *types, length_t length);

// ---------------- ast_types_free_fully ----------------
// Calls 'ast_type_free_fully' on each type in the list
void ast_types_free_fully(ast_type_t *types, length_t length);

// ---------------- ast_type_make_base ----------------
// Takes ownership of 'base' and creates a type from it
void ast_type_make_base(ast_type_t *type, char *base);

// ---------------- ast_type_make_base_newstr ----------------
// Clones 'base' and creates a type from the cloned string
void ast_type_make_base_newstr(ast_type_t *type, char *base);

// ---------------- ast_type_make_baseptr ----------------
// Takes ownership of 'base' and creates a type from it
// that's preceded by a pointer element
void ast_type_make_baseptr(ast_type_t *type, char *base);

// ---------------- ast_type_make_base_newstr ----------------
// Clones 'base' and creates a type from the cloned string
// that's preceded by a pointer element
void ast_type_make_baseptr_newstr(ast_type_t *type, char *base);

// ---------------- ast_type_prepend_ptr ----------------
// Prepends a pointer to an AST type
void ast_type_prepend_ptr(ast_type_t *type);

// ---------------- ast_type_make_generic_int ----------------
// Creates a generic integer type
void ast_type_make_generic_int(ast_type_t *type);

// ---------------- ast_type_make_generic_float ----------------
// Creates a generic floating point type
void ast_type_make_generic_float(ast_type_t *type);

// ---------------- ast_type_str ----------------
// Generates a c-string given an AST type
char* ast_type_str(const ast_type_t *type);

// ---------------- ast_types_identical ----------------
// Returns whether or not two AST types are identical
bool ast_types_identical(const ast_type_t *a, const ast_type_t *b);

#endif // AST_TYPE_H
