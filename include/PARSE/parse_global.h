
#ifndef PARSE_GLOBAL_H
#define PARSE_GLOBAL_H

#include "PARSE/parse_ctx.h"

// ------------------ parse_global ------------------
// Parses a global variable or constant
int parse_global(parse_ctx_t *ctx);

// ------------------ parse_constant_global ------------------
// Parses a global constant
int parse_constant_global(parse_ctx_t *ctx, char *name, source_t source);

#endif // PARSE_GLOBAL_H