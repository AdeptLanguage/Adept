
#ifndef PARSE_CTX_H
#define PARSE_CTX_H

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
    ast_struct_t *struct_association;
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
// Returns the word held by the token after current token.
// If that token isn't a word, then 'error' will be spit out
// and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
// (NOTE: ownership isn't taken)
maybe_null_weak_cstr_t parse_grab_word(parse_ctx_t *ctx, const char *error);

// ------------------ parse_grab_string ------------------
// Returns the string held by the token after current token.
// If that token isn't a string, then 'error' will be spit out
// and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
// (NOTE: ownership isn't taken)
maybe_null_weak_cstr_t parse_grab_string(parse_ctx_t *ctx, const char *error);

#ifdef __cplusplus
}
#endif

#endif // PARSE_CTX_H
