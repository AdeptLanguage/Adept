
#include "UTIL/util.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_namespace.h"

errorcode_t parse_namespace(parse_ctx_t *ctx){
    // namespace my_namespace
    //     ^

    token_t *tokens = ctx->tokenlist->tokens;
    length_t *i = ctx->i;

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Cannot change namespaces within struct domain");
        return FAILURE;
    }

    if(ctx->has_namespace_scope){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Cannot change namespaces while already inside scoped namespace");
        return FAILURE;
    }

    if(parse_eat(ctx, TOKEN_NAMESPACE, "Expected 'namespace' keyword for namespace")) return FAILURE;

    bool has_prename = ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename;

    if(tokens[*i].id == TOKEN_NEWLINE && !has_prename){
        free(ctx->object->current_namespace);

        // Reset namespace to no namespace
        ctx->object->current_namespace = NULL;
        ctx->object->current_namespace_length = 0;
        return SUCCESS;
    }

    strong_cstr_t new_namespace;
    
    if(has_prename){
        new_namespace = ctx->prename;
        ctx->prename = NULL;
    } else {
        new_namespace = parse_take_word(ctx, "Expected name of namespace after 'namespace' keyword");
    }
    
    if(new_namespace == NULL) return FAILURE;

    // Ignore any newlines, don't error if nothing afterwards
    parse_ignore_newlines(ctx, NULL);

    // Look for '{'
    if(*i < ctx->tokenlist->length && ctx->tokenlist->tokens[*i].id == TOKEN_BEGIN){
        // If '{' was found, start the namespace scope
        ctx->has_namespace_scope = true;
    } else {
        // If '{' wasn't found, then move back one to properly end on last token
        (*i)--;
    }

    free(ctx->object->current_namespace);
    ctx->object->current_namespace = new_namespace;
    ctx->object->current_namespace_length = strlen(new_namespace);
    return SUCCESS;
}

errorcode_t parse_using_namespace(parse_ctx_t *ctx){
    // using namespace my_namespace
    //   ^
    
    if(parse_eat(ctx, TOKEN_USING, "Expected 'using' keyword for 'using namespace' directive")) return FAILURE;
    if(parse_eat(ctx, TOKEN_NAMESPACE, "Expected 'namespace' keyword after 'using' keyword")) return FAILURE;

    weak_cstr_t namespace = parse_eat_word(ctx, "Expected name of namespace after 'using namespace'");
    if(namespace == NULL) return FAILURE;

    object_t *object = ctx->object;
    expand((void**) &object->using_namespaces, sizeof(weak_lenstr_t), object->using_namespaces_length, &object->using_namespaces_capacity, 1, 4);

    weak_lenstr_t *now_using = &object->using_namespaces[object->using_namespaces_length++];
    now_using->cstr = namespace;
    now_using->length = strlen(namespace);
    return SUCCESS;
}
