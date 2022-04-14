
#include <stdbool.h>
#include <stdlib.h>

#include "AST/ast.h"
#include "BRIDGE/any.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/token.h"
#include "PARSE/parse.h"
#include "PARSE/parse_alias.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_dependency.h"
#include "PARSE/parse_enum.h"
#include "PARSE/parse_func.h"
#include "PARSE/parse_global.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_namespace.h"
#include "PARSE/parse_pragma.h"
#include "PARSE/parse_struct.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"

errorcode_t parse(compiler_t *compiler, object_t *object){
    parse_ctx_t ctx;

    object_init_ast(object, compiler->cross_compile_for);
    parse_ctx_init(&ctx, compiler, object);
    
    if(!(compiler->traits & COMPILER_INFLATE_PACKAGE)){
        any_inject_ast(ctx.ast);
        va_args_inject_ast(compiler, ctx.ast);
    }
    
    if(parse_tokens(&ctx)){
        if(ctx.prename) free(ctx.prename);
        return FAILURE;
    }

    if(ctx.prename) free(ctx.prename);
    qsort(object->ast.poly_funcs, object->ast.poly_funcs_length, sizeof(ast_poly_func_t), &ast_poly_funcs_cmp);
    qsort(object->ast.polymorphic_methods, object->ast.polymorphic_methods_length, sizeof(ast_poly_func_t), &ast_poly_funcs_cmp);
    return SUCCESS;
}

errorcode_t parse_tokens(parse_ctx_t *ctx){
    // Expects from 'ctx': compiler, object, tokenlist, ast

    length_t i = 0;
    token_t *tokens = ctx->tokenlist->tokens;
    length_t tokens_length = ctx->tokenlist->length;

    for(ctx->i = &i; i != tokens_length; i++){
        switch(tokens[i].id){
        case TOKEN_NEWLINE:
            break;
        case TOKEN_FUNC: case TOKEN_STDCALL: case TOKEN_VERBATIM: case TOKEN_IMPLICIT: case TOKEN_CONSTRUCTOR:
            if(parse_func(ctx)) return FAILURE;
            break;
        case TOKEN_FOREIGN:
            if(tokens[i + 1].id == TOKEN_STRING || tokens[i + 1].id == TOKEN_CSTRING){
                if(parse_foreign_library(ctx)) return FAILURE;
                break;
            }
            if(parse_func(ctx)) return FAILURE;
            break;
        case TOKEN_STRUCT: case TOKEN_PACKED: case TOKEN_RECORD:
            if(parse_composite(ctx, false)) return FAILURE;
            break;
        case TOKEN_UNION:
            if(parse_composite(ctx, true)) return FAILURE;
            break;
        case TOKEN_DEFINE:
            if(parse_global_constant_definition(ctx)) return FAILURE;
            break;
        case TOKEN_WORD:
            if(ctx->compiler->traits & COMPILER_COLON_COLON && tokens[i + 1].id == TOKEN_ASSOCIATE){
                if(ctx->prename) free(ctx->prename);
                ctx->prename = parse_take_word(ctx, "Expected pre-name for ::");
                break;
            }
            if(parse_global(ctx)) return FAILURE;
            break;
        case TOKEN_EXTERNAL: {
                tokenid_t next = tokens[i + 1].id;
                if(next == TOKEN_FUNC || next == TOKEN_STDCALL || next == TOKEN_VERBATIM || next == TOKEN_IMPLICIT){
                    if(parse_func(ctx)) return FAILURE;
                } else {
                    if(parse_global(ctx)) return FAILURE;
                }
            }
            break;
        case TOKEN_ALIAS:
            if(parse_alias(ctx)) return FAILURE;
            break;
        case TOKEN_IMPORT:
            if(parse_import(ctx)) return FAILURE;
            break;
        case TOKEN_PRAGMA:
            if(parse_pragma(ctx)) return FAILURE;
            break;
        case TOKEN_ENUM:
            if(parse_enum(ctx)) return FAILURE;
            break;
        case TOKEN_META:
            if(parse_meta(ctx)) return FAILURE;
            break;
        case TOKEN_END:
            if(ctx->has_namespace_scope){
                ctx->has_namespace_scope = false;

                free(ctx->object->current_namespace);
                ctx->object->current_namespace = NULL;
                ctx->object->current_namespace_length = 0;
            } else if(ctx->composite_association != NULL){
                ctx->composite_association = NULL;
            } else {
                compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Unexpected trailing closing brace '}'");
                return FAILURE;
            }
            break;
        case TOKEN_NAMESPACE:
            if(parse_namespace(ctx)) return FAILURE;
            break;
        default:
            parse_panic_token(ctx, ctx->tokenlist->sources[i], tokens[i].id, "Encountered unexpected token '%s' in global scope");
            return FAILURE;
        }
    }

    if(ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Expected closing brace '}' for struct domain");
        return FAILURE;
    }

    return SUCCESS;
}
