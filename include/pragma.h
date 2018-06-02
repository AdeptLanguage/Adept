
#ifndef PRAGMA_H
#define PRAGMA_H

/*
    ================================= pragma.h =================================
    Module for handling 'pragma' directives sent from code being compiled
    ----------------------------------------------------------------------------
*/

#include "parse.h"
#include "ground.h"

// ---------------- parse_pragma ----------------
// Handles parsing and interpretation of pragma directions
// Expects 'ctx->i' to point to 'pragma' keyword
int parse_pragma(parse_ctx_t *ctx);

#endif // PRAGMA_H
