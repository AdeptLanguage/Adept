
#ifndef PARSE_GLOBAL_H
#define PARSE_GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "PARSE/parse_ctx.h"

// ------------------ parse_global ------------------
// Parses a global variable or constant
errorcode_t parse_global(parse_ctx_t *ctx);

// ------------------ parse_constant_global ------------------
// Parses a global constant
errorcode_t parse_constant_global(parse_ctx_t *ctx, char *name, source_t source);

#ifdef __cplusplus
}
#endif

#endif // PARSE_GLOBAL_H
