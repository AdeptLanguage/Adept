
#ifndef _ISAAC_PARSE_ENUM_H
#define _ISAAC_PARSE_ENUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "PARSE/parse_ctx.h"
#include "UTIL/ground.h"

// ------------------ parse_enum ------------------
// Parses a enum
errorcode_t parse_enum(parse_ctx_t *ctx, bool is_foreign);

// ------------------ parse_enum_body ------------------
// Parses the body of a enum
errorcode_t parse_enum_body(parse_ctx_t *ctx, char ***kinds, length_t *length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_ENUM_H
