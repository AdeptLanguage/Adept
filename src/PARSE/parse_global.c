
#include <stdbool.h>
#include <stdlib.h>

#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_named_expression.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_global.h"
#include "PARSE/parse_type.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

errorcode_t parse_global(parse_ctx_t *ctx){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t source = ctx->tokenlist->sources[*i];
    ast_t *ast = ctx->ast;
    trait_t traits = TRAIT_NONE;

    if(ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare global variable within struct domain");
        return FAILURE;
    }

    while(true){
        if(tokens[*i].id == TOKEN_EXTERNAL){
            traits |= AST_GLOBAL_EXTERNAL;
            (*i)++;
            continue;
        }

        if(tokens[*i].id == TOKEN_THREAD_LOCAL){
            traits |= AST_GLOBAL_THREAD_LOCAL;
            (*i)++;
            continue;
        }    

        break;
    }
    
    ast_type_t type = {0};
    ast_expr_t *initial_value = NULL;
    strong_cstr_t name = parse_take_word(ctx, "INTERNAL ERROR: Expected word");

    if(name == NULL) goto failure;

    parse_prepend_namespace(ctx, &name);

    if(parse_ctx_peek(ctx) == TOKEN_EQUALS){
        // Handle old style named expression '==' syntax

        if(parse_old_style_named_expression_global(ctx, name, source)){
            goto failure;
        }

        return SUCCESS;
    }

    if(parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS){
        traits |= AST_GLOBAL_POD;
    }
    
    if(parse_type(ctx, &type)) goto failure;

    if(parse_eat(ctx, TOKEN_ASSIGN, NULL) == SUCCESS){
        if(parse_eat(ctx, TOKEN_UNDEF, NULL) == SUCCESS){
            // 'undef' does nothing for globals, so pretend like this is a plain definition
        } else if(parse_expr(ctx, &initial_value)){
            goto failure;
        }
    }

    if(parse_ctx_peek(ctx) != TOKEN_NEWLINE){
        compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Expected end-of-line after global variable definition");
        goto failure;
    }

    ast_add_global(ast, name, type, initial_value, traits, source);
    return SUCCESS;

failure:
    free(name);
    ast_type_free(&type);
    ast_expr_free_fully(initial_value);
    return FAILURE;
}

errorcode_t parse_named_expression_definition(parse_ctx_t *ctx, ast_named_expression_t *out_named_expression){
    // define NAME = value
    //   ^

    // NOTE: Assumes first token is 'define' keyword
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    // Parse name for the named expression
    strong_cstr_t name;
    
    if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        name = ctx->prename;
        ctx->prename = NULL;
    } else {
        name = parse_take_word(ctx, "Expected name for named expression definition after 'define' keyword");
    }
    if(name == NULL) return FAILURE;

    // Prepend namespace name
    parse_prepend_namespace(ctx, &name);

    // Eat '='
    if(parse_eat(ctx, TOKEN_ASSIGN, "Expected '=' after name of named expression")){
        free(name);
        return FAILURE;
    }

    // Parse the expression of the named expression
    ast_expr_t *value;
    if(parse_expr(ctx, &value)){
        free(name);
        return FAILURE;
    }

    if(parse_ctx_peek(ctx) != TOKEN_NEWLINE){
        compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Expected end-of-line after named expression definition");
        ast_expr_free_fully(value);
        free(name);
        return FAILURE;
    }

    *out_named_expression = ast_named_expression_create(name, value, TRAIT_NONE, source);
    return SUCCESS;
}

errorcode_t parse_global_named_expression_definition(parse_ctx_t *ctx){
    ast_t *ast = ctx->ast;

    ast_named_expression_t new_named_expression;
    if(parse_named_expression_definition(ctx, &new_named_expression)) return FAILURE;

    ast_add_global_named_expression(ast, new_named_expression);
    return SUCCESS;
}

errorcode_t parse_old_style_named_expression_global(parse_ctx_t *ctx, strong_cstr_t name, source_t source){
    // SOME_NAMED_EXPRESSION == value
    //                       ^

    ast_t *ast = ctx->ast;

    *ctx->i += 1;

    ast_expr_t *value;
    if(parse_expr(ctx, &value)) return FAILURE;

    // Prepend namespace name
    parse_prepend_namespace(ctx, &name);

    if(parse_ctx_peek(ctx) != TOKEN_NEWLINE){
        compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Expected end-of-line after named expression definition");
        ast_expr_free_fully(value);
        free(name);
        return FAILURE;
    }

    ast_named_expression_list_append(&ast->named_expressions, ast_named_expression_create(name, value, TRAIT_NONE, source));
    return SUCCESS;
}
