
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_struct.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
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
                out_type->elements[out_type->elements_length] = malloc(sizeof(ast_elem_pointer_t));
                out_type->elements[out_type->elements_length]->id = AST_ELEM_POINTER;
                out_type->elements[out_type->elements_length]->source = sources[*i];
                out_type->elements_length++;
                id = tokens[++(*i)].id;
            }
            break;
        case TOKEN_GENERIC_INT: {
                ast_elem_fixed_array_t *fixed_array = malloc(sizeof(ast_elem_fixed_array_t));
                source_t fixed_array_source = sources[*i];

                fixed_array->length = *((adept_generic_int*) tokens[*i].data);

                out_type->elements[out_type->elements_length] = (ast_elem_t*) fixed_array;
                out_type->elements[out_type->elements_length]->id = AST_ELEM_FIXED_ARRAY;
                out_type->elements[out_type->elements_length]->source = fixed_array_source;
                out_type->elements_length++;
                id = tokens[++(*i)].id;
            }
            break;
        case TOKEN_BRACKET_OPEN: {
                // Eat '['
                (*i)++;

                ast_expr_t *length;
                if(parse_expr(ctx, &length)) goto failure;

                ast_elem_var_fixed_array_t *var_fixed_array = malloc(sizeof(ast_elem_var_fixed_array_t));
                source_t var_fixed_array_source = sources[*i];

                var_fixed_array->length = length;

                out_type->elements[out_type->elements_length] = (ast_elem_t*) var_fixed_array;
                out_type->elements[out_type->elements_length]->id = AST_ELEM_VAR_FIXED_ARRAY;
                out_type->elements[out_type->elements_length]->source = var_fixed_array_source;
                out_type->elements_length++;

                if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']' after size of fixed array in type")) goto failure;
                id = tokens[*i].id;
            }
            break;
        case TOKEN_POLYCOUNT: {
                ast_elem_polycount_t *polycount = malloc(sizeof(ast_elem_polycount_t));
                polycount->id = AST_ELEM_POLYCOUNT;
                polycount->name = parse_ctx_peek_data_take(ctx);
                polycount->source = sources[*i];

                out_type->elements[out_type->elements_length] = (ast_elem_t*) polycount;
                out_type->elements_length++;
                id = tokens[++(*i)].id;
            }
            break;
        }
    }

    expand((void**) &out_type->elements, sizeof(ast_elem_t*), out_type->elements_length, &elements_capacity, 1, 1);

    switch(id){
    case TOKEN_WORD: {
            ast_elem_base_t *base_elem = malloc(sizeof(ast_elem_base_t));
            base_elem->id = AST_ELEM_BASE;
            base_elem->base = parse_ctx_peek_data_take(ctx);
            base_elem->source = sources[*i];
            *i += 1;
            
            out_type->elements[out_type->elements_length] = (ast_elem_t*) base_elem;
        }
        break;
    case TOKEN_FUNC: case TOKEN_STDCALL: {
            ast_elem_func_t *func_elem = malloc(sizeof(ast_elem_func_t));
            func_elem->id = AST_ELEM_FUNC;
            func_elem->source = sources[*i];

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

            if(id == TOKEN_PACKED){
                traits |= AST_LAYOUT_PACKED;
                id = tokens[++(*i)].id;
            }

            // Assumes token is either TOKEN_UNION or TOKEN_STRUCT
            layout_kind = id == TOKEN_UNION ? AST_LAYOUT_UNION : AST_LAYOUT_STRUCT;
            (*i)++;

            if(parse_composite_body(ctx, &field_map, &skeleton)) goto failure;

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

            // If polymorph tokens starts with tilde, then we allow auto conversion
            if(name[0] == '~'){
                allow_auto_conversion = true;
                memmove(name, &name[1], strlen(name)); // (includes '\0')
            }

            if(tokens[*i].id == TOKEN_BIT_COMPLEMENT){
                if(!ctx->allow_polymorphic_prereqs){
                    free(name);
                    compiler_panicf(ctx->compiler, sources[*i], "Polymorphic prerequisites are not allowed here");
                    goto failure;
                }

                // Skip over '~'
                (*i)++;
                
                maybe_null_strong_cstr_t similar = parse_take_word(ctx, "Expected struct name after '~' in polymorphic prerequisite");
                if(!similar){
                    free(name);
                    goto failure;
                }

                ast_elem_polymorph_prereq_t *polymorph_elem = malloc(sizeof(ast_elem_polymorph_prereq_t));
                polymorph_elem->id = AST_ELEM_POLYMORPH_PREREQ;
                polymorph_elem->name = name;
                polymorph_elem->source = source;
                polymorph_elem->allow_auto_conversion = allow_auto_conversion;
                polymorph_elem->similarity_prerequisite = similar;
                out_type->elements[out_type->elements_length] = (ast_elem_t*) polymorph_elem;
            } else {
                ast_elem_polymorph_t *polymorph_elem = malloc(sizeof(ast_elem_polymorph_t));
                polymorph_elem->id = AST_ELEM_POLYMORPH;
                polymorph_elem->name = name;
                polymorph_elem->source = source;
                polymorph_elem->allow_auto_conversion = allow_auto_conversion;
                out_type->elements[out_type->elements_length] = (ast_elem_t*) polymorph_elem;
            }
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
            if(parse_eat(ctx, TOKEN_GREATERTHAN, "Expected '>' after polymorphic generics")
            || (base_name = parse_take_word(ctx, "Expected type name")) == NULL){
                ast_types_free_fully(generics, generics_length);
                goto failure;
            }

            ast_elem_generic_base_t *generic_base_elem = malloc(sizeof(ast_elem_generic_base_t));
            generic_base_elem->id = AST_ELEM_GENERIC_BASE;
            generic_base_elem->name = base_name;
            generic_base_elem->source = sources[*i - 1];
            generic_base_elem->generics = generics;
            generic_base_elem->generics_length = generics_length;
            generic_base_elem->name_is_polymorphic = false;
            out_type->elements[out_type->elements_length] = (ast_elem_t*) generic_base_elem;
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
    return FAILURE;
}

errorcode_t parse_type_func(parse_ctx_t *ctx, ast_elem_func_t *out_func_elem){
    // Parses the func pointer type element of a type into an ast_elem_func_t
    // func (int, int) int
    //  ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    out_func_elem->arg_types = NULL;
    out_func_elem->arity = 0;
    out_func_elem->traits = TRAIT_NONE;
    out_func_elem->ownership = true;

    if(tokens[*i].id == TOKEN_STDCALL){
        out_func_elem->traits |= AST_FUNC_STDCALL; (*i)++;
    }

    bool is_vararg = false;
    length_t args_capacity = 0;

    if(parse_eat(ctx, TOKEN_FUNC, "Expected 'func' keyword in function type")) return FAILURE;
    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after 'func' keyword in type")) return FAILURE;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(is_vararg){
            compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
            return FAILURE;
        }

        expand((void**) &out_func_elem->arg_types, sizeof(ast_type_t), out_func_elem->arity, &args_capacity, 1, 4);

        // Ignore argument flow
        switch(tokens[*i].id){
        case TOKEN_IN:    (*i)++; break;
        case TOKEN_OUT:   (*i)++; break;
        case TOKEN_INOUT: (*i)++; break;
        default:          break;
        }

        if(tokens[*i].id == TOKEN_ELLIPSIS){
            (*i)++; is_vararg = true;
            out_func_elem->traits |= AST_FUNC_VARARG;
        } else {
            if(parse_type(ctx, &out_func_elem->arg_types[out_func_elem->arity])){
                ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
                return FAILURE;
            }
            out_func_elem->arity++;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
                ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            if(is_vararg) compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            else compiler_panic(ctx->compiler, sources[*i], "Expected ',' after argument type");
            ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
            return FAILURE;
        }
    }

    (*i)++;
    out_func_elem->return_type = malloc(sizeof(ast_type_t));
    out_func_elem->return_type->elements = NULL;
    out_func_elem->return_type->elements_length = 0;
    out_func_elem->return_type->source.index = 0;
    out_func_elem->return_type->source.object_index = ctx->object->index;
    out_func_elem->return_type->source.stride = 0;

    if(parse_type(ctx, out_func_elem->return_type)){
        ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
        return FAILURE;
    }

    return SUCCESS;
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
