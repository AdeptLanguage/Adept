
#ifndef _ISAAC_PARSE_H
#define _ISAAC_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= parse.h =================================
    Module for parsing a token list into an abstract syntax tree. Also
    responsible for forwarding 'pragma' directives to the pragma module.
    ---------------------------------------------------------------------------
*/

#include "PARSE/parse_ctx.h"

// ------------------ parse ------------------
// Entry point of the parser
// Returns SUCCESS on conventional success (parse completed)
// Returns FAILURE on error
// Returns ALT_FAILURE on unconventional success (parse incomplete)
errorcode_t parse(compiler_t *compiler, object_t *object);

// ------------------ parse_tokens ------------------
// Indirect entry point of the parser. Takes in a
// parsing context that has already been created.
errorcode_t parse_tokens(parse_ctx_t *ctx);

// ------------------ parse_namespace ------------------
// Parses 'namespace' directive
errorcode_t parse_namespace(parse_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_H
