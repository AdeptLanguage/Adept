
#ifndef PRAGMA_H
#define PRAGMA_H

#ifdef __cplusplus
extern "C" {
#endif
/*
    ================================= pragma.h =================================
    Module for handling 'pragma' directives sent from code being compiled
    ----------------------------------------------------------------------------
*/

#include "LEX/token.h"
#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ---------------- parse_pragma ----------------
// Handles parsing and interpretation of pragma directions
// Expects 'ctx->i' to point to 'pragma' keyword
errorcode_t parse_pragma(parse_ctx_t *ctx);

// ---------------- parse_pragma_cloptions ----------------
// Parses the 'options' pragma directive's command line
// arguments string and passes it off to the compiler
errorcode_t parse_pragma_cloptions(parse_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // PRAGMA_H
