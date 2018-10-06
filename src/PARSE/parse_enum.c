
#include "UTIL/util.h"
#include "PARSE/parse_enum.h"
#include "PARSE/parse_util.h"

errorcode_t parse_enum(parse_ctx_t *ctx){
    char **kinds = NULL;
    length_t length = 0;
    source_t source = ctx->tokenlist->sources[*ctx->i];

    if(parse_eat(ctx, TOKEN_ENUM, "Expected 'enum' keyword")) return FAILURE;

    char *name = parse_eat_word(ctx, "Expected name of enum after 'enum' keyword");
    if(name == NULL) return FAILURE;

    if(parse_enum_body(ctx, &kinds, &length)) return FAILURE;

    ast_add_enum(ctx->ast, name, kinds, length, source);
    return SUCCESS;
}

errorcode_t parse_enum_body(parse_ctx_t *ctx, char ***kinds, length_t *length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    length_t capacity = 0;
    
    if(parse_ignore_newlines(ctx, "Expected '(' after enum name")
    || parse_eat(ctx, TOKEN_OPEN, "Expected '(' after enum name")){
        return FAILURE;
    }

    while(tokens[*i].id != TOKEN_CLOSE){
        expand((void**) kinds, sizeof(char*), *length, &capacity, 1, 4);

        if(parse_ignore_newlines(ctx, "Expected element")){
            free(*kinds);
            return FAILURE;
        }

        char *kind = parse_eat_word(ctx, "Expected element");
        if(kind == NULL){
            free(*kinds);
            return FAILURE;
        }

        (*kinds)[(*length)++] = kind;

        if(parse_ignore_newlines(ctx, "Expected ',' or ')'")){
            free(*kinds);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected element after ',' in element list");
                free(*kinds);
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' after element");
            free(*kinds);
            return FAILURE;
        }
    }

    return SUCCESS;
}
