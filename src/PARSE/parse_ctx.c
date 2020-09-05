
#include "UTIL/color.h"
#include "PARSE/parse_ctx.h"

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
    ctx->struct_association = NULL;
    ctx->ignore_newlines_in_expr_depth = 0;
    ctx->allow_polymorphic_prereqs = false;
    ctx->next_builtin_traits = TRAIT_NONE;
}

void parse_ctx_fork(parse_ctx_t *ctx, object_t *new_object, parse_ctx_t *out_ctx_fork){
    *out_ctx_fork = *ctx;
    out_ctx_fork->object = new_object;
    out_ctx_fork->tokenlist = &new_object->tokenlist;
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
    
    length_t i = (*ctx->i)++;

    if(ctx->tokenlist->tokens[i].id != id){
        // ERROR: That token isn't what it's expected to be
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return FAILURE;
    }

    return SUCCESS;
}

maybe_null_weak_cstr_t parse_eat_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    length_t i = *ctx->i;

    if(ctx->tokenlist->tokens[i].id != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return NULL;
    }

    return ctx->tokenlist->tokens[(*ctx->i)++].data;
}

// =================================================
//                   parse_take_*
// =================================================

maybe_null_strong_cstr_t parse_take_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    length_t i = *ctx->i;

    if(ctx->tokenlist->tokens[i].id != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return NULL;
    }

    strong_cstr_t ownership = (char*) ctx->tokenlist->tokens[i].data;
    ctx->tokenlist->tokens[(*ctx->i)++].data = NULL;
    return ownership;
}

maybe_null_strong_cstr_t parse_take_string(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    length_t i = *ctx->i;
    
    if(ctx->tokenlist->tokens[i].id != TOKEN_STRING){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return NULL;
    }

    strong_cstr_t ownership = (char*) ctx->tokenlist->tokens[i].data;
    ctx->tokenlist->tokens[(*ctx->i)++].data = NULL;
    return ownership;
}

// =================================================
//                   parse_grab_*
// =================================================

maybe_null_weak_cstr_t parse_grab_word(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    tokenid_t id = ctx->tokenlist->tokens[++(*ctx->i)].id;

    if(id != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], error);
        return NULL;
    }

    return (char*) ctx->tokenlist->tokens[(*ctx->i)].data;
}

maybe_null_weak_cstr_t parse_grab_string(parse_ctx_t *ctx, const char *error){
    // NOTE: We don't need to check whether *ctx->i == ctx->tokenlist->length because
    // every token list is terminated with a newline and this should never
    // be called from a newline token

    tokenid_t id = ctx->tokenlist->tokens[++(*ctx->i)].id;

    if(id == TOKEN_CSTRING)
        return (char*) ctx->tokenlist->tokens[(*ctx->i)].data;
    
    if(id == TOKEN_STRING)
        // Do lazy conversion to weak c-string
        return ((token_string_data_t*) ctx->tokenlist->tokens[(*ctx->i)].data)->array;

    // ERROR: That token isn't a string
    if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], error);
    return NULL;
}
