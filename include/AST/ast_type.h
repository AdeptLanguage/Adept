
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
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type_lean.h" // IWYU pragma: export

// Possible AST type elements
enum {
    AST_ELEM_NONE,
    AST_ELEM_BASE,
    AST_ELEM_POINTER,
    AST_ELEM_ARRAY,
    AST_ELEM_FIXED_ARRAY,
    AST_ELEM_VAR_FIXED_ARRAY,
    AST_ELEM_GENERIC_INT,
    AST_ELEM_GENERIC_FLOAT,
    AST_ELEM_FUNC,
    AST_ELEM_POLYMORPH,
    AST_ELEM_POLYCOUNT,
    AST_ELEM_POLYMORPH_PREREQ,
    AST_ELEM_GENERIC_BASE,
    AST_ELEM_LAYOUT,
};

// Possible data flow patterns
enum {
    FLOW_NONE,
    FLOW_IN,
    FLOW_OUT,
    FLOW_INOUT,
};

// ---------------- ast_elem_base_t ----------------
// Type element for base structure or primitive
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t base;
} ast_elem_base_t;

// ---------------- ast_elem_pointer_t ----------------
// Type element for a pointer
typedef ast_elem_t ast_elem_pointer_t;

// ---------------- ast_elem_array_t ----------------
// Type element for an array
typedef ast_elem_t ast_elem_array_t;

// ---------------- ast_elem_fixed_array_t ----------------
// Type element for a fixed array
typedef struct {
    unsigned int id;
    source_t source;
    length_t length;
} ast_elem_fixed_array_t;

// ---------------- ast_elem_generic_int_t ----------------
// Type element for a generic integer type
typedef ast_elem_t ast_elem_generic_int_t;

// ---------------- ast_elem_generic_float_t ----------------
// Type element for a generic floating point type
typedef ast_elem_t ast_elem_generic_float_t;

// ---------------- ast_elem_var_fixed_array_t ----------------
// Type element for a variable fixed array
// This type element should be collapsed into a regular ast_elem_fixed_array_t
// during the inference stage.
// We will assume that
// sizeof(ast_elem_var_fixed_array_t) <= sizeof(ast_elem_fixed_array_t)
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *length;
} ast_elem_var_fixed_array_t;

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
#define DERIVE_ELEM_POLYMORPH struct { \
    unsigned int id; \
    source_t source; \
    strong_cstr_t name; \
    bool allow_auto_conversion; \
}

typedef struct {
    DERIVE_ELEM_POLYMORPH;
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
// NOTE: Guaranteed to overlap with 'ast_elem_polymorph_t'
typedef struct {
    DERIVE_ELEM_POLYMORPH;

    strong_cstr_t similarity_prerequisite;
    ast_type_t extends;
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

// ---------------- ast_elem_layout_t ----------------
// Type element for an anonymous composite
typedef struct {
    unsigned int id;
    source_t source;
    ast_layout_t layout;
} ast_elem_layout_t;

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

// ---------------- ast_types_clone ----------------
// Clones a list of AST types
ast_type_t *ast_types_clone(ast_type_t *types, length_t length);

// ---------------- ast_type_free ----------------
// Frees data within an AST type
void ast_type_free(ast_type_t *type);

// ---------------- ast_type_free_fully ----------------
// Frees data within an AST type and the container (type can be NULL)
void ast_type_free_fully(ast_type_t *type);

// ---------------- ast_types_free ----------------
// Calls 'ast_type_free' on each type in the list
void ast_types_free(ast_type_t *types, length_t length);

// ---------------- ast_types_free_fully ----------------
// Calls 'ast_type_free_fully' on each type in the list
void ast_types_free_fully(ast_type_t *types, length_t length);

// ---------------- ast_type_prepend_ptr ----------------
// Prepends a pointer to an AST type
void ast_type_prepend_ptr(ast_type_t *type);

// ---------------- ast_type_pointer_to ----------------
// Takes an AST type and returns it will a pointer prepended
ast_type_t ast_type_pointer_to(ast_type_t type);

// ---------------- ast_type_str ----------------
// Generates a c-string given an AST type
strong_cstr_t ast_type_str(const ast_type_t *type);

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

// ---------------- ast_type_is_polymorph_like_ptr ----------------
// Returns whether an AST type is a pointer to a plain polymorphic type
// or a polymorphic prerequisite type
bool ast_type_is_polymorph_like_ptr(const ast_type_t *type);

// ---------------- ast_type_is_generic_base ----------------
// Returns whether an AST type is a plain generic base type
bool ast_type_is_generic_base(const ast_type_t *type);

// ---------------- ast_type_is_generic_base_ptr ----------------
// Returns whether an AST type is a pointer to a plain generic base type
bool ast_type_is_generic_base_ptr(const ast_type_t *type);

// ---------------- ast_type_is_fixed_array ----------------
// Returns whether an AST type is a fixed array
bool ast_type_is_fixed_array(const ast_type_t *type);

// ---------------- ast_type_is_func ----------------
// Returns whether an AST type is a function pointer
bool ast_type_is_func(const ast_type_t *type);

// ---------------- ast_type_has_polymorph ----------------
// Returns whether an AST type contains a polymorphic type
bool ast_type_has_polymorph(const ast_type_t *type);

// ---------------- ast_type_list_has_polymorph ----------------
// Returns whether a list of AST types contains a polymorphic type
bool ast_type_list_has_polymorph(const ast_type_t *types, length_t length);

// ---------------- ast_type_dereferenced_view ----------------
// Creates a temporary view of a pointer type as if it had been dereferenced
ast_type_t ast_type_dereferenced_view(const ast_type_t *pointer_type);

// ---------------- ast_type_dereference ----------------
// Removes the first pointer element of a pointer type
// NOTE: Assumes inout_type has ownership of AST type elements
// NOTE: Frees memory allocated for the pointer element
void ast_type_dereference(ast_type_t *inout_type);

// ---------------- ast_type_unwrapped_view ----------------
// Returns an AST type that references the internal data of another
// with the first element removed.
// NOTE: The returned type is only valid until the supplied 'type' is
// modified, moved or destroyed
ast_type_t ast_type_unwrapped_view(ast_type_t *type);

// ---------------- ast_type_unwrap_fixed_array ----------------
// Removes the first fixed-array element of a fixed-array type
// NOTE: Assumes inout_type has ownership of AST type elements
// NOTE: Frees memory allocated for the fixed-array element
void ast_type_unwrap_fixed_array(ast_type_t *inout_type);

// ---------------- ast_type_struct_name ----------------
// Returns the struct name for a type (or NULL if not applicable)
maybe_null_weak_cstr_t ast_type_struct_name(const ast_type_t *type);

// ---------------- ast_elem_clone ----------------
// Clones an AST type element, producing a duplicate
ast_elem_t *ast_elem_clone(const ast_elem_t *element);

// ---------------- ast_elems_clone ----------------
// Clones a list of AST type elements
ast_elem_t **ast_elems_clone(ast_elem_t **elements, length_t length);

// ---------------- ast_elem_free ----------------
// Frees a collection of AST type elements
void ast_elems_free(ast_elem_t **elements, length_t elements_length);

// ---------------- ast_elem_free ----------------
// Frees an individual AST type element
void ast_elem_free(ast_elem_t *elem);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_H
