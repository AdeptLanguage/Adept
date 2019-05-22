
#ifndef PARSE_STMT_H
#define PARSE_STMT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "AST/ast_expr.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_stmts ------------------
// Parses one or more statements into two lists.
// 'expr_list' for standard statements
// 'defer_list' for defered statements
errorcode_t parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *expr_list, ast_expr_list_t *defer_list, trait_t mode);

// Possible modes for 'parse_stmts'
#define PARSE_STMTS_STANDARD TRAIT_NONE // Standard mode (will parse multiple statements)
#define PARSE_STMTS_SINGLE   TRAIT_1    // Single statement mode (will parse a single statement)

// ------------------ parse_stmt_call ------------------
// Parses a function call statement
errorcode_t parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *expr_list, bool is_tentative);

// ------------------ parse_stmt_declare ------------------
// Parses a variable declaration statement
errorcode_t parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *expr_list);

// ------------------ parse_unravel_defer_stmts ------------------
// Unravels a list of defered statements into another list of
// statements in reverse order until 'unravel_point' is reached.
void parse_unravel_defer_stmts(ast_expr_list_t *stmts, ast_expr_list_t *defer_list, length_t unravel_point);

#ifdef __cplusplus
}
#endif

#endif // PARSE_STMT_H
