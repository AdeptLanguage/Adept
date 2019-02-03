
#include "PARSE/parse.h"
#include "PARSE/parse_type.h"
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

    while(id == TOKEN_MULTIPLY || id == TOKEN_GENERIC_INT){
        expand((void**) &out_type->elements, sizeof(ast_elem_t*), out_type->elements_length, &elements_capacity, 1, 2);

        if(id == TOKEN_MULTIPLY){
            out_type->elements[out_type->elements_length] = malloc(sizeof(ast_elem_pointer_t));
            out_type->elements[out_type->elements_length]->id = AST_ELEM_POINTER;
            out_type->elements[out_type->elements_length]->source = sources[*i];
            out_type->elements_length++;
            id = tokens[++(*i)].id;
            continue;
        }

        if(id == TOKEN_GENERIC_INT){
            ast_elem_fixed_array_t *fixed_array = malloc(sizeof(ast_elem_fixed_array_t));
            source_t fixed_array_source = sources[*i];

            fixed_array->length = *((long long*) tokens[*i].data);

            out_type->elements[out_type->elements_length] = (ast_elem_t*) fixed_array;
            out_type->elements[out_type->elements_length]->id = AST_ELEM_FIXED_ARRAY;
            out_type->elements[out_type->elements_length]->source = fixed_array_source;
            out_type->elements_length++;
            id = tokens[++(*i)].id;
            continue;
        }
    }

    expand((void**) &out_type->elements, sizeof(ast_elem_t*), out_type->elements_length, &elements_capacity, 1, 1);

    switch(id){
    case TOKEN_WORD: {
            ast_elem_base_t *base_elem = malloc(sizeof(ast_elem_base_t));
            base_elem->id = AST_ELEM_BASE;
            base_elem->base = tokens[*i].data;
            base_elem->source = sources[*i];
            tokens[(*i)++].data = NULL; // Take ownership
            out_type->elements[out_type->elements_length] = (ast_elem_t*) base_elem;
        }
        break;
    case TOKEN_FUNC: case TOKEN_STDCALL: {
            ast_elem_func_t *func_elem = malloc(sizeof(ast_elem_func_t));
            func_elem->id = AST_ELEM_FUNC;
            func_elem->source = sources[*i];

            if(parse_type_func(ctx, func_elem)){
                ast_type_free(out_type);
                free(func_elem);
                return FAILURE;
            }

            out_type->elements[out_type->elements_length] = (ast_elem_t*) func_elem;
        }
        break;
    default:
        compiler_panic(ctx->compiler, sources[*i], "Expected type");
        ast_type_free(out_type);
        out_type->elements = NULL;
        out_type->elements_length = 0;
        out_type->source.index = 0;
        out_type->source.object_index = ctx->object->index;
        out_type->source.stride = 0;
        return FAILURE;
    }

    out_type->source = sources[start];
    out_type->elements_length++;
    return SUCCESS;
}

errorcode_t parse_type_func(parse_ctx_t *ctx, ast_elem_func_t *out_func_elem){
    // Parses the func pointer type element of a type into an ast_elem_func_t
    // func (int, int) int
    //  ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    out_func_elem->arg_types = NULL;
    out_func_elem->arg_flows = NULL;
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
            return FAILURE;
        }

        coexpand((void**) &out_func_elem->arg_types, sizeof(ast_type_t), (void**) &out_func_elem->arg_flows,
                sizeof(char), out_func_elem->arity, &args_capacity, 1, 4);

        // Set argument flow
        switch(tokens[*i].id){
        case TOKEN_IN:    out_func_elem->arg_flows[out_func_elem->arity] = FLOW_IN;    (*i)++; break;
        case TOKEN_OUT:   out_func_elem->arg_flows[out_func_elem->arity] = FLOW_OUT;   (*i)++; break;
        case TOKEN_INOUT: out_func_elem->arg_flows[out_func_elem->arity] = FLOW_INOUT; (*i)++; break;
        default:          out_func_elem->arg_flows[out_func_elem->arity] = FLOW_IN;    break;
        }

        if(tokens[*i].id == TOKEN_ELLIPSIS){
            (*i)++; is_vararg = true;
            out_func_elem->traits |= AST_FUNC_VARARG;
        } else {
            if(parse_type(ctx, &out_func_elem->arg_types[out_func_elem->arity])){
                ast_types_free(out_func_elem->arg_types, out_func_elem->arity);
                free(out_func_elem->arg_types);
                free(out_func_elem->arg_flows);
                return FAILURE;
            }
            out_func_elem->arity++;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
                ast_types_free(out_func_elem->arg_types, out_func_elem->arity);
                free(out_func_elem->arg_types);
                free(out_func_elem->arg_flows);
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            if(is_vararg) compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            else compiler_panic(ctx->compiler, sources[*i], "Expected ',' after argument type");
            ast_types_free(out_func_elem->arg_types, out_func_elem->arity);
            free(out_func_elem->arg_types);
            free(out_func_elem->arg_flows);
            return FAILURE;
        }
    }

    out_func_elem->return_type = malloc(sizeof(ast_type_t)); (*i)++;
    out_func_elem->return_type->elements = NULL;
    out_func_elem->return_type->elements_length = 0;
    out_func_elem->return_type->source.index = 0;
    out_func_elem->return_type->source.object_index = ctx->object->index;
    out_func_elem->return_type->source.stride = 0;

    if(parse_type(ctx, out_func_elem->return_type)){
        ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
        ast_type_free_fully(out_func_elem->return_type);
        free(out_func_elem->arg_flows);
        return FAILURE;
    }

    return SUCCESS;
}
