
#ifndef _ISAAC_PARSE_GLOBAL_H
#define _ISAAC_PARSE_GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast_named_expression.h"
#include "PARSE/parse_ctx.h"
#include "UTIL/ground.h"

// ------------------ parse_global ------------------
// Parses a global variable or global named expression
errorcode_t parse_global(parse_ctx_t *ctx);

// ------------------ parse_named_expression_definition ------------------
// Parses a named expression declaration
errorcode_t parse_named_expression_definition(parse_ctx_t *ctx, ast_named_expression_t *out_named_expression);

// ------------------ parse_global_named_expression_definition ------------------
// Parses a global named expression definition
errorcode_t parse_global_named_expression_definition(parse_ctx_t *ctx);

// ------------------ parse_old_style_named_expression_global ------------------
// Parses an old-style global named expression definition
errorcode_t parse_old_style_named_expression_global(parse_ctx_t *ctx, strong_cstr_t name, source_t source);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_GLOBAL_H
