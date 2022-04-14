
#ifndef _ISAAC_PARSE_EXPR_H
#define _ISAAC_PARSE_EXPR_H

#include <stdbool.h>

#include "AST/ast_expr.h"
#include "PARSE/parse_ctx.h"
#include "UTIL/ground.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------ parse_expr ------------------
// Parses an expression into an AST.
errorcode_t parse_expr(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_primary_expr ------------------
// Parses the concrete part of an expression into an AST.
// Primarily called from 'parse_expr'.
errorcode_t parse_primary_expr(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_post ------------------
// Handles [] and '.' operators of a primary expression
// Primarily called from 'parse_primary_expr'
errorcode_t parse_expr_post(parse_ctx_t *ctx, ast_expr_t **inout_expr);

// ------------------ parse_op_expr ------------------
// Parses the non-concrete parts of an expression that
// follows the concrete part. This function handles
// expression precedence and most operators.
// Primarily called from 'parse_expr'.
errorcode_t parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t** inout_left, bool keep_mutable);

// ------------------ parse_rhs_expr ------------------
// Parses the right hand side of an operator that takes
// two operands. This is a sub-part of 'parse_op_expr'.
// Primarily called from 'parse_op_expr'.
errorcode_t parse_rhs_expr(parse_ctx_t *ctx, ast_expr_t **left, ast_expr_t **out_right, int op_prec);

// ------------------ parse_expr_word ------------------
// Parses an expression that starts with a word
errorcode_t parse_expr_word(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_call ------------------
// Parses a call expression
// If 'allow_tentative' is true, then the call expression is allowed to be tentative (used for statements)
errorcode_t parse_expr_call(parse_ctx_t *ctx, ast_expr_t **out_expr, bool allow_tentative);

// ------------------ parse_expr_arguments ------------------
// Parses the arguments to a call
// NOTE: 'out_capacity' may be NULL
// NOTE: Should start inside the parantheses, with the first argument
errorcode_t parse_expr_arguments(parse_ctx_t *ctx, ast_expr_t ***out_args, length_t *out_arity, length_t *out_capacity);

// ------------------ parse_expr_enum_value ------------------
// Parses an enum value
errorcode_t parse_expr_enum_value(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_address ------------------
// Parses an address expression
errorcode_t parse_expr_address(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_func_address ------------------
// Parses a function address expression
errorcode_t parse_expr_func_address(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_dereference ------------------
// Parses a dereference expression
errorcode_t parse_expr_dereference(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_cast ------------------
// Parses a 'cast' expression
errorcode_t parse_expr_cast(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_as ------------------
// Parses a 'as' expression
errorcode_t parse_expr_as(parse_ctx_t *ctx, ast_expr_t **inout_expr);

// ------------------ parse_expr_at ------------------
// Parses a 'at' expression
errorcode_t parse_expr_at(parse_ctx_t *ctx, ast_expr_t **inout_expr);

// ------------------ parse_expr_sizeof ------------------
// Parses a 'sizeof' expression
errorcode_t parse_expr_sizeof(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_alignof ------------------
// Parses a 'alignof' expression
errorcode_t parse_expr_alignof(parse_ctx_t *ctx, ast_expr_t **out_expr);


// ------------------ parse_expr_unary ------------------
// Parses an unary expression
errorcode_t parse_expr_unary(parse_ctx_t *ctx, unsigned int expr_id, ast_expr_t **out_expr);

// ------------------ parse_expr_new ------------------
// Parses a 'new' expression
errorcode_t parse_expr_new(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_static ------------------
// Parses a 'static' expression
errorcode_t parse_expr_static(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_def ------------------
// Parses a 'def' expression
errorcode_t parse_expr_def(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_typeinfo ------------------
// Parses a 'typeinfo' expression
errorcode_t parse_expr_typeinfo(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_preincrement ------------------
// Parses a pre-increment expression
errorcode_t parse_expr_preincrement(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_predecrement ------------------
// Parses a pre-decrement expression
errorcode_t parse_expr_predecrement(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_expr_initlist ------------------
// Parses an initializer list expression
errorcode_t parse_expr_initlist(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_get_precedence ------------------
// Returns the precedence of the expression that will
// be created by a given token id.
int parse_get_precedence(unsigned int id);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_EXPR_H
