
#ifndef _ISAAC_PARSE_STRUCT_H
#define _ISAAC_PARSE_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "AST/ast.h"
#include "AST/ast_layout.h"
#include "AST/ast_type_lean.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "UTIL/ground.h"

// ------------------ parse_generics ------------------
// Parses a list of generics
// Only writes to `out_` parameters on success
errorcode_t parse_generics(parse_ctx_t *ctx, strong_cstr_t **out_generics, length_t *out_generics_length);

// ------------------ parse_composite ------------------
// Parses a composite
errorcode_t parse_composite(parse_ctx_t *ctx, bool is_union);

// ------------------ parse_composite_domain ------------------
// Parses constructs after the fields of a composite definition
errorcode_t parse_composite_domain(parse_ctx_t *ctx, ast_composite_t *composite);

// ------------------ parse_composite_head ------------------
// Parses the head of a composite
// NOTE: All arguments must be valid pointers
errorcode_t parse_composite_head(
    parse_ctx_t *ctx,
    bool is_union,
    strong_cstr_t *out_name,
    bool *out_is_packed,
    bool *out_is_record,
    bool *out_is_class,
    ast_type_t *out_parent_class,
    strong_cstr_t **out_generics,
    length_t *out_generics_length
);

// ------------------ parse_composite_body ------------------
// Parses the body of a composite
// NOTE: 'maybe_parent_class' may be 'AST_TYPE_NONE'
errorcode_t parse_composite_body(
    parse_ctx_t *ctx,
    ast_field_map_t *out_field_map,
    ast_layout_skeleton_t *out_skeleton,
    bool is_class,
    ast_type_t maybe_parent_class
);

// ------------------ parse_composite_field ------------------
// Parses a single field of a composite
errorcode_t parse_composite_field(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    length_t *inout_backfill,
    ast_layout_endpoint_t *inout_next_endpoint
);

// ------------------ parse_composite_integrate_another ------------------
// Parses a single struct integration field of a composite
errorcode_t parse_composite_integrate_another(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    ast_layout_endpoint_t *inout_next_endpoint,
    const ast_type_t *other_type,
    bool require_class
);

// ------------------ parse_anonymous_composite ------------------
// Parses a single anonymous composite field of a composite
errorcode_t parse_anonymous_composite(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    ast_layout_endpoint_t *inout_next_endpoint
);

// ------------------ parse_struct_is_function_like_beginning ------------------
// Returns whether a token is used as the beginning of a function-like declaration
// (Used for automatic transition from struct fields to struct domain)
bool parse_struct_is_function_like_beginning(tokenid_t token);

// ------------------ parse_create_record_constructor ------------------
// Generate a constructor for a record type
// NOTE: Ownership of 'return_type' is taken
errorcode_t parse_create_record_constructor(parse_ctx_t *ctx, weak_cstr_t name, strong_cstr_t *generics, length_t generics_length, ast_layout_t *layout, source_t source);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_STRUCT_H
