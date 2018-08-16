
#include "UTIL/util.h"
#include "PARSE/parse.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_struct.h"

int parse_struct(parse_ctx_t *ctx){
    ast_t *ast = ctx->ast;
    source_t source = ctx->tokenlist->sources[*ctx->i];

    char *name;
    bool is_packed;
    if(parse_struct_head(ctx, &name, &is_packed)) return 1;

    char **field_names = NULL;
    ast_type_t *field_types = NULL;
    length_t field_count = 0;

    if(parse_struct_body(ctx, &field_names, &field_types, &field_count)){
        free(name);
        return 1;
    }

    trait_t traits = is_packed ? AST_STRUCT_PACKED : TRAIT_NONE;
    expand((void**) &ast->structs, sizeof(ast_struct_t), ast->structs_length, &ast->structs_capacity, 1, 4);

    ast_struct_t *structure = &ast->structs[ast->structs_length++];
    ast_struct_init(structure, name, field_names, field_types, field_count, traits, source);
    return 0;
}

int parse_struct_head(parse_ctx_t *ctx, char **out_name, bool *out_is_packed){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    *out_is_packed = (tokens[*i].id == TOKEN_PACKED);
    if(*out_is_packed) *i += 1;

    if(parse_eat(ctx, TOKEN_STRUCT, "Expected 'struct' keyword after 'packed' keyword")){
        return 1;
    }

    *out_name = parse_take_word(ctx, "Expected structure name after 'struct' keyword");
    if(*out_name == NULL) return 1;

    return 0;
}

int parse_struct_body(parse_ctx_t *ctx, char ***names, ast_type_t **types, length_t *length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    if(parse_ignore_newlines(ctx, "Expected '(' after struct name")
    || parse_eat(ctx, TOKEN_OPEN, "Expected '(' after struct name")){
        return 1;
    }

    length_t capacity = 0;
    length_t backfill = 0;

    while(tokens[*i].id != TOKEN_CLOSE){
        parse_struct_grow_fields(names, types, *length, &capacity, backfill);

        if(parse_ignore_newlines(ctx, "Expected name of field")){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return 1;
        }

        char *field_name = parse_take_word(ctx, "Expected name of field");
        if(field_name == NULL){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return 1;
        }

        (*names)[(*length)++] = field_name;

        if(tokens[*i].id == TOKEN_NEXT){
            backfill++; (*i)++;
            continue;
        }

        ast_type_t *end_type_ptr =  &((*types)[*length - 1]);

        if(parse_type(ctx, end_type_ptr)){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return 1;
        }

        while(backfill != 0){
            (*types)[*length - backfill - 1] = ast_type_clone(end_type_ptr);
            backfill -= 1;
        }

        if(parse_ignore_newlines(ctx, "Expected ')' or ',' after struct field")){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return 1;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected field name and type after ',' in field list");
                parse_struct_free_fields(*names, *types, *length, backfill);
                return 1;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' after field name and type");
            parse_struct_free_fields(*names, *types, *length, backfill);
            return 1;
        }
    }

    return 0;
}

void parse_struct_grow_fields(char ***names, ast_type_t **types, length_t length, length_t *capacity, length_t backfill){
    if(length == *capacity){
        if(*capacity == 0){
            *capacity = 4;
            *names = malloc(sizeof(char*) * 4);
            *types = malloc(sizeof(ast_type_t) * 4);
            return;
        }

        *capacity *= 2;
        grow((void**) names, sizeof(char*), length, *capacity);
        grow((void**) types, sizeof(ast_type_t), length - backfill, *capacity);
    }
}

void parse_struct_free_fields(char **names, ast_type_t *types, length_t length, length_t backfill){
    for(length_t i = 0; i != length; i++){
        free(names[i]);
    }

    free(names);
    ast_types_free_fully(types, length - backfill);
}
