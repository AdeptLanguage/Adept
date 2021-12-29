
#ifndef AST_UTIL_STRING_BUILDER_EXTENSIONS_H
#define AST_UTIL_STRING_BUILDER_EXTENSIONS_H


/*
    ======================= string_builder_extensions.h =======================
    Module for string builder extensions to more easily work with
    abstract syntax tree nodes
    ---------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast_expr.h"
#include "AST/ast_type_lean.h"
#include "UTIL/string_builder.h"

// ---------------- string_builder_append_type ----------------
// Appends an AST type to the string being built by a string builder
void string_builder_append_type(string_builder_t *builder, ast_type_t *ast_type);

// ---------------- string_builder_append_expr ----------------
// Appends an AST expression to the string being built by a string builder
void string_builder_append_expr(string_builder_t *builder, ast_expr_t *ast_expr);

#ifdef __cplusplus
}
#endif

#endif /* AST_UTIL_STRING_BUILDER_EXTENSIONS_H */
