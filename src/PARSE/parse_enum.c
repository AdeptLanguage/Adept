
#include <stdlib.h>

#include "AST/ast.h"
#include "DRVR/compiler.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_enum.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

errorcode_t parse_enum(parse_ctx_t *ctx, bool is_foreign){
    char **kinds = NULL;
    length_t length = 0;
    source_t source = parse_ctx_peek_source(ctx);

    if(ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare enum within struct domain");
        return FAILURE;
    }

    if(parse_eat(ctx, TOKEN_ENUM, "Expected 'enum' keyword")) return FAILURE;

    maybe_null_strong_cstr_t name;
    
    if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        name = ctx->prename;
        ctx->prename = NULL;
    } else {
        name = parse_take_word(ctx, "Expected name of enum after 'enum' keyword");
    }

    if(name == NULL) return FAILURE;

    // Prepend namespace if applicable
    parse_prepend_namespace(ctx, &name);

    if(parse_enum_body(ctx, &kinds, &length)) return FAILURE;

    ast_add_enum(ctx->ast, name, kinds, length, source);

    if(is_foreign){
        // Automatically add defines so using the enum name is optional
        for(length_t i = 0; i != length; i++){
            ast_expr_t *value;
            ast_expr_create_enum_value(&value, name, kinds[i], source);

            ast_constant_t auto_constant = (ast_constant_t){
                .name = strclone(kinds[i]),
                .expression = value,
                .traits = TRAIT_NONE,
                .source = source,
            };

            ast_add_global_constant(ctx->ast, auto_constant);
        }
    }

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
