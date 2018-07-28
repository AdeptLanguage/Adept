
#include "parse_util.h"

int parse_eat(parse_ctx_t *ctx, tokenid_t id, const char *error){

    if(ctx->tokenlist->tokens[(*ctx->i)++].id != id){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[(*ctx->i) - 1], error);
        return 1;
    }
    return 0;
}

char* parse_eat_word(parse_ctx_t *ctx, const char *error){
    if(ctx->tokenlist->tokens[*ctx->i].id == TOKEN_WORD){
        return ctx->tokenlist->tokens[(*ctx->i)++].data;
    }
    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], error);
    return NULL;
}

char* parse_take_word(parse_ctx_t *ctx, const char *error){
    if(ctx->tokenlist->tokens[*ctx->i].id == TOKEN_WORD){
        char *data = (char*) ctx->tokenlist->tokens[*ctx->i].data;
        ctx->tokenlist->tokens[(*ctx->i)++].data = NULL;
        return data;
    }
    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], error);
    return NULL;
}
