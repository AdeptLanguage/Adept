
#ifndef _ISAAC_PARSE_CTX_H
#define _ISAAC_PARSE_CTX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast.h"
#include "LEX/lex.h"
#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"

// ------------------ parse_ctx_t ------------------
// A general container struct that holds general
// information about the current parsing context
typedef struct {
    compiler_t *compiler;
    object_t *object;
    tokenlist_t *tokenlist;
    ast_t *ast;
    length_t *i;
    ast_func_t *func;

    // NOTE: 256 is the max number of meta ends that can be expected
    length_t meta_ends_expected;
    unsigned char meta_else_allowed_flag[32];

    // Used to 'unlex' multiple angle brackets
    length_t angle_bracket_repeat;

    // Used to automatically promote functions into methods
    ast_polymorphic_struct_t *struct_association;
    bool struct_association_is_polymorphic;

    // Used to allow parse_expr and friends to ignore newlines within expressions
    length_t ignore_newlines_in_expr_depth;

    // Used to allow parse_type to parse polymorphic types that have prerequisites
    bool allow_polymorphic_prereqs;

    trait_t next_builtin_traits;

    // Experimental pre-naming syntax
    maybe_null_strong_cstr_t prename;
} parse_ctx_t;

// ------------------ parse_ctx_init ------------------
// Initializes a parse context for parsing
void parse_ctx_init(parse_ctx_t *ctx, compiler_t *compiler, object_t *object);

// ------------------ parse_ctx_fork ------------------
// Forks an existing parse context for parsing
void parse_ctx_fork(parse_ctx_t *ctx, object_t *new_object, parse_ctx_t *out_ctx_fork);

// ------------------ parse_ctx_set_meta_else_allowed ------------------
// Sets whether #else is allowed at a certain ends-expected depth
// NOTE: 'at_ends_expected' must be between 1-256 inclusive
void parse_ctx_set_meta_else_allowed(parse_ctx_t *ctx, length_t at_ends_expected, bool allowed);

// ------------------ parse_ctx_get_meta_else_allowed ------------------
// Returns whether #else is allowed at a certain ends-expected depth
// NOTE: 'at_ends_expected' must be between 1-256 inclusive
bool parse_ctx_get_meta_else_allowed(parse_ctx_t *ctx, length_t at_ends_expected);

// ==================================================
//                    PARSE_EAT_*
//         Eats current token and mores ahead
//
//                  [0]    [1]    [2]
//                   ^
//             current/eaten
// ==================================================

// ------------------ parse_eat ------------------
// Eats a token with a given id, if the current token
// doesn't match the given id, 'error' will be spit out
// and 1 will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
errorcode_t parse_eat(parse_ctx_t *ctx, tokenid_t id, const char *error);

// ------------------ parse_eat_word ------------------
// Returns the string held by a word at the current
// token index. If the current token isn't a word, 'error'
// will be spit out and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
maybe_null_weak_cstr_t parse_eat_word(parse_ctx_t *ctx, const char *error);

// ==================================================
//                   PARSE_TAKE_*
//  Same as parse_eat_* except ownership is taken
// ==================================================

// ------------------ parse_take_word ------------------
// NOTE: THIS FUNCTION TAKES OWNERSHIP AND RETURNS IT.
// Takes ownership of the string held by a word
// at the current token index. If the current token isn't
// a word, 'error' will be spit out and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
maybe_null_strong_cstr_t parse_take_word(parse_ctx_t *ctx, const char *error);

// ==================================================
//                    PARSE_GRAB_*
//         Moves ahead and grabs the next token
//
//                  [0]    [1]    [2]
//                   ^      ^
//               current  grabbed
// ==================================================

// ------------------ parse_grab_word ------------------
// Returns the word held by the token AFTER the current token.
// If that token isn't a word, then 'error' will be spit out
// and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
// (NOTE: ownership isn't taken)
// (NOTE: *ctx->i is moved ahead by 1)
maybe_null_weak_cstr_t parse_grab_word(parse_ctx_t *ctx, const char *error);

// ------------------ parse_grab_string ------------------
// Returns the string held by the token AFTER the current token.
// If that token isn't a string, then 'error' will be spit out
// and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
// (NOTE: ownership isn't taken)
// (NOTE: *ctx->i is moved ahead by 1)
maybe_null_weak_cstr_t parse_grab_string(parse_ctx_t *ctx, const char *error);

// ------------------ parse_prepend_namespace ------------------
// Prepends the current namespace to a strong cstring
void parse_prepend_namespace(parse_ctx_t *ctx, strong_cstr_t *inout_name);

// ------------------ parse_relocate_name ------------------
// Modifies names such as 'local\myFunction' to point to the
// local namespace. If the name provided is not a 'local\'
// name, then nothing will be modified.
errorcode_t parse_relocate_name(parse_ctx_t *ctx, source_t source, strong_cstr_t *inout_name);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_PARSE_CTX_H
