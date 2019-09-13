
#include "UTIL/util.h"
#include "PARSE/parse.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_global.h"

errorcode_t parse_global(parse_ctx_t *ctx){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t source = ctx->tokenlist->sources[*i];
    ast_t *ast = ctx->ast;
    trait_t traits = TRAIT_NONE;

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare global variable within struct domain");
        return FAILURE;
    }

    if(tokens[*i].id == TOKEN_EXTERNAL){
        traits |= AST_GLOBAL_EXTERNAL;
        (*i)++;
    }

    weak_cstr_t name = parse_eat_word(ctx, "INTERNAL ERROR: Expected word");
    if(name == NULL) return FAILURE;

    if(tokens[*i].id == TOKEN_EQUALS){
        return parse_constant_global(ctx, name, source);
    }

    if(tokens[*i].id == TOKEN_POD){
        traits |= AST_GLOBAL_POD;
        (*i)++;
    }
    
    ast_expr_t *initial_value = NULL;

    ast_type_t type;
    if(parse_type(ctx, &type)) return FAILURE;

    if(tokens[*i].id == TOKEN_ASSIGN){
        (*i)++;
        if(tokens[*i].id == TOKEN_UNDEF){
             // 'undef' does nothing for globals, so pretend like this is a plain definition
            (*i)++;
        } else if(parse_expr(ctx, &initial_value)){
            ast_type_free(&type);
            return FAILURE;
        }
    }

    ast_add_global(ast, name, type, initial_value, traits, source);
    return SUCCESS;
}

errorcode_t parse_constant_global(parse_ctx_t *ctx, char *name, source_t source){
    // SOME_CONSTANT == value
    //               ^

    ast_t *ast = ctx->ast;
    *(ctx->i) += 1;

    ast_expr_t *value;
    if(parse_expr(ctx, &value)) return FAILURE;

    expand((void**) &ast->constants, sizeof(ast_constant_t), ast->constants_length, &ast->constants_capacity, 1, 8);

    ast_constant_t *constant = &ast->constants[ast->constants_length++];
    constant->name = name;
    constant->expression = value;
    constant->traits = TRAIT_NONE;
    constant->source = source;

    return SUCCESS;
}
