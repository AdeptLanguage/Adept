
#ifndef PARSE_META_H
#define PARSE_META_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"
#include "AST/meta_directives.h"

errorcode_t parse_meta(parse_ctx_t *ctx);
errorcode_t parse_meta_expr(parse_ctx_t *ctx, meta_expr_t **out_expr);
errorcode_t parse_meta_primary_expr(parse_ctx_t *ctx, meta_expr_t **out_expr);
errorcode_t parse_meta_op_expr(parse_ctx_t *ctx, int precedence, meta_expr_t **inout_left);
errorcode_t meta_parse_rhs_expr(parse_ctx_t *ctx, meta_expr_t **left, meta_expr_t **out_right, int op_prec);

#ifdef __cplusplus
}
#endif

#endif
