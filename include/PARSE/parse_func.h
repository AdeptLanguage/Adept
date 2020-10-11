
#ifndef _ISAAC_PARSE_FUNC_H
#define _ISAAC_PARSE_FUNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "PARSE/parse_ctx.h"

// ------------------ parse_func ------------------
// Parses a function
errorcode_t parse_func(parse_ctx_t *ctx);

// ------------------ parse_func_head ------------------
// Parses the head of a function definition
errorcode_t parse_func_head(parse_ctx_t *ctx, strong_cstr_t *out_name, bool *out_is_stdcall,
        bool *out_is_foreign, bool *out_is_verbatim, bool *out_is_implicit, maybe_null_strong_cstr_t *out_export_name);

// ------------------ parse_func_body ------------------
// Parses the body of a function
errorcode_t parse_func_body(parse_ctx_t *ctx, ast_func_t *func);

// ------------------ parse_func_arguments ------------------
// Parses the arguments that a function takes
errorcode_t parse_func_arguments(parse_ctx_t *ctx, ast_func_t *func);

// ------------------ parse_func_argument ------------------
// Parses a single argument that a function takes
errorcode_t parse_func_argument(parse_ctx_t *ctx, ast_func_t *func, length_t capacity, length_t *backfill, bool *out_is_solid);

// ------------------ parse_func_default_arg_value ------------------
// Parses the default value portion of a function argument
errorcode_t parse_func_default_arg_value(parse_ctx_t *ctx, ast_func_t *func, length_t capacity, length_t *backfill);

// ------------------ parse_func_backfill_arguments ------------------
// Backfills arguments that weren't initially given a type / default value
void parse_func_backfill_arguments(ast_func_t *func, length_t *backfill);

// ------------------ parse_func_grow_arguments ------------------
// Grows function argument list so that another argument can be appended
void parse_func_grow_arguments(ast_func_t *func, length_t backfill, length_t *capacity);

// ------------------ parse_func_prefixes ------------------
// Handles 'stdcall', 'verbatim', 'implicit', and 'external' descriptive keywords in function head
void parse_func_prefixes(parse_ctx_t *ctx, bool *out_is_stdcall, bool *out_is_verbatim, bool *out_is_implicit, bool *out_is_external);

// ------------------ parse_free_unbackfilled_arguments ------------------
// Frees function arguments that never got backfilled
void parse_free_unbackfilled_arguments(ast_func_t *func, length_t backfill);

// ------------------ parse_func_alias ------------------
// Parses a function alias
errorcode_t parse_func_alias(parse_ctx_t *ctx);

// ------------------ parse_func_alias_args ------------------
// Parses argument types for function alias
errorcode_t parse_func_alias_args(parse_ctx_t *ctx, ast_type_t **out_arg_types, length_t *out_arity, trait_t *out_required_traits, bool *out_match_first_of_name);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_FUNC_H
