
#ifndef _ISAAC_PARSE_STRUCT_H
#define _ISAAC_PARSE_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_struct ------------------
// Parses a struct
errorcode_t parse_struct(parse_ctx_t *ctx);

// ------------------ parse_struct_head ------------------
// Parses the head of a struct
// NOTE: All arguments must be valid pointers
errorcode_t parse_struct_head(parse_ctx_t *ctx, strong_cstr_t *out_name, bool *out_is_packed, strong_cstr_t **out_generics, length_t *out_generics_length);

// ------------------ parse_struct_body ------------------
// Parses the body of a struct
errorcode_t parse_struct_body(parse_ctx_t *ctx, char ***names, ast_type_t **types, length_t *length);

// ------------------ parse_struct_grow_fields ------------------
// Grows struct fields so that another field can be appended
void parse_struct_grow_fields(strong_cstr_t **names, ast_type_t **types, length_t length, length_t *capacity, length_t backfill);

// ------------------ parse_struct_free_fields ------------------
// Frees struct fields (used for when errors occur)
void parse_struct_free_fields(strong_cstr_t *names, ast_type_t *types, length_t length, length_t backfill);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_STRUCT_H
