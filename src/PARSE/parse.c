
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/filename.h"
#include "PARSE/parse.h"
#include "PARSE/parse_enum.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_func.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_alias.h"
#include "PARSE/parse_global.h"
#include "PARSE/parse_pragma.h"
#include "PARSE/parse_struct.h"
#include "PARSE/parse_dependency.h"
#include "BRIDGE/any.h"

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
    qsort(object->ast.polymorphic_funcs, object->ast.polymorphic_funcs_length, sizeof(ast_polymorphic_func_t), &ast_polymorphic_funcs_cmp);
    qsort(object->ast.polymorphic_methods, object->ast.polymorphic_methods_length, sizeof(ast_polymorphic_func_t), &ast_polymorphic_funcs_cmp);
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
        case TOKEN_FUNC: case TOKEN_STDCALL: case TOKEN_VERBATIM:
            if(parse_func(ctx)) return FAILURE;
            break;
        case TOKEN_FOREIGN:
            if(tokens[i + 1].id == TOKEN_STRING || tokens[i + 1].id == TOKEN_CSTRING){
                if(parse_foreign_library(ctx)) return FAILURE;
                break;
            }
            if(parse_func(ctx)) return FAILURE;
            break;
        case TOKEN_STRUCT: case TOKEN_PACKED:
            if(parse_struct(ctx, false)) return FAILURE;
            break;
        case TOKEN_UNION:
            if(parse_struct(ctx, true)) return FAILURE;
            break;
        case TOKEN_CONST:
            if(parse_global_constant_declaration(ctx)) return FAILURE;
            break;
        case TOKEN_WORD:
            if(ctx->compiler->traits & COMPILER_COLON_COLON && tokens[i + 1].id == TOKEN_NAMESPACE){
                if(ctx->prename) free(ctx->prename);
                ctx->prename = parse_take_word(ctx, "Expected pre-name for ::");
                break;
            }
            /* fallthrough */
        case TOKEN_EXTERNAL:
            if(parse_global(ctx)) return FAILURE;
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
            if(ctx->struct_association == NULL){
                compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Unexpected trailing closing brace '}'");
                return FAILURE;
            }
            ctx->struct_association = NULL;
            break;
        default:
            parse_panic_token(ctx, ctx->tokenlist->sources[i], tokens[i].id, "Encountered unexpected token '%s' in global scope");
            return FAILURE;
        }
    }

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Expected closing brace '}' for struct domain");
        return FAILURE;
    }

    return SUCCESS;
}
