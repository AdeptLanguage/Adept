
#ifndef _ISAAC_PARSE_STRUCT_H
#define _ISAAC_PARSE_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "AST/ast_layout.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_composite ------------------
// Parses a composite
errorcode_t parse_composite(parse_ctx_t *ctx, bool is_union);

// ------------------ parse_composite_head ------------------
// Parses the head of a composite
// NOTE: All arguments must be valid pointers
errorcode_t parse_composite_head(parse_ctx_t *ctx, bool is_union, strong_cstr_t *out_name, bool *out_is_packed, strong_cstr_t **out_generics, length_t *out_generics_length);

// ------------------ parse_composite_body ------------------
// Parses the body of a composite
errorcode_t parse_composite_body(parse_ctx_t *ctx, ast_field_map_t *out_field_map, ast_layout_skeleton_t *out_skeleton);

// ------------------ parse_composite_field ------------------
// Parses a single field of a composite
errorcode_t parse_composite_field(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, length_t *inout_backfill, ast_layout_endpoint_t *inout_next_endpoint);

// ------------------ parse_struct_integration_field ------------------
// Parses a single struct integration field of a composite
errorcode_t parse_struct_integration_field(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, ast_layout_endpoint_t *inout_next_endpoint);

// ------------------ parse_anonymous_composite ------------------
// Parses a single anonymous composite field of a composite
errorcode_t parse_anonymous_composite(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, ast_layout_endpoint_t *inout_next_endpoint);

// ------------------ parse_struct_is_function_like_beginning ------------------
// Returns whether a token is used as the beginning of a function-like declaration
// (Used for automatic transition from struct fields to struct domain)
bool parse_struct_is_function_like_beginning(tokenid_t token);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_STRUCT_H
