
#ifndef PRAGMA_H
#define PRAGMA_H

/*
    ================================= pragma.h =================================
    Module for handling 'pragma' directives sent from code being compiled
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "PARSE/parse.h"

// ---------------- parse_pragma ----------------
// Handles parsing and interpretation of pragma directions
// Expects 'ctx->i' to point to 'pragma' keyword
int parse_pragma(parse_ctx_t *ctx);

// ---------------- parse_pragma_string ----------------
// Returns a string that is after the current token.
// Returns NULL if syntax error.
char* parse_pragma_string(token_t *tokens, length_t *i);

// ---------------- parse_pragma_cloptions ----------------
// Parses the 'options' pragma directive's command line
// arguments string and passes it off to the compiler
int parse_pragma_cloptions(parse_ctx_t *ctx);

#endif // PRAGMA_H
