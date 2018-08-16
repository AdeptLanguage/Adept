
#include "AST/ast.h"
#include "UTIL/util.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_alias.h"

int parse_alias(parse_ctx_t *ctx){
    // NOTE: Assumes 'alias' keyword

    ast_t *ast = ctx->ast;

    ast_type_t type;
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    char *name = parse_eat_word(ctx, "Expected alias name after 'alias' keyword");
    if (name == NULL) return 1;

    if(parse_eat(ctx, TOKEN_ASSIGN, "Expected '=' after alias name")) return 1;
    if(parse_type(ctx, &type)) return 1;

    expand((void**) &ast->aliases, sizeof(ast_alias_t), ast->aliases_length, &ast->aliases_capacity, 1, 8);

    ast_alias_t *alias = &ast->aliases[ast->aliases_length++];
    ast_alias_init(alias, name, type, TRAIT_NONE, source);
    return 0;
}
