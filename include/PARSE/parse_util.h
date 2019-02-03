
#ifndef PARSE_UTIL_H
#define PARSE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_ignore_newlines ------------------
// Passes over newlines until something else is encountered
errorcode_t parse_ignore_newlines(parse_ctx_t *ctx, const char *error_message);

// ------------------ parse_panic_token ------------------
// Will print an error message with general information
// using the message 'message' and replacing '%s' within
// it with the name of the token pointed to by 'source'
void parse_panic_token(parse_ctx_t *ctx, source_t source, unsigned int token_id, const char *message);

#ifdef __cplusplus
}
#endif

#endif // PARSE_UTIL_H
