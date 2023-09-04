
#ifndef _ISAAC_AST_H
#define _ISAAC_AST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================== ast.h ==================================
    Module for creating and manipulating abstract syntax trees
    ---------------------------------------------------------------------------
*/

#include <stdbool.h>
#include <stdio.h>

#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_named_expression.h"
#include "AST/ast_type.h"
#include "AST/meta_directives.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/index_id_list.h"
#include "UTIL/trait.h"

struct compiler;

// ---------------- ast_func_t ----------------
// A function within the root AST
typedef struct {
    strong_cstr_t name;
    strong_cstr_t *arg_names;
    ast_type_t *arg_types;
    source_t *arg_sources;
    char *arg_flows; // in | out | inout
    trait_t *arg_type_traits;
    ast_expr_t **arg_defaults; // maybe null array of maybe null pointers
    length_t arity;
    ast_type_t return_type;
    trait_t traits;
    strong_cstr_t variadic_arg_name;
    source_t variadic_source;
    ast_expr_list_t statements;
    source_t source;
    maybe_null_strong_cstr_t export_as;
    length_t instantiation_depth;
    
    union {
        func_id_t virtual_origin; // can be INVALID_FUNC_ID
        func_id_t virtual_dispatcher; // can be INVALID_FUNC_ID
    };

    #ifdef ADEPT_INSIGHT_BUILD
    source_t end_source;
    #endif
} ast_func_t;

// ---------------- ast_func_alias_t ----------------
// A function redirection within the root AST
typedef struct {
    strong_cstr_t from;
    weak_cstr_t to;
    ast_type_t *arg_types;
    length_t arity;
    trait_t required_traits;
    source_t source;
    bool match_first_of_name;
} ast_func_alias_t;

#define AST_FUNC_ARG_TYPE_TRAIT_POD TRAIT_1

// Possible AST function traits
#define AST_FUNC_FOREIGN                TRAIT_1
#define AST_FUNC_VARARG                 TRAIT_2
#define AST_FUNC_MAIN                   TRAIT_3
#define AST_FUNC_STDCALL                TRAIT_4
#define AST_FUNC_POLYMORPHIC            TRAIT_5
#define AST_FUNC_GENERATED              TRAIT_6
#define AST_FUNC_DEFER                  TRAIT_7
#define AST_FUNC_PASS                   TRAIT_8
#define AST_FUNC_AUTOGEN                TRAIT_9
#define AST_FUNC_VARIADIC               TRAIT_A
#define AST_FUNC_IMPLICIT               TRAIT_B
#define AST_FUNC_WINMAIN                TRAIT_C
#define AST_FUNC_NO_DISCARD             TRAIT_D
#define AST_FUNC_DISALLOW               TRAIT_E
#define AST_FUNC_VIRTUAL                TRAIT_F
#define AST_FUNC_OVERRIDE               TRAIT_G
#define AST_FUNC_USED_OVERRIDE          TRAIT_2_1
#define AST_FUNC_NO_SUGGEST             TRAIT_2_2
#define AST_FUNC_DISPATCHER             TRAIT_2_3
#define AST_FUNC_CLASS_CONSTRUCTOR      TRAIT_2_4
#define AST_FUNC_WARN_BAD_PRINTF_FORMAT TRAIT_2_5
#define AST_FUNC_INIT                   TRAIT_2_6
#define AST_FUNC_DEINIT                 TRAIT_2_7

// ------------------ ast_func_prefixes_t ------------------
// Information about the keywords that prefix a function
typedef struct {
    bool is_stdcall  : 1,
         is_verbatim : 1,
         is_implicit : 1,
         is_external : 1,
         is_virtual  : 1,
         is_override : 1;
} ast_func_prefixes_t;

// ------------------ ast_func_head_t ------------------
// Information about the head of function declaration
typedef struct {
    strong_cstr_t name;
    source_t source;
    bool is_foreign : 1,
         is_entry   : 1;
    ast_func_prefixes_t prefixes;
    maybe_null_strong_cstr_t export_name;
} ast_func_head_t;

// ---------------- DERIVE_AST_COMPOSITE ----------------
// Common fields for all ast_composite_*_t derivatives
// NOTE: `parent` may be AST_TYPE_NONE
#define DERIVE_AST_COMPOSITE struct { \
    strong_cstr_t name; \
    ast_layout_t layout; \
    source_t source; \
    ast_type_t parent; \
    bool is_polymorphic : 1; \
    bool is_class : 1; \
    bool has_constructor : 1;\
}

// ---------------- ast_composite_t ----------------
// A structure/union within the root AST
typedef struct {
    DERIVE_AST_COMPOSITE;
} ast_composite_t;

// ---------------- ast_poly_composite_t ----------------
// A polymorphic composite
// Guaranteed to overlap with 'ast_composite_t'
typedef struct {
    DERIVE_AST_COMPOSITE;
    // ---------------------------
    strong_cstr_t *generics;
    length_t generics_length;
} ast_poly_composite_t;

// ---------------- ast_alias_t ----------------
// A type alias within the root AST
typedef struct {
    strong_cstr_t name;
    ast_type_t type;
    trait_t traits;
    source_t source;
} ast_alias_t;

// ---------------- ast_global_t ----------------
// A global variable within the root AST
typedef struct {
    strong_cstr_t name;
    length_t name_length;
    ast_type_t type;
    ast_expr_t *initial;
    trait_t traits;
    source_t source;
} ast_global_t;

// Possible ast_global_t traits
#define AST_GLOBAL_POD                   TRAIT_1
#define AST_GLOBAL_EXTERNAL              TRAIT_2
#define AST_GLOBAL_THREAD_LOCAL          TRAIT_3
#define AST_GLOBAL_SPECIAL               TRAIT_4
#define AST_GLOBAL___TYPES__             TRAIT_5 // Sub-trait of AST_GLOBAL_SPECIAL
#define AST_GLOBAL___TYPES_LENGTH__      TRAIT_6 // Sub-trait of AST_GLOBAL_SPECIAL
#define AST_GLOBAL___TYPE_KINDS__        TRAIT_7 // Sub-trait of AST_GLOBAL_SPECIAL
#define AST_GLOBAL___TYPE_KINDS_LENGTH__ TRAIT_8 // Sub-trait of AST_GLOBAL_SPECIAL

// ---------------- ast_enum_t ----------------
// An enum AST node
typedef struct {
    weak_cstr_t name;
    weak_cstr_t *kinds;
    length_t length;
    source_t source;
} ast_enum_t;

typedef struct {
    ast_type_t ast_int_type;
    ast_type_t ast_usize_type;
    ast_type_t *ast_variadic_array;
    source_t ast_variadic_source; // Only exists if 'ast_variadic_array' isn't NULL
    ast_type_t *ast_initializer_list;
    source_t ast_initializer_list_source; // Only exists if 'ast_initializer_list' isn't NULL
} ast_shared_common_t;

typedef struct {
    weak_cstr_t name;
    func_id_t ast_func_id;
    signed char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ast_poly_func_t;

// ---------------- ast_t ----------------
// The root AST
typedef struct {
    ast_func_t *funcs;
    length_t funcs_length;
    length_t funcs_capacity;
    ast_func_alias_t *func_aliases;
    length_t func_aliases_length;
    length_t func_aliases_capacity;
    ast_composite_t *composites;
    length_t composites_length;
    length_t composites_capacity;
    ast_alias_t *aliases;
    length_t aliases_length;
    length_t aliases_capacity;
    ast_named_expression_list_t named_expressions;
    ast_global_t *globals;
    length_t globals_length;
    length_t globals_capacity;
    ast_enum_t *enums;
    length_t enums_length;
    length_t enums_capacity;
    strong_cstr_t *libraries;
    char *library_kinds;
    length_t libraries_length;
    length_t libraries_capacity;
    ast_shared_common_t common;

    // Data members for meta definitions
    meta_definition_t *meta_definitions;
    length_t meta_definitions_length;
    length_t meta_definitions_capacity;

    // Polymorphic functions (eventually sorted)
    ast_poly_func_t *poly_funcs;
    length_t poly_funcs_length;
    length_t poly_funcs_capacity;

    // A second list of polymorphic functions that only contains methods
    ast_poly_func_t *polymorphic_methods;
    length_t polymorphic_methods_length;
    length_t polymorphic_methods_capacity;

    // Polymorphic composites
    ast_poly_composite_t *poly_composites;
    length_t poly_composites_length;
    length_t poly_composites_capacity;
} ast_t;

#define LIBRARY_KIND_NONE           0x00
#define LIBRARY_KIND_LIBRARY        0x01
#define LIBRARY_KIND_FRAMEWORK      0x02

// ---------------- ast_init ----------------
// Initializes an AST
void ast_init(ast_t *ast, unsigned int cross_compile_for);

// ---------------- ast_free ----------------
// Frees data within an AST
void ast_free(ast_t *ast);

// ---------------- ast_free_* ----------------
// Frees a specific part of the data within an AST
void ast_free_functions(ast_func_t *functions, length_t functions_length);
void ast_free_function_aliases(ast_func_alias_t *faliases, length_t length);
void ast_free_composites(ast_composite_t *composites, length_t composites_length);
void ast_free_aliases(ast_alias_t *aliases, length_t aliases_length);
void ast_free_globals(ast_global_t *globals, length_t globals_length);
void ast_free_enums(ast_enum_t *enums, length_t enums_length);

// ---------------- ast_func_is_method ----------------
// Returns whether an AST function is method-like
// (considered so if it has a first parameter name of 'this')
bool ast_func_is_method(ast_func_t *func);

// ---------------- ast_method_get_subject_typename ----------------
// Gets the typename of the subject of a method
// NOTE: Assumes that 'ast_func_is_method(method)' is true
// NOTE: Returns NULL if subject type is not compatible
maybe_null_weak_cstr_t ast_method_get_subject_typename(ast_func_t *method);

// ---------------- ast_func_args_str ----------------
// Create a string of the inside of the parentheses for the
// arguments of a function declaration
strong_cstr_t ast_func_args_str(ast_func_t *func);

// ---------------- ast_func_head_str ----------------
// Creates a string for displaying the signature
// of the head of a function
strong_cstr_t ast_func_head_str(ast_func_t *func);

// ---------------- ast_func_new ----------------
// Allocates a new uninitialized AST function and
// returns an id that can be used to access it
func_id_t ast_new_func(ast_t *ast);

// ---------------- ast_func_create_template ----------------
// Fills out a blank template for a new function
void ast_func_create_template(struct compiler *compiler, ast_func_t *func, const ast_func_head_t *options);

// ---------------- ast_func_has_polymorphic_signature ----------------
// Returns whether an AST function has polymorphic arguments or return type
bool ast_func_has_polymorphic_signature(ast_func_t *func);

// ---------------- ast_alias_init ----------------
// Initializes an AST alias
void ast_alias_init(ast_alias_t *alias, weak_cstr_t name, ast_type_t type, trait_t traits, source_t source);

// ---------------- ast_enum_init ----------------
// Initializes an AST enum
void ast_enum_init(ast_enum_t *enum_definition, weak_cstr_t name, weak_cstr_t *kinds, length_t length, source_t source);

// ---------------- ast_composite_find_exact ----------------
// Finds a composite by its exact name
ast_composite_t *ast_composite_find_exact(ast_t *ast, const char *name);

// ---------------- ast_poly_composite_find_exact ----------------
// Finds a polymorphic composite by its exact name
ast_poly_composite_t *ast_poly_composite_find_exact(ast_t *ast, const char *name);

// ---------------- ast_find_composite ----------------
// Finds a composite (polymorphic or not) using an AST type
// Returns NULL if no such composite exists
ast_composite_t *ast_find_composite(ast_t *ast, const ast_type_t *type);

// ---------------- ast_composite_find_exact_field ----------------
// Finds a field by name within a composite
successful_t ast_composite_find_exact_field(ast_composite_t *composite, const char *name, ast_layout_endpoint_t *out_endpoint, ast_layout_endpoint_path_t *out_path);

// ---------------- ast_enum_find_kind ----------------
// Finds a kind by name within an enum
successful_t ast_enum_find_kind(ast_enum_t *ast_enum, const char *kind_name, length_t *out_index);

// ---------------- ast_enum_contains ----------------
// Returns whether an AST enum contains a kind with the given name
bool ast_enum_contains(ast_enum_t *ast_enum, const char *kind_name);

// ---------------- ast_find_alias ----------------
// Finds an alias by name
maybe_index_t ast_find_alias(ast_alias_t *aliases, length_t aliases_length, const char *alias);

// ---------------- ast_find_enum ----------------
// Finds a enum expression by name
maybe_index_t ast_find_enum(ast_enum_t *enums, length_t enums_length, const char *enum_name);

// ---------------- ast_find_global ----------------
// Finds a global variable by name
// NOTE: Requires that 'globals' is sorted
maybe_index_t ast_find_global(ast_global_t *globals, length_t globals_length, weak_cstr_t name);

// ---------------- ast_func_end_is_reachable ----------------
// Checks whether its possible to execute every statement in a function
// and still have not returned
bool ast_func_end_is_reachable(ast_t *ast, func_id_t ast_func_id);
bool ast_func_end_is_reachable_inner(ast_expr_list_t *stmts, unsigned int max_depth, unsigned int depth);

// ---------------- ast_add_alias ----------------
// Adds a type alias to the global scope of an AST
void ast_add_alias(ast_t *ast, strong_cstr_t name, ast_type_t strong_type, trait_t traits, source_t source);

// ---------------- ast_add_enum ----------------
// Adds an enum to the global scope of an AST
void ast_add_enum(ast_t *ast, weak_cstr_t name, weak_cstr_t *kinds, length_t length, source_t source);

// ---------------- ast_add_global_named_expression ----------------
// Adds a named expression to the global scope of an AST
void ast_add_global_named_expression(ast_t *ast, ast_named_expression_t named_expression);

// ---------------- ast_add_poly_func ----------------
// Adds a function to the list of polymorphic functions for an AST
void ast_add_poly_func(ast_t *ast, weak_cstr_t func_name_persistent, func_id_t ast_func_id);

// ---------------- ast_add_composite ----------------
// Adds a composite to the global scope of an AST
// NOTE: 'maybe_parent' may be 'AST_TYPE_NONE'
ast_composite_t *ast_add_composite(
    ast_t *ast,
    strong_cstr_t name,
    ast_layout_t layout,
    source_t source,
    ast_type_t maybe_parent,
    bool is_class
);

// ---------------- ast_add_poly_composite ----------------
// Adds a polymorphic composite to the global scope of an AST
// NOTE: 'maybe_parent' may be 'AST_TYPE_NONE'
ast_poly_composite_t *ast_add_poly_composite(
    ast_t *ast,
    strong_cstr_t name,
    ast_layout_t layout,
    source_t source,
    ast_type_t maybe_parent,
    bool is_class,
    strong_cstr_t *generics,
    length_t generics_length
);

// ---------------- ast_add_global ----------------
// Adds a global variable to the global scope of an AST
void ast_add_global(ast_t *ast, weak_cstr_t name, ast_type_t type, ast_expr_t *initial_value, trait_t traits, source_t source);

// ---------------- ast_add_foreign_library ----------------
// Adds a library to the list of foreign libraries
// NOTE: Takes ownership of library string
void ast_add_foreign_library(ast_t *ast, strong_cstr_t library, char kind);

// ---------------- va_args_inject_ast ----------------
// Injects va_* definitions into AST
void va_args_inject_ast(struct compiler *compiler, ast_t *ast);

// ---------------- ast_aliases_cmp ----------------
// Compares two 'ast_alias_t' structures.
// Used for qsort()
int ast_aliases_cmp(const void *a, const void *b);

// ---------------- ast_enums_cmp ----------------
// Compares two 'ast_enum_t' structures.
// Used for qsort()
int ast_enums_cmp(const void *a, const void *b);

// ---------------- ast_poly_funcs_cmp ----------------
// Compares two 'ast_func_t*' structures by name.
// Used for qsort()
int ast_poly_funcs_cmp(const void *a, const void *b);

// ---------------- ast_globals_cmp ----------------
// Compares two 'ast_global_t*' structures by name
int ast_globals_cmp(const void *a, const void *b);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_H
