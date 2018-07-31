
#ifndef PARSE_CTX_H
#define PARSE_CTX_H

#include "ast.h"
#include "lex.h"
#include "ground.h"
#include "object.h"
#include "compiler.h"

// Internal parse states that determine what to do
#define PARSE_STATE_NONE         0x00000000
#define PARSE_STATE_IDLE         0x00000001
#define PARSE_STATE_FUNC         0x00000002
#define PARSE_STATE_FOREIGN      0x00000003
#define PARSE_STATE_STRUCT       0x00000004
#define PARSE_STATE_GLOBAL       0x00000005
#define PARSE_STATE_ALIAS        0x00000006

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
} parse_ctx_t;

// ------------------ parse_ctx_init ------------------
// Initializes a parse context for parsing
void parse_ctx_init(parse_ctx_t *ctx, compiler_t *compiler, object_t *object);

// ------------------ parse_ctx_fork ------------------
// Forks an existing parse context for parsing
void parse_ctx_fork(parse_ctx_t *ctx, object_t *new_object, parse_ctx_t *out_ctx_fork);


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
int parse_eat(parse_ctx_t *ctx, tokenid_t id, const char *error);

// ------------------ parse_eat_word ------------------
// Returns the string held by a word at the current
// token index. If the current token isn't a word, 'error'
// will be spit out and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
char* parse_eat_word(parse_ctx_t *ctx, const char *error);

// ==================================================
//                   PARSE_TAKE_*
//  Same as parse_eat_* except ownership isn't taken
// ==================================================

// ------------------ parse_take_word ------------------
// NOTE: THIS FUNCTION TAKES OWNERSHIP AND RETURNS IT.
// Takes ownership of the string held by a word
// at the current token index. If the current token isn't
// a word, 'error' will be spit out and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
char* parse_take_word(parse_ctx_t *ctx, const char *error);

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
char* parse_grab_word(parse_ctx_t *ctx, const char *error);

// ------------------ parse_grab_string ------------------
// Returns the string held by the token after current token.
// If that token isn't a string, then 'error' will be spit out
// and NULL will be returned.
// (NOTE: error can be NULL to indicate no error should be printed)
// (NOTE: ownership isn't taken)
char* parse_grab_string(parse_ctx_t *ctx, const char *error);

#endif // PARSE_CTX_H
