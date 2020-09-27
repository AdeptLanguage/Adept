
#ifndef _ISAAC_PARSE_NAMESPACE_H
#define _ISAAC_PARSE_NAMESPACE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================ parse_namespace.h ============================
    Module for parsing namespace-related operations
    ---------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_namespace ------------------
// Parses 'namespace' directive
errorcode_t parse_namespace(parse_ctx_t *ctx);

// ------------------ parse_using_namespace ------------------
// Parses 'using namespace' directive
errorcode_t parse_using_namespace(parse_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_NAMESPACE_H
