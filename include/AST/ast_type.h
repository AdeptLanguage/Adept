
#ifndef _ISAAC_AST_TYPE_H
#define _ISAAC_AST_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif


/*
    =============================== ast_type.h ===============================
    Module for manipulating abstract syntax tree types

    NOTE: Type elements appear in ast_type_t in the same order they are
    in the actual source code. For example the type **ubyte would be represented
    as [PTR] [PTR] [BASE] in the ast_type_t elements array
    ---------------------------------------------------------------------------
*/

#include "UTIL/hash.h"
#include "UTIL/trait.h"
#include "UTIL/ground.h"

// Possible AST type elements
#define AST_ELEM_NONE             0x00
#define AST_ELEM_BASE             0x01
#define AST_ELEM_POINTER          0x02
#define AST_ELEM_ARRAY            0x03
#define AST_ELEM_FIXED_ARRAY      0x04
#define AST_ELEM_GENERIC_INT      0x05
#define AST_ELEM_GENERIC_FLOAT    0x06
#define AST_ELEM_FUNC             0x07
#define AST_ELEM_POLYMORPH        0x08
#define AST_ELEM_POLYCOUNT        0x09
#define AST_ELEM_POLYMORPH_PREREQ 0x0A
#define AST_ELEM_GENERIC_BASE     0x0B

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
    strong_cstr_t base;
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
    length_t arity;
    ast_type_t *return_type;
    trait_t traits; // Uses AST_FUNC_* traits
    bool ownership; // Used for referencing functions w/o copying
} ast_elem_func_t;

// ---------------- ast_elem_polymorph_t ----------------
// Type element for a polymorphic type variable
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
    bool allow_auto_conversion;
} ast_elem_polymorph_t;

// ---------------- ast_elem_polycount_t ----------------
// Type element for a polymorphic count variable
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
} ast_elem_polycount_t;

// ---------------- ast_elem_polymorph_prereq_t ----------------
// Type element for a polymorphic type variable which only fits
// for structs that match the similarity prerequisite
// NOTE: Must overlap with 'ast_elem_polymorph_t'
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
    bool allow_auto_conversion;
    // ----------------------------
    strong_cstr_t similarity_prerequisite;
} ast_elem_polymorph_prereq_t;

// ---------------- ast_elem_generic_base_t ----------------
// Type element for a variant of a generic base
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
    ast_type_t *generics;
    length_t generics_length;
    bool name_is_polymorphic;
} ast_elem_generic_base_t;

// ---------------- ast_unnamed_arg_t ----------------
// Data structure for an unnamed argument
typedef struct {
    ast_type_t type;
    source_t source;
    char flow; // in | out | inout
} ast_unnamed_arg_t;

// ---------------- ast_poly_catalog_type_t ----------------
// Data structure for a single polymorphic type binding
typedef struct {
    weak_cstr_t name;
    ast_type_t binding;
} ast_poly_catalog_type_t;

// ---------------- ast_poly_catalog_count_t ----------------
// Data structure for a single polymorphic count binding
typedef struct {
    weak_cstr_t name;
    length_t binding;
} ast_poly_catalog_count_t;

// ---------------- ast_poly_catalog_t ----------------
// Data structure for polymorphic type bindings
typedef struct {
    ast_poly_catalog_type_t *types;
    length_t types_length;
    length_t types_capacity;
    ast_poly_catalog_count_t *counts;
    length_t counts_length;
    length_t counts_capacity;
} ast_poly_catalog_t;

// ---------------- ast_elem_clone ----------------
// Clones an AST type element, producing a duplicate
ast_elem_t *ast_elem_clone(const ast_elem_t *element);

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
void ast_type_make_base(ast_type_t *type, strong_cstr_t base);

// ---------------- ast_type_make_base_ptr ----------------
// Takes ownership of 'base' and creates a type from it
// that's preceded by a pointer element
void ast_type_make_base_ptr(ast_type_t *type, strong_cstr_t base);

// ---------------- ast_type_make_base_ptr_ptr ----------------
// Takes ownership of 'base' and creates a type from it
// that's preceded by two pointer elements
void ast_type_make_base_ptr_ptr(ast_type_t *type, strong_cstr_t base);

// ---------------- ast_type_prepend_ptr ----------------
// Prepends a pointer to an AST type
void ast_type_prepend_ptr(ast_type_t *type);

// ---------------- ast_type_make_polymorph ----------------
// Takes ownership of 'name' and creates a polymorphic type variable
void ast_type_make_polymorph(ast_type_t *type, strong_cstr_t name, bool allow_auto_conversion);

// ---------------- ast_type_make_func_ptr ----------------
// Makes a function pointer AST type
void ast_type_make_func_ptr(ast_type_t *type, source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership);

// ---------------- ast_type_make_generic_int ----------------
// Creates a generic integer type
void ast_type_make_generic_int(ast_type_t *type);

// ---------------- ast_type_make_generic_float ----------------
// Creates a generic floating point type
void ast_type_make_generic_float(ast_type_t *type);

// ---------------- ast_type_str ----------------
// Generates a c-string given an AST type
strong_cstr_t ast_type_str(const ast_type_t *type);

// ---------------- ast_types_identical ----------------
// Returns whether or not two AST types are identical
bool ast_types_identical(const ast_type_t *a, const ast_type_t *b);

// ---------------- ast_type_is_void ----------------
// Returns whether an AST type is "void"
bool ast_type_is_void(const ast_type_t *type);

// ---------------- ast_type_is_base ----------------
// Returns whether an AST type is a base
bool ast_type_is_base(const ast_type_t *type);

// ---------------- ast_type_is_base_ptr ----------------
// Returns whether an AST type is a pointer to a base
bool ast_type_is_base_ptr(const ast_type_t *type);

// ---------------- ast_type_is_base_of ----------------
// Returns whether an AST type is a particular base
bool ast_type_is_base_of(const ast_type_t *type, const char *base);

// ---------------- ast_type_is_base_ptr ----------------
// Returns whether an AST type is a pointer to a particular base
bool ast_type_is_base_ptr_of(const ast_type_t *type, const char *base);

// ---------------- ast_type_is_base_like ----------------
// Returns whether an AST type is a base-like AST type.
// This includes either regular base types and generic base types
bool ast_type_is_base_like(const ast_type_t *type);

// ---------------- ast_type_is_pointer ----------------
// Returns whether an AST type is a pointer
// NOTE: Does NOT include 'ptr'
bool ast_type_is_pointer(const ast_type_t *type);

// ---------------- ast_type_is_pointer_to ----------------
// Returns whether an AST type is a pointer to another AST type
bool ast_type_is_pointer_to(const ast_type_t *type, const ast_type_t *to);

// ---------------- ast_type_is_pointer_to_base ----------------
// Returns whether an AST type is a pointer to a base
bool ast_type_is_pointer_to_base(const ast_type_t *type);

// ---------------- ast_type_is_pointer_to_generic_base ----------------
// Returns whether an AST type is a pointer to a generic base
bool ast_type_is_pointer_to_generic_base(const ast_type_t *type);

// ---------------- ast_type_is_pointer_to_base_like ----------------
// Returns whether an AST type is a pointer to a base-like AST type.
// This includes either regular base types and generic base types
bool ast_type_is_pointer_to_base_like(const ast_type_t *type);

// ---------------- ast_type_is_polymorph ----------------
// Returns whether an AST type is a plain polymorphic type
bool ast_type_is_polymorph(const ast_type_t *type);

// ---------------- ast_type_is_polymorph_ptr ----------------
// Returns whether an AST type is a pointer to a plain polymorphic type
bool ast_type_is_polymorph_ptr(const ast_type_t *type);

// ---------------- ast_type_is_generic_base ----------------
// Returns whether an AST type is a plain generic base type
bool ast_type_is_generic_base(const ast_type_t *type);

// ---------------- ast_type_is_generic_base_ptr ----------------
// Returns whether an AST type is a pointer to a plain generic base type
bool ast_type_is_generic_base_ptr(const ast_type_t *type);

// ---------------- ast_type_is_fixed_array ----------------
// Returns whether an AST type is a fixed array
bool ast_type_is_fixed_array(const ast_type_t *type);

// ---------------- ast_type_has_polymorph ----------------
// Returns whether an AST type contains a polymorphic type
bool ast_type_has_polymorph(const ast_type_t *type);

// ---------------- ast_type_dereference ----------------
// Removes the first pointer element of a pointer type
// NOTE: Assumes inout_type has ownership of AST type elements
// NOTE: Frees memory allocated for the pointer element
void ast_type_dereference(ast_type_t *inout_type);

// ---------------- ast_type_unwrap_fixed_array ----------------
// Removes the first fixed-array element of a fixed-array type
// NOTE: Assumes inout_type has ownership of AST type elements
// NOTE: Frees memory allocated for the fixed-array element
void ast_type_unwrap_fixed_array(ast_type_t *inout_type);

// ---------------- ast_poly_catalog_init ----------------
// Initializes an AST type var catalog
void ast_poly_catalog_init(ast_poly_catalog_t *catalog);

// ---------------- ast_poly_catalog_free ----------------
// Frees an AST type var catalog
void ast_poly_catalog_free(ast_poly_catalog_t *catalog);

// ---------------- ast_poly_catalog_add_type ----------------
// Adds an AST type variable binding to an AST type var catalog
// NOTE: 'binding' doesn't have to be preserved after this call
void ast_poly_catalog_add_type(ast_poly_catalog_t *catalog, weak_cstr_t name, ast_type_t *binding);

// ---------------- ast_poly_catalog_add_count ----------------
// Adds an AST count variable binding to an AST type var catalog
// NOTE: 'binding' doesn't have to be preserved after this call
void ast_poly_catalog_add_count(ast_poly_catalog_t *catalog, weak_cstr_t name, length_t binding);

// ---------------- ast_poly_catalog_find_type ----------------
// Finds an AST type variable binding within an AST type var catalog
ast_poly_catalog_type_t *ast_poly_catalog_find_type(ast_poly_catalog_t *catalog, weak_cstr_t name);

// ---------------- ast_poly_catalog_find_count ----------------
// Finds an AST count variable binding within an AST type var catalog
ast_poly_catalog_count_t *ast_poly_catalog_find_count(ast_poly_catalog_t *catalog, weak_cstr_t name);

// ---------------- ast_type_hash ----------------
// Hashes an AST type
hash_t ast_type_hash(ast_type_t *type);

// ---------------- ast_elem_hash ----------------
// Hashes an AST type element
hash_t ast_elem_hash(ast_elem_t *element);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_H
