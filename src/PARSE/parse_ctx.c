
#include "UTIL/color.h"
#include "PARSE/parse_ctx.h"

void parse_ctx_init(parse_ctx_t *ctx, compiler_t *compiler, object_t *object){
    ctx->compiler = compiler;
    ctx->object = object;
    ctx->tokenlist = &object->tokenlist;
    ctx->ast = &object->ast;
    ctx->i = NULL;
    ctx->func = NULL;
}

void parse_ctx_fork(parse_ctx_t *ctx, object_t *new_object, parse_ctx_t *out_ctx_fork){
    *out_ctx_fork = *ctx;
    out_ctx_fork->object = new_object;
    out_ctx_fork->tokenlist = &new_object->tokenlist;
}

// =================================================
//                   parse_eat_*
// =================================================

errorcode_t parse_eat(parse_ctx_t *ctx, tokenid_t id, const char *error){
    length_t i = (*ctx->i)++;

    if(ctx->tokenlist->tokens[i].id != id){
        // ERROR: That token isn't what it's expected to be
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return FAILURE;
    }

    return SUCCESS;
}

maybe_null_weak_cstr_t parse_eat_word(parse_ctx_t *ctx, const char *error){
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
    length_t i = *ctx->i;

    if(ctx->tokenlist->tokens[i].id != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[i], error);
        return NULL;
    }

    char *ownership = (char*) ctx->tokenlist->tokens[i].data;
    ctx->tokenlist->tokens[(*ctx->i)++].data = NULL;
    return ownership;
}

// =================================================
//                   parse_grab_*
// =================================================

maybe_null_weak_cstr_t parse_grab_word(parse_ctx_t *ctx, const char *error){
    tokenid_t id = ctx->tokenlist->tokens[++(*ctx->i)].id;

    if(id != TOKEN_WORD){
        // ERROR: That token isn't a word
        if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i - 1], error);
        return NULL;
    }

    return (char*) ctx->tokenlist->tokens[(*ctx->i)].data;
}

maybe_null_weak_cstr_t parse_grab_string(parse_ctx_t *ctx, const char *error){
    tokenid_t id = ctx->tokenlist->tokens[++(*ctx->i)].id;

    if(id == TOKEN_CSTRING)
        return (char*) ctx->tokenlist->tokens[(*ctx->i)].data;
    
    if(id == TOKEN_STRING)
        // Do lazy conversion to weak c-string
        return ((token_string_data_t*) ctx->tokenlist->tokens[(*ctx->i)].data)->array;

    // ERROR: That token isn't a string
    if(error) compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i - 1], error);
    return NULL;
}
