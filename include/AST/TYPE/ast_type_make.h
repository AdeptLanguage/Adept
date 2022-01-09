
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
void ast_type_make_base(ast_type_t *out_type, strong_cstr_t base);

// ---------------- ast_type_make_base_ptr ----------------
// Makes a pointer-to-simple-base type (e.g. *ubyte, *int, *GameData)
void ast_type_make_base_ptr(ast_type_t *out_type, strong_cstr_t base);

// ---------------- ast_type_make_base_ptr_ptr ----------------
// Makes a pointer-to-pointer-to-simple-base type (e.g. **ubyte, **int)
void ast_type_make_base_ptr_ptr(ast_type_t *out_type, strong_cstr_t base);

// ---------------- ast_type_make_base_with_polymorphs ----------------
// Makes base type with polymorphic parameter subtypes (e.g. <$T> List, <$K, $V> Pair)
// NOTE: Creates copies of strings that are inside 'generics'
void ast_type_make_base_with_polymorphs(ast_type_t *out_type, strong_cstr_t base, weak_cstr_t *generics, length_t length);

// ---------------- ast_type_make_polymorph ----------------
// Makes a polymorphic parameter type (e.g. $T, $K)
void ast_type_make_polymorph(ast_type_t *out_type, strong_cstr_t name, bool allow_auto_conversion);

// ---------------- ast_type_make_func_ptr ----------------
// Makes a function pointer type (e.g. func() void, func(ptr, ptr) int)
void ast_type_make_func_ptr(ast_type_t *out_type, source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership);

// ---------------- ast_type_make_generic_int ----------------
// Creates a generic integer type (no syntactic equivalent - represents the type of an unsuffixed integer)
void ast_type_make_generic_int(ast_type_t *out_type);

// ---------------- ast_type_make_generic_float ----------------
// Creates a generic floating point type (no syntactic equivalent - represents the type of an unsuffixed float)
void ast_type_make_generic_float(ast_type_t *out_type);

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
// Makes a polymorph element (e.g. $T, $K, $V, $InitialierList)
ast_elem_t *ast_elem_polymorph_make(strong_cstr_t name, source_t source, bool allow_auto_conversion);

// ---------------- ast_elem_func_make ----------------
// Makes a function element (e.g. func() void, func(ptr, ptr) int, func(double, double) double)
ast_elem_t *ast_elem_func_make(source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership);

// =====================================================================
// =                              helpers                              =
// =====================================================================

ast_type_t *ast_type_make_polymorph_list(weak_cstr_t *generics, length_t generics_length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_TYPE_MAKE_H
