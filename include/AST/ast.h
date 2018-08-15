
#ifndef AST_H
#define AST_H

/*
    ================================== ast.h ==================================
    Module for creating and manipulating abstract syntax trees
    ---------------------------------------------------------------------------
*/

#include "UTIL/trait.h"
#include "UTIL/ground.h"
#include "AST/ast_type.h"
#include "AST/ast_expr.h"

// ---------------- ast_func_t ----------------
// A function within the root AST
typedef struct {
    char *name;
    char **arg_names;
    ast_type_t *arg_types;
    source_t *arg_sources;
    char *arg_flows; // in | out | inout
    length_t arity;
    ast_type_t return_type;
    trait_t traits;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
    source_t source;
} ast_func_t;

// Possible AST function traits
#define AST_FUNC_FOREIGN TRAIT_1
#define AST_FUNC_VARARG  TRAIT_2
#define AST_FUNC_MAIN    TRAIT_3
#define AST_FUNC_STDCALL TRAIT_4

// ---------------- ast_struct_t ----------------
// A structure within the root AST
typedef struct {
    char *name;
    char **field_names;
    ast_type_t *field_types;
    length_t field_count;
    trait_t traits;
    source_t source;
} ast_struct_t;

// Possible AST structure traits
#define AST_STRUCT_PACKED TRAIT_1

// ---------------- ast_alias_t ----------------
// A type alias within the root AST
typedef struct {
    const char *name;
    ast_type_t type;
    trait_t traits;
    source_t source;
} ast_alias_t;

// ---------------- ast_constant_t ----------------
// A global constant expression within the root AST
typedef struct {
    const char *name;
    ast_expr_t *expression;
    trait_t traits;
    source_t source;
} ast_constant_t;

// ---------------- ast_global_t ----------------
// A global variable within the root AST
typedef struct {
    const char *name;
    ast_type_t type;
    ast_expr_t *initial;
    source_t source;
} ast_global_t;

// ---------------- ast_t ----------------
// The root AST
typedef struct {
    ast_func_t *funcs;
    length_t funcs_length;
    length_t funcs_capacity;
    ast_struct_t *structs;
    length_t structs_length;
    length_t structs_capacity;
    ast_alias_t *aliases;
    length_t aliases_length;
    length_t aliases_capacity;
    ast_constant_t* constants;
    length_t constants_length;
    length_t constants_capacity;
    ast_global_t *globals;
    length_t globals_length;
    length_t globals_capacity;
    char** libraries;
    length_t libraries_length;
    length_t libraries_capacity;
} ast_t;

// ---------------- ast_init ----------------
// Initializes an AST
void ast_init(ast_t *ast);

// ---------------- ast_free ----------------
// Frees data within an AST
void ast_free(ast_t *ast);

// ---------------- ast_free_* ----------------
// Frees a specific part of the data within an AST
void ast_free_functions(ast_func_t *functions, length_t functions_length);
void ast_free_statements(ast_expr_t **statements, length_t length);
void ast_free_statements_fully(ast_expr_t **statements, length_t length);
void ast_free_structs(ast_struct_t *structs, length_t structs_length);
void ast_free_constants(ast_constant_t *constants, length_t constants_length);
void ast_free_globals(ast_global_t *globals, length_t globals_length);

// ---------------- ast_dump ----------------
// Converts an AST to a string representation
// and then dumps it to a file
void ast_dump(ast_t *ast, const char *filename);

// ---------------- ast_dump_* ----------------
// Writes a specific part of an AST to a file
void ast_dump_functions(FILE *file, ast_func_t *functions, length_t functions_length);
void ast_dump_statements(FILE *file, ast_expr_t **statements, length_t length, length_t indentation);
void ast_dump_structs(FILE *file, ast_struct_t *structs, length_t structs_length);
void ast_dump_globals(FILE *file, ast_global_t *globals, length_t globals_length);

// ---------------- ast_func_create_template ----------------
// Fills out a blank template for a new function
void ast_func_create_template(ast_func_t *func, char *name, bool is_stdcall, bool is_foreign, source_t source);

// ---------------- ast_struct_find ----------------
// Finds a structure by name
ast_struct_t *ast_struct_find(ast_t *ast, char *name);

// ---------------- ast_struct_find_field ----------------
// Finds a field by name within a structure
bool ast_struct_find_field(ast_struct_t *ast_struct, char *name, length_t *out_index);

// ---------------- find_alias ----------------
// Finds an alias by name
int find_alias(ast_alias_t *aliases, length_t aliases_length, const char *alias);

// ---------------- find_constant ----------------
// Finds a global constant expression by name
int find_constant(ast_constant_t *constants, length_t constants_length, const char *constant);

// ---------------- ast_add_foreign_library ----------------
// Adds a library to the list of foreign libraries
// NOTE: Does not have ownership of library string
void ast_add_foreign_library(ast_t *ast, char *library);

// ---------------- ast_aliases_cmp ----------------
// Compares two 'ast_alias_t' structures.
// Used for qsort()
int ast_aliases_cmp(const void *a, const void *b);

// ---------------- ast_constants_cmp ----------------
// Compares two 'ast_constant_t' structures.
// Used for qsort()
int ast_constants_cmp(const void *a, const void *b);

#endif // AST_H
