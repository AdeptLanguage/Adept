
#ifndef _ISAAC_PARSE_CHECKS_H
#define _ISAAC_PARSE_CHECKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast.h"
#include "PARSE/parse_ctx.h"

errorcode_t validate_func_requirements(parse_ctx_t *ctx, ast_func_t *func, source_t source);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_CHECKS_H
