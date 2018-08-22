
#ifndef PARSE_EXPR_H
#define PARSE_EXPR_H

#include "UTIL/ground.h"
#include "AST/ast_expr.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_expr ------------------
// Parses an expression into an AST.
int parse_expr(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_primary_expr ------------------
// Parses the concrete part of an expression into an AST.
// Primarily called from 'parse_expr'.
int parse_primary_expr(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_post ------------------
// Handles [] and '.' operators of a primary expression
// Primarily called from 'parse_primary_expr'
int parse_expr_post(parse_ctx_t *ctx, ast_expr_t **inout_expr);

// ------------------ parse_op_expr ------------------
// Parses the non-concrete parts of an expression that
// follows the concrete part. This function handles
// expression precedence and most operators.
// Primarily called from 'parse_expr'.
int parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t** inout_left, bool keep_mutable);

// ------------------ parse_rhs_expr ------------------
// Parses the right hand side of an operator that takes
// two operands. This is a sub-part of 'parse_op_expr'.
// Primarily called from 'parse_op_expr'.
int parse_rhs_expr(parse_ctx_t *ctx, ast_expr_t **left, ast_expr_t **out_right, int op_prec);

// ------------------ parse_expr_word ------------------
// Parses an expression that starts with a word
int parse_expr_word(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_call ------------------
// Parses a call expression
int parse_expr_call(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_address ------------------
// Parses an address expression
int parse_expr_address(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_func_address ------------------
// Parses a function address expression
int parse_expr_func_address(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_dereference ------------------
// Parses a dereference expression
int parse_expr_dereference(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_cast ------------------
// Parses a 'cast' expression
int parse_expr_cast(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_sizeof ------------------
// Parses a 'sizeof' expression
int parse_expr_sizeof(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_not ------------------
// Parses a 'not' expression
int parse_expr_not(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_new ------------------
// Parses a 'new' expression
int parse_expr_new(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_get_precedence ------------------
// Returns the precedence of the expression that will
// be created by a given token id.
int parse_get_precedence(unsigned int id);

#endif // PARSE_EXPR_H
