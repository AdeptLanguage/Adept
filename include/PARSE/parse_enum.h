
#ifndef PARSE_ENUM_H
#define PARSE_ENUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_enum ------------------
// Parses a enum
errorcode_t parse_enum(parse_ctx_t *ctx);

// ------------------ parse_enum_body ------------------
// Parses the body of a enum
errorcode_t parse_enum_body(parse_ctx_t *ctx, char ***kinds, length_t *length);

#ifdef __cplusplus
}
#endif

#endif // PARSE_ENUM_H
