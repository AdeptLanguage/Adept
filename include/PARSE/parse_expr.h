
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
// Primary called from 'parse_expr'.
int parse_primary_expr(parse_ctx_t *ctx, ast_expr_t **out_expr);

// ------------------ parse_op_expr ------------------
// Parses the non-concrete parts of an expression that
// follows the concrete part. This function handles
// expression precedence and most operators.
// Primary called from 'parse_expr'.
int parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t** inout_left, bool keep_mutable);


// ------------------ parse_rhs_expr ------------------
// Parses the right hand side of an operator that takes
// two operands. This is a sub-part of 'parse_op_expr'.
// Primary called from 'parse_op_expr'.
int parse_rhs_expr(parse_ctx_t *ctx, ast_expr_t **left, ast_expr_t **out_right, int op_prec);

// ------------------ parse_get_precedence ------------------
// Returns the precedence of the expression that will
// be created by a given token id.
int parse_get_precedence(unsigned int id);

#endif // PARSE_EXPR_H
