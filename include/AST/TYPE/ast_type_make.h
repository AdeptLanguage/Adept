
#ifndef _ISAAC_AST_TYPE_MAKE_H
#define _ISAAC_AST_TYPE_MAKE_H

/*
    ============================= ast_type_make.h ==============================
    Definitions for 'ast_type_make_*' and 'ast_elem_*_make' functions
    ----------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "AST/ast_type.h"

// =====================================================================
// =                          ast_type_make_*                          =
// =====================================================================

// ---------------- ast_type_make_base ----------------
// Makes a simple base type (e.g. void, int, ubyte, GameData, ptr)
ast_type_t ast_type_make_base(strong_cstr_t base);

// ---------------- ast_type_make_base_ptr ----------------
// Makes a pointer-to-simple-base type (e.g. *ubyte, *int, *GameData)
ast_type_t ast_type_make_base_ptr(strong_cstr_t base);

// ---------------- ast_type_make_base_ptr_ptr ----------------
// Makes a pointer-to-pointer-to-simple-base type (e.g. **ubyte, **int)
ast_type_t ast_type_make_base_ptr_ptr(strong_cstr_t base);

// ---------------- ast_type_make_base_with_polymorphs ----------------
// Makes base type with polymorphic parameter subtypes (e.g. <$T> List, <$K, $V> Pair)
// NOTE: Creates copies of strings that are inside 'generics'
ast_type_t ast_type_make_base_with_polymorphs(strong_cstr_t base, weak_cstr_t *generics, length_t length);

// ---------------- ast_type_make_polymorph ----------------
// Makes a polymorphic parameter type (e.g. $T, $K)
ast_type_t ast_type_make_polymorph(strong_cstr_t name, bool allow_auto_conversion);

// ---------------- ast_type_make_polymorph_prereq ----------------
// Make a polymorphic parameter type with a prerequisite (e.g. $T~__number__, $This extends Shape)
// NOTE: 'similarity_prerequisite' may be NULL to indicate no extends requirement
// NOTE: 'maybe_extends' may be zero length to indicate no extends requirement
ast_type_t ast_type_make_polymorph_prereq(strong_cstr_t name, bool allow_auto_conversion, maybe_null_strong_cstr_t similarity_prereq, ast_type_t maybe_extends);

// ---------------- ast_type_make_func_ptr ----------------
// Makes a function pointer type (e.g. func() void, func(ptr, ptr) int)
ast_type_t ast_type_make_func_ptr(source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership);

// ---------------- ast_type_make_generic_int ----------------
// Creates a generic integer type (no syntactic equivalent - represents the type of an unsuffixed integer)
ast_type_t ast_type_make_generic_int();

// ---------------- ast_type_make_generic_float ----------------
// Creates a generic floating point type (no syntactic equivalent - represents the type of an unsuffixed float)
ast_type_t ast_type_make_generic_float();

// =====================================================================
// =                          ast_elem_*_make                          =
// =====================================================================

// ---------------- ast_elem_empty_make ----------------
// Makes an empty ast_elem_t with a given ID and no extra data
ast_elem_t *ast_elem_empty_make(unsigned int id, source_t source);

// ---------------- ast_elem_pointer_make ----------------
// Makes a pointer element (e.g. '*')
#define ast_elem_pointer_make(SOURCE) ast_elem_empty_make(AST_ELEM_POINTER, SOURCE)

// ---------------- ast_elem_generic_int_make ----------------
// Makes a generic int type element (no syntactic equivalent - represents the type of an unsuffixed integer)
#define ast_elem_generic_int_make(SOURCE) ast_elem_empty_make(AST_ELEM_GENERIC_INT, SOURCE)

// ---------------- ast_elem_generic_float_make ----------------
// Makes a generic float type element (no syntactic equivalent - represents the type of an unsuffixed float)
#define ast_elem_generic_float_make(SOURCE) ast_elem_empty_make(AST_ELEM_GENERIC_FLOAT, SOURCE)

// ---------------- ast_elem_base_make ----------------
// Makes a base element (e.g. void, int, ubyte, GameData, ptr)
ast_elem_t *ast_elem_base_make(strong_cstr_t base, source_t source);

// ---------------- ast_elem_generic_base_make ----------------
// Makes a generic base element (e.g. <int> List, <$T> List, <$K, $V> Pair, <float> Array)
// NOTE: Ownership of both the elements and the array 'generics' are taken
ast_elem_t *ast_elem_generic_base_make(strong_cstr_t base, source_t source, ast_type_t *generics, length_t generics_length);

// ---------------- ast_elem_polymorph_make ----------------
// Makes a polymorph element (e.g. $T, $K, $V, $InitializerList)
ast_elem_t *ast_elem_polymorph_make(strong_cstr_t name, source_t source, bool allow_auto_conversion);

// ---------------- ast_elem_polymorph_prereq_make ----------------
// Makes a polymorph prerequisite element (e.g. $T~__number__ or $K~__struct__)
// NOTE: `similarity_prerequisite` may be NULL
// NOTE: `extends` may be zero length to indicate N/A
// NOTE: Takes ownership of `extends`
ast_elem_t *ast_elem_polymorph_prereq_make(strong_cstr_t name, source_t source, bool allow_auto_conversion, maybe_null_strong_cstr_t similarity_prerequisite, ast_type_t extends);

// ---------------- ast_elem_func_make ----------------
// Makes a function element (e.g. func() void, func(ptr, ptr) int, func(double, double) double)
ast_elem_t *ast_elem_func_make(source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership);

// ---------------- ast_elem_fixed_array_make ----------------
// Makes a fixed array element (e.g. 10 or 8)
ast_elem_t *ast_elem_fixed_array_make(source_t source, length_t count);

// ---------------- ast_elem_var_fixed_array_make ----------------
// Makes a variably-sized fixed array element (e.g. [DEFAULT_CONTAINER_SIZE] or [100] or [#get b2_max_vertices])
ast_elem_t *ast_elem_var_fixed_array_make(source_t source, ast_expr_t *length);

// =====================================================================
// =                              helpers                              =
// =====================================================================

ast_type_t *ast_type_make_polymorph_list(weak_cstr_t *generics, length_t generics_length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_MAKE_H
