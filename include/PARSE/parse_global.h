
#ifndef _ISAAC_PARSE_GLOBAL_H
#define _ISAAC_PARSE_GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "PARSE/parse_ctx.h"

// ------------------ parse_global ------------------
// Parses a global variable or constant
errorcode_t parse_global(parse_ctx_t *ctx);

// ------------------ parse_constant_global ------------------
// Parses a global constant
errorcode_t parse_constant_global(parse_ctx_t *ctx);

// ------------------ parse_old_style_constant_global ------------------
// Parses a global constant
errorcode_t parse_old_style_constant_global(parse_ctx_t *ctx, weak_cstr_t name, source_t source);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_GLOBAL_H
