
#ifndef _ISAAC_AST_DUMP_H
#define _ISAAC_AST_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    =============================== ast_dump.h ===============================
    Module for dumping abstract syntax trees
    --------------------------------------------------------------------------
*/

#include <stdio.h>

#include "AST/ast.h"
#include "AST/ast_expr_lean.h"
#include "UTIL/ground.h"

// ---------------- ast_dump ----------------
// Converts an AST to a string representation
// and then dumps it to a file
void ast_dump(ast_t *ast, const char *filename);

// ---------------- ast_dump_* ----------------
// Writes a specific part of an AST to a file
void ast_dump_funcs(FILE *file, ast_func_t *functions, length_t functions_length);
void ast_dump_stmts_list(FILE *file, ast_expr_list_t *statements, length_t indentation);
void ast_dump_stmts(FILE *file, ast_expr_t **statements, length_t length, length_t indentation);
void ast_dump_composites(FILE *file, ast_composite_t *composites, length_t composites_length);
void ast_dump_composite(FILE *file, ast_composite_t *composite, length_t additional_indentation);
void ast_dump_globals(FILE *file, ast_global_t *globals, length_t globals_length);
void ast_dump_enums(FILE *file, ast_enum_t *enums, length_t enums_length);
void ast_dump_aliases(FILE *file, ast_alias_t *aliases, length_t length);
void ast_dump_libraries(FILE *file, strong_cstr_t *libraries, length_t length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_DUMP_H
