
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

void parse_ctx_init(parse_ctx_t *ctx, compiler_t *compiler, object_t *object){
    ctx->compiler = compiler;
    ctx->object = object;
    ctx->tokenlist = &object->tokenlist;
    ctx->ast = &object->ast;
    ctx->i = NULL;
    ctx->func = NULL;
    ctx->meta_ends_expected = 0;
    memset(ctx->meta_else_allowed_flag, 0, sizeof(ctx->meta_else_allowed_flag));
    ctx->angle_bracket_repeat = 0;
    ctx->composite_association = NULL;
    ctx->has_namespace_scope = false;
    ctx->ignore_newlines_in_expr_depth = 0;
    ctx->allow_polymorphic_prereqs = false;
    ctx->next_builtin_traits = TRAIT_NONE;
    ctx->prename = NULL;
    ctx->struct_closer = TOKEN_CLOSE;
    ctx->struct_closer_char = ')';
}

void parse_ctx_fork(parse_ctx_t *ctx, object_t *new_object, parse_ctx_t *out_ctx_fork){
    *out_ctx_fork = *ctx;
    out_ctx_fork->object = new_object;
    out_ctx_fork->tokenlist = &new_object->tokenlist;
    
    out_ctx_fork->func = NULL;
    out_ctx_fork->meta_ends_expected = 0;
    memset(out_ctx_fork->meta_else_allowed_flag, 0, sizeof(out_ctx_fork->meta_else_allowed_flag));
    out_ctx_fork->angle_bracket_repeat = 0;
    out_ctx_fork->composite_association = NULL;
    out_ctx_fork->has_namespace_scope = false;
    out_ctx_fork->ignore_newlines_in_expr_depth = 0;
    out_ctx_fork->allow_polymorphic_prereqs = false;
    out_ctx_fork->next_builtin_traits = TRAIT_NONE;
    out_ctx_fork->prename = NULL;
}

void parse_ctx_set_meta_else_allowed(parse_ctx_t *ctx, length_t at_ends_expected, bool allowed){
    // Shift range from 1-256 to 0-255
    at_ends_expected--;

    if(at_ends_expected > 255) return;

    length_t group_index = at_ends_expected / 8;

    if(allowed){
        ctx->meta_else_allowed_flag[group_index] |= (1 << (at_ends_expected % 8));
    } else {
        ctx->meta_else_allowed_flag[group_index] &= ~(1 << (at_ends_expected % 8));
    }
}

bool parse_ctx_get_meta_else_allowed(parse_ctx_t *ctx, length_t at_ends_expected){
    // Shift range from 1-256 to 0-255
    at_ends_expected--;

    if(at_ends_expected > 255) return false;

    return ctx->meta_else_allowed_flag[at_ends_expected / 8] & (1 << (at_ends_expected % 8));
}

// =================================================
//                   parse_eat_*
// =================================================

errorcode_t parse_eat(parse_ctx_t *ctx, tokenid_t id, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    if(parse_ctx_peek(ctx) != id){
        // ERROR: That token isn't what it's expected to be
        if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
        return FAILURE;
    }

    *ctx->i += 1;
    return SUCCESS;
}

maybe_null_weak_cstr_t parse_eat_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    if(parse_ctx_peek(ctx) != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
        return NULL;
    }

    return ctx->tokenlist->tokens[(*ctx->i)++].data;
}

maybe_null_weak_cstr_t parse_eat_string(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    tokenid_t id = parse_ctx_peek(ctx);

    if(id == TOKEN_CSTRING || id == TOKEN_STRING){
        // Do lazy conversion to weak c-string
        return ((token_string_data_t*) ctx->tokenlist->tokens[(*ctx->i)++].data)->array;
    }

    // ERROR: That token isn't a string
    if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
    return NULL;
}

// =================================================
//                   parse_take_*
// =================================================

maybe_null_strong_cstr_t parse_take_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    if(parse_ctx_peek(ctx) != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
        return NULL;
    }

    strong_cstr_t ownership = (strong_cstr_t) parse_ctx_peek_data_take(ctx);
    *ctx->i += 1;
    return ownership;
}

maybe_null_strong_cstr_t parse_take_string(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    if(parse_ctx_peek(ctx) != TOKEN_STRING){
        // ERROR: That token isn't a string
        if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
        return NULL;
    }

    strong_cstr_t ownership = (char*) parse_ctx_peek_data_take(ctx);
    *ctx->i += 1;
    return ownership;
}

// =================================================
//                   parse_grab_*
// =================================================

maybe_null_weak_cstr_t parse_grab_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token
    
    *ctx->i += 1;

    if(parse_ctx_peek(ctx) != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
        return NULL;
    }

    return (char*) parse_ctx_peek_data(ctx);
}

maybe_null_weak_cstr_t parse_grab_string(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    *ctx->i += 1;

    tokenid_t id = parse_ctx_peek(ctx);
    
    if(id == TOKEN_CSTRING || id == TOKEN_STRING){
        // Do lazy conversion to weak c-string
        return ((token_string_data_t*) ctx->tokenlist->tokens[(*ctx->i)].data)->array;
    }

    // ERROR: That token isn't a string
    if(error) compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), error);
    return NULL;
}

void parse_prepend_namespace(parse_ctx_t *ctx, strong_cstr_t *inout_name){
    // Don't modify anything if no current namespace
    if(ctx->object->current_namespace == NULL) return;

    strong_cstr_t new_name = mallocandsprintf("%s\\%s", ctx->object->current_namespace, *inout_name);
    free(*inout_name);
    *inout_name = new_name;
}

tokenid_t parse_ctx_peek(parse_ctx_t *ctx){
    return ctx->tokenlist->tokens[*ctx->i].id;
}

source_t parse_ctx_peek_source(parse_ctx_t *ctx){
    return ctx->tokenlist->sources[*ctx->i];
}

void *parse_ctx_peek_data(parse_ctx_t *ctx){
    return ctx->tokenlist->tokens[*ctx->i].data;
}

void *parse_ctx_peek_data_take(parse_ctx_t *ctx){
    token_t *token = &ctx->tokenlist->tokens[*ctx->i];
    void *tmp = token->data;
    token->data = NULL;
    return tmp;
}
