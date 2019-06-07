
#include "UTIL/util.h"
#include "UTIL/search.h"
#include "PARSE/parse.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_struct.h"

errorcode_t parse_struct(parse_ctx_t *ctx){
    ast_t *ast = ctx->ast;
    source_t source = ctx->tokenlist->sources[*ctx->i];

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare struct within another struct's domain");
        return FAILURE;
    }

    strong_cstr_t name;
    bool is_packed;
    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    if(parse_struct_head(ctx, &name, &is_packed, &generics, &generics_length)) return FAILURE;

    const char *invalid_names[] = {
        "Any", "AnyFixedArrayType", "AnyFuncPtrType", "AnyPtrType", "AnyStructType",
        "AnyType", "AnyTypeKind", "StringOwnership", "bool", "byte", "double", "float", "int", "long", "ptr",
        "short", "successful", "ubyte", "uint", "ulong", "ushort", "usize", "void"

        // NOTE: 'String' is allowed
    };
    length_t invalid_names_length = sizeof(invalid_names) / sizeof(const char*);

    if(binary_string_search(invalid_names, invalid_names_length, name) != -1){
        compiler_panicf(ctx->compiler, source, "Reserved type name '%s' can't be used to create a struct", name);
        return FAILURE;
    }

    strong_cstr_t *field_names = NULL;
    ast_type_t *field_types = NULL;
    length_t field_count = 0;

    if(parse_struct_body(ctx, &field_names, &field_types, &field_count)){
        free(name);
        return FAILURE;
    }

    ast_struct_t *domain = NULL;
    trait_t traits = is_packed ? AST_STRUCT_PACKED : TRAIT_NONE;

    if(generics){
        domain = (ast_struct_t*) ast_add_polymorphic_struct(ast, name, field_names, field_types, field_count, traits, source, generics, generics_length);
    } else {
        domain = ast_add_struct(ast, name, field_names, field_types, field_count, traits, source);
    }

    length_t scan_i = *ctx->i + 1;
    while(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_NEWLINE)
        scan_i++;
    
    if(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_BEGIN){
        ctx->struct_association = domain;
        *ctx->i = scan_i;
    }

    return SUCCESS;
}

errorcode_t parse_struct_head(parse_ctx_t *ctx, strong_cstr_t *out_name, bool *out_is_packed, strong_cstr_t **out_generics, length_t *out_generics_length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    *out_is_packed = (tokens[*i].id == TOKEN_PACKED);
    if(*out_is_packed) *i += 1;

    if(parse_eat(ctx, TOKEN_STRUCT, "Expected 'struct' keyword after 'packed' keyword")){
        return FAILURE;
    }

    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    length_t generics_capacity = 0;

    if(tokens[*i].id == TOKEN_LESSTHAN){
        (*i)++;

        while(tokens[*i].id != TOKEN_GREATERTHAN){
            expand((void**) &generics, sizeof(strong_cstr_t), generics_length, &generics_capacity, 1, 4);

            if(parse_ignore_newlines(ctx, "Expected polymorphic generic type")){
                freestrs(generics, generics_length);
                return FAILURE;
            }

            if(tokens[*i].id != TOKEN_POLYMORPH){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type");
                freestrs(generics, generics_length);
                return FAILURE;
            }

            generics[generics_length++] = tokens[*i].data;
            tokens[(*i)++].data = NULL; // Take ownership

            if(parse_ignore_newlines(ctx, "Expected '>' or ',' after polymorphic generic type")){
                freestrs(generics, generics_length);
                return FAILURE;
            }

            if(tokens[*i].id == TOKEN_NEXT){
                if(tokens[++(*i)].id == TOKEN_GREATERTHAN){
                    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type after ',' in generics list");
                    freestrs(generics, generics_length);
                    return FAILURE;
                }
            } else if(tokens[*i].id != TOKEN_GREATERTHAN){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' after polymorphic generic type");
                freestrs(generics, generics_length);
                return FAILURE;
            }
        }

        (*i)++;
    }

    *out_name = parse_take_word(ctx, "Expected structure name after 'struct' keyword");
    if(*out_name == NULL){
        freestrs(generics, generics_length);
        return FAILURE;
    }

    *out_generics = generics;
    *out_generics_length = generics_length;
    return SUCCESS;
}

errorcode_t parse_struct_body(parse_ctx_t *ctx, strong_cstr_t **names, ast_type_t **types, length_t *length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    if(parse_ignore_newlines(ctx, "Expected '(' after struct name")
    || parse_eat(ctx, TOKEN_OPEN, "Expected '(' after struct name")){
        return FAILURE;
    }

    length_t capacity = 0;
    length_t backfill = 0;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected name of field")){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_STRUCT){
            if(backfill != 0){
                compiler_panic(ctx->compiler, sources[*i], "Expected field type for previous fields before struct integration");
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }

            maybe_null_weak_cstr_t inner_struct_name = parse_grab_word(ctx, "Expected struct name for integration");
            if(inner_struct_name == NULL){
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }

            ast_struct_t *inner_struct = ast_struct_find(ctx->ast, inner_struct_name);
            if(inner_struct == NULL){
                compiler_panicf(ctx->compiler, sources[*i], "Struct '%s' must already be declared", inner_struct_name);
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }

            for(length_t f = 0; f != inner_struct->field_count; f++){
                parse_struct_grow_fields(names, types, *length, &capacity, backfill);
                (*types)[*length] = ast_type_clone(&inner_struct->field_types[f]);
                (*names)[(*length)++] = strclone(inner_struct->field_names[f]);
            }

            (*i)++;
        } else {
            parse_struct_grow_fields(names, types, *length, &capacity, backfill);

            strong_cstr_t field_name = parse_take_word(ctx, "Expected name of field");
            if(field_name == NULL){
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }

            (*names)[(*length)++] = field_name;

            if(tokens[*i].id == TOKEN_NEXT){
                backfill++; (*i)++;
                continue;
            }

            ast_type_t *end_type_ptr = &((*types)[*length - 1]);

            if(parse_type(ctx, end_type_ptr)){
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }

            while(backfill != 0){
                (*types)[*length - backfill - 1] = ast_type_clone(end_type_ptr);
                backfill -= 1;
            }
        }

        if(parse_ignore_newlines(ctx, "Expected ')' or ',' after struct field")){
            parse_struct_free_fields(*names, *types, *length, backfill);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected field name and type after ',' in field list");
                parse_struct_free_fields(*names, *types, *length, backfill);
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' after field name and type");
            parse_struct_free_fields(*names, *types, *length, backfill);
            return FAILURE;
        }
    }

    return SUCCESS;
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

void parse_struct_free_fields(strong_cstr_t *names, ast_type_t *types, length_t length, length_t backfill){
    if(names) freestrs(names, length);
    if(types) ast_types_free_fully(types, length - backfill);
}
