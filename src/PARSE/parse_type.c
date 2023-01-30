
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_enum.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_struct.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

errorcode_t parse_type(parse_ctx_t *ctx, ast_type_t *out_type){
    // Expects from 'ctx': compiler, object, tokenlist, i

    // Expects first token of type to be pointed to by 'i'
    // On success, 'i' will point to the first token after the type parsed
    // 'out_type' is not guaranteed to be in the same state

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    length_t start = *i;

    out_type->elements = NULL;
    out_type->elements_length = 0;

    length_t elements_capacity = 0;
    tokenid_t id = tokens[*i].id;

    if(ctx->compiler->traits & COMPILER_TYPE_COLON && id == TOKEN_COLON){
        // Skip over colon for experimental ": int" syntax
        start = ++(*i);
        id = tokens[start].id;
    }

    while(id == TOKEN_MULTIPLY || id == TOKEN_GENERIC_INT || id == TOKEN_POLYCOUNT || id == TOKEN_BRACKET_OPEN){
        expand((void**) &out_type->elements, sizeof(ast_elem_t*), out_type->elements_length, &elements_capacity, 1, 2);

        switch(id){
        case TOKEN_MULTIPLY: {
                out_type->elements[out_type->elements_length++] = malloc_init(ast_elem_pointer_t, {
                    .id = AST_ELEM_POINTER,
                    .source = sources[*i],
                });
                id = tokens[++(*i)].id;
            }
            break;
        case TOKEN_GENERIC_INT: {
                out_type->elements[out_type->elements_length++] = ast_elem_fixed_array_make(sources[*i], *((adept_generic_int*) tokens[*i].data));
                id = tokens[++(*i)].id;
            }
            break;
        case TOKEN_BRACKET_OPEN: {
                // Eat '['
                (*i)++;

                ast_expr_t *length;
                if(parse_expr(ctx, &length)) goto failure;

                out_type->elements[out_type->elements_length++] = (ast_elem_t*) malloc_init(ast_elem_var_fixed_array_t, {
                    .id = AST_ELEM_VAR_FIXED_ARRAY,
                    .source = sources[*i],
                    .length = length,
                });

                if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']' after size of fixed array in type")) goto failure;
                id = tokens[*i].id;
            }
            break;
        case TOKEN_POLYCOUNT: {
                out_type->elements[out_type->elements_length++] = (ast_elem_t*) malloc_init(ast_elem_polycount_t, {
                    .id = AST_ELEM_POLYCOUNT,
                    .name = parse_ctx_peek_data_take(ctx),
                    .source = sources[*i],
                });

                id = tokens[++(*i)].id;
            }
            break;
        }
    }

    expand((void**) &out_type->elements, sizeof(ast_elem_t*), out_type->elements_length, &elements_capacity, 1, 1);

    switch(id){
    case TOKEN_WORD: {
            out_type->elements[out_type->elements_length] = ast_elem_base_make(parse_ctx_peek_data_take(ctx), sources[*i]);
            *i += 1;
        }
        break;
    case TOKEN_FUNC: case TOKEN_STDCALL: {
            ast_elem_func_t *func_elem = malloc(sizeof(ast_elem_func_t));

            if(parse_type_func(ctx, func_elem)){
                free(func_elem);
                goto failure;
            }

            out_type->elements[out_type->elements_length] = (ast_elem_t*) func_elem;
        }
        break;
    case TOKEN_PACKED: case TOKEN_STRUCT: case TOKEN_UNION: {
            ast_field_map_t field_map;
            ast_layout_skeleton_t skeleton;
            trait_t traits = TRAIT_NONE;
            ast_layout_kind_t layout_kind;

            if(parse_eat(ctx, TOKEN_PACKED, NULL) == SUCCESS){
                traits |= AST_LAYOUT_PACKED;
            }

            // Assumes token is either TOKEN_UNION or TOKEN_STRUCT
            layout_kind = parse_ctx_peek(ctx) == TOKEN_UNION ? AST_LAYOUT_UNION : AST_LAYOUT_STRUCT;
            (*i)++;

            if(parse_composite_body(ctx, &field_map, &skeleton, false, AST_TYPE_NONE)) goto failure;

            // Pass over closing ')'
            (*i)++;
            
            ast_elem_layout_t *layout_elem = malloc(sizeof(ast_elem_layout_t));
            layout_elem->id = AST_ELEM_LAYOUT;
            layout_elem->source = sources[*i];
            ast_layout_init(&layout_elem->layout, layout_kind, field_map, skeleton, traits);

            out_type->elements[out_type->elements_length] = (ast_elem_t*) layout_elem;
        }
        break;
    case TOKEN_POLYMORPH: {
            strong_cstr_t name = parse_ctx_peek_data_take(ctx);
            source_t source = sources[(*i)++];
            bool allow_auto_conversion = false;

            maybe_null_strong_cstr_t similar = NULL;
            ast_type_t extends = {0};

            // If polymorph tokens starts with tilde, then we allow auto conversion
            if(name[0] == '~'){
                allow_auto_conversion = true;
                memmove(name, &name[1], strlen(name)); // (includes '\0')
            }

            if(parse_eat(ctx, TOKEN_BIT_COMPLEMENT, NULL) == SUCCESS){
                if(!ctx->allow_polymorphic_prereqs){
                    free(name);
                    compiler_panicf(ctx->compiler, sources[*i], "Polymorphic prerequisites are not allowed here");
                    goto failure;
                }

                similar = parse_take_word(ctx, "Expected struct name after '~' in polymorphic prerequisite");

                if(similar == NULL){
                    free(name);
                    goto failure;
                }
            }

            if(parse_eat(ctx, TOKEN_EXTENDS, NULL) == SUCCESS){
                if(parse_type(ctx, &extends)){
                    free(name);
                    free(similar);
                    goto failure;
                }
            }

            out_type->elements[out_type->elements_length] =
                (similar != NULL || extends.elements_length != 0)
                    ? ast_elem_polymorph_prereq_make(name, source, allow_auto_conversion, similar, extends)
                    : ast_elem_polymorph_make(name, source, allow_auto_conversion);
        }
        break;
    case TOKEN_LESSTHAN: case TOKEN_BIT_LSHIFT: case TOKEN_BIT_LGC_LSHIFT: {
            if(ctx->angle_bracket_repeat == 0){
                ctx->angle_bracket_repeat = id == TOKEN_LESSTHAN ? 1 : id == TOKEN_BIT_LSHIFT ? 2 : 3;
            }

            if(--ctx->angle_bracket_repeat == 0){
                (*i)++;
            }

            ast_type_t *generics = NULL;
            length_t generics_length = 0;
            length_t generics_capacity = 0;
            
            while(tokens[*i].id != TOKEN_GREATERTHAN){
                expand((void**) &generics, sizeof(ast_type_t), generics_length, &generics_capacity, 1, 4);

                if(parse_ignore_newlines(ctx, "Expected type in polymorphic generics")){
                    ast_types_free_fully(generics, generics_length);
                    goto failure;
                }

                if(parse_type(ctx, &generics[generics_length])){
                    ast_types_free_fully(generics, generics_length);
                    goto failure;
                }

                generics_length++;

                if(parse_ignore_newlines(ctx, "Expected '>' or ',' after type in polymorphic generics")){
                    ast_types_free_fully(generics, generics_length);
                    goto failure;
                }

                if(tokens[*i].id == TOKEN_NEXT){
                    if(tokens[++(*i)].id == TOKEN_GREATERTHAN){
                        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected type after ',' in polymorphic generics");
                        ast_types_free_fully(generics, generics_length);
                        goto failure;
                    }
                } else if(tokens[*i].id != TOKEN_GREATERTHAN){
                    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' after type in polymorphic generics");
                    ast_types_free_fully(generics, generics_length);
                    goto failure;
                }
            }

            strong_cstr_t base_name;
            if(
                parse_eat(ctx, TOKEN_GREATERTHAN, "Expected '>' after polymorphic generics")
                || (base_name = parse_take_word(ctx, "Expected type name")) == NULL
            ){
                ast_types_free_fully(generics, generics_length);
                goto failure;
            }

            out_type->elements[out_type->elements_length] = ast_elem_generic_base_make(base_name, sources[*i - 1], generics, generics_length);
        }
        break;
    case TOKEN_ENUM: {
            weak_cstr_t *raw_kinds;
            length_t raw_kinds_length;
            source_t source = sources[(*i)++];

            if(parse_enum_body(ctx, &raw_kinds, &raw_kinds_length)){
                return FAILURE;
            }

            strong_cstr_list_t kinds = (strong_cstr_list_t){
                .items = raw_kinds,
                .length = raw_kinds_length,
                .capacity = raw_kinds_length,
            };

            // Convert each `weak_cstr_t` to a `strong_cstr_t`
            for(length_t i = 0; i != raw_kinds_length; i++){
                raw_kinds[i] = strclone(raw_kinds[i]);
            }

            // Sort kinds
            strong_cstr_list_sort(&kinds);

            // Assert distinct
            const char *duplicate_name = strong_cstr_list_presorted_has_duplicates(&kinds);
            if(duplicate_name){
                compiler_panicf(ctx->compiler, source, "Anonymous enum type cannot have more than one member named '%s'", duplicate_name);
                strong_cstr_list_free(&kinds);
                return FAILURE;
            }

            out_type->elements[out_type->elements_length] = (ast_elem_t*) malloc_init(ast_elem_anonymous_enum_t, {
                .id = AST_ELEM_ANONYMOUS_ENUM,
                .source = source,
                .kinds = kinds,
            });

            id = tokens[++(*i)].id;
        }
        break;
    default:
        compiler_panic(ctx->compiler, sources[start], "Expected type");
        goto failure;
    }

    out_type->source = sources[start];
    out_type->elements_length++;
    return SUCCESS;

failure:
    ast_type_free(out_type);
    memset(out_type, 0, sizeof(ast_type_t));
    return FAILURE;
}

errorcode_t parse_type_func(parse_ctx_t *ctx, ast_elem_func_t *out_func_elem){
    // Parses the func pointer type element of a type into an ast_elem_func_t
    // func (int, int) int
    //  ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    *out_func_elem = (ast_elem_func_t){
        .id = AST_ELEM_FUNC,
        .source = sources[*i],
        .arg_types = NULL,
        .arity = 0,
        .return_type = NULL,
        .traits = TRAIT_NONE,
        .ownership = true,
    };

    if(parse_eat(ctx, TOKEN_STDCALL, NULL) == SUCCESS){
        out_func_elem->traits |= AST_FUNC_STDCALL;
    }

    bool is_vararg = false;
    length_t args_capacity = 0;

    if(parse_eat(ctx, TOKEN_FUNC, "Expected 'func' keyword in function type")) goto failure;
    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after 'func' keyword in type")) goto failure;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(is_vararg){
            compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            goto failure;
        }

        expand((void**) &out_func_elem->arg_types, sizeof(ast_type_t), out_func_elem->arity, &args_capacity, 1, 4);

        // Ignore argument flow
        switch(tokens[*i].id){
        case TOKEN_IN:    (*i)++; break;
        case TOKEN_OUT:   (*i)++; break;
        case TOKEN_INOUT: (*i)++; break;
        default:          break;
        }

        if(parse_eat(ctx, TOKEN_ELLIPSIS, NULL) == SUCCESS){
            is_vararg = true;
            out_func_elem->traits |= AST_FUNC_VARARG;
        } else {
            if(parse_type(ctx, &out_func_elem->arg_types[out_func_elem->arity])) goto failure;
            out_func_elem->arity++;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
                ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            if(is_vararg){
                compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            } else {
                compiler_panic(ctx->compiler, sources[*i], "Expected ',' after argument type");
            }
            goto failure;
        }
    }

    (*i)++;
    
    ast_type_t *return_type = malloc(sizeof(ast_type_t));

    if(parse_type(ctx, return_type)){
        free(return_type);
        goto failure;
    }
    
    out_func_elem->return_type = return_type;
    return SUCCESS;

failure:
    ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
    return FAILURE;
}

bool parse_can_type_start_with(tokenid_t id, bool allow_open_bracket){
    switch(id){
        case TOKEN_WORD:
        case TOKEN_MULTIPLY:
        case TOKEN_GENERIC_INT:
        case TOKEN_POLYMORPH:
        case TOKEN_POLYCOUNT:
        case TOKEN_LESSTHAN:
        case TOKEN_FUNC:
        case TOKEN_STDCALL:
        case TOKEN_PACKED:
        case TOKEN_STRUCT:
        case TOKEN_UNION:
            return true;
    }

    return allow_open_bracket && id == TOKEN_BRACKET_OPEN;
}
