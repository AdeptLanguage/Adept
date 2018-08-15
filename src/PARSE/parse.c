
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/filename.h"
#include "PARSE/parse.h"
#include "PARSE/parse_func.h"
#include "PARSE/parse_pragma.h"
#include "PARSE/parse_dependency.h"

int parse(compiler_t *compiler, object_t *object){
    parse_ctx_t ctx;

    object_init_ast(object);
    parse_ctx_init(&ctx, compiler, object);

    if(parse_tokens(&ctx)) return 1;

    return 0;
}

int parse_tokens(parse_ctx_t *ctx){
    // Expects from 'ctx': compiler, object, tokenlist, ast

    ast_t *ast = ctx->ast;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    unsigned int state = PARSE_STATE_IDLE;
    source_t source;

    length_t i = 0;
    ctx->i = &i;
    source.index = 0;
    source.object_index = ctx->object->index;

    for(; i != ctx->tokenlist->length; i++){
        switch(state){
        case PARSE_STATE_IDLE:
            switch(tokens[i].id){
            case TOKEN_NEWLINE:
                break;
            case TOKEN_FUNC: case TOKEN_STDCALL:
                if(parse_func(ctx)) return 1;
                break;
            case TOKEN_FOREIGN:
                if(tokens[i + 1].id == TOKEN_STRING || tokens[i + 1].id == TOKEN_CSTRING){
                    parse_foreign_library(ctx);
                    state = PARSE_STATE_IDLE;
                    break;
                }
                if(parse_func(ctx)) return 1;
                break;
            case TOKEN_STRUCT: case TOKEN_PACKED:
                state = PARSE_STATE_STRUCT;
                break;
            case TOKEN_WORD:
                state = PARSE_STATE_GLOBAL;
                break;
            case TOKEN_ALIAS:
                state = PARSE_STATE_ALIAS;
                break;
            case TOKEN_IMPORT:
                if(parse_import(ctx)) return 1;
                break;
            case TOKEN_PRAGMA:
                if(parse_pragma(ctx)) return 1;
                break;
            default:
                parse_panic_token(ctx, sources[i], tokens[i].id, "Encountered unexpected token '%s' in global scope");
                return 1;
            }
            break;
        case PARSE_STATE_STRUCT: {
                source = sources[i - 1];

                bool is_packed = false;
                if(tokens[i - 1].id == TOKEN_PACKED){ i++; is_packed = true; }

                char *struct_name = parse_take_word(ctx, "Expected structure name after 'struct' keyword");
                if(struct_name == NULL) return 1;

                if(parse_ignore_newlines(ctx, "Expected '(' after name of struct")){
                    free(struct_name);
                    return 1;
                }

                if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after structure name")){
                    free(struct_name);
                    return 1;
                }

                char **field_names = malloc(sizeof(char*) * 2);
                ast_type_t *field_types = malloc(sizeof(ast_type_t) * 2);
                length_t field_count = 0;
                length_t field_capacity = 2;
                length_t field_backfill = 0;

                while(tokens[i].id != TOKEN_CLOSE){
                    if(field_count == field_capacity){
                        field_capacity *= 2;
                        char **new_field_names = malloc(sizeof(char*) * field_capacity);
                        ast_type_t *new_field_types = malloc(sizeof(ast_type_t) * field_capacity);
                        memcpy(new_field_names, field_names, sizeof(char*) * field_count);
                        memcpy(new_field_types, field_types, sizeof(ast_type_t) * (field_count  - field_backfill));
                        free(field_names);
                        free(field_types);
                        field_names = new_field_names;
                        field_types = new_field_types;
                    }

                    if(parse_ignore_newlines(ctx, "Expected name of field")){
                        for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                        ast_types_free_fully(field_types, field_count - field_backfill);
                        free(struct_name);
                        free(field_names);
                        return 1;
                    }

                    if(tokens[i].id != TOKEN_WORD){
                        compiler_panic(ctx->compiler, sources[i], "Expected name of field");
                        for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                        ast_types_free_fully(field_types, field_count - field_backfill);
                        free(struct_name);
                        free(field_names);
                        return 1;
                    }

                    field_names[field_count] = (char*) tokens[i].data;
                    tokens[i++].data = NULL; // Take ownership
                    field_count++;

                    if(tokens[i].id == TOKEN_NEXT){
                        // This field has the type of the next field
                        field_backfill += 1;
                        i++; continue;
                    }

                    if(parse_type(ctx, &field_types[field_count - 1])){
                        for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                        ast_types_free_fully(field_types, field_count - field_backfill - 1);
                        free(struct_name);
                        free(field_names);
                        return 1;
                    }

                    while(field_backfill != 0){
                        field_types[field_count - field_backfill - 1] = ast_type_clone(&field_types[field_count - 1]);
                        field_backfill -= 1;
                    }

                    if(parse_ignore_newlines(ctx, "Expected ')' or ',' after struct field")){
                        for(length_t n = 0; n != field_count + 1; n++) free(field_names[n]);
                        ast_types_free_fully(field_types, field_count - field_backfill);
                        free(struct_name);
                        free(field_names);
                        return 1;
                    }

                    if(tokens[i].id == TOKEN_NEXT){
                        if(tokens[++i].id == TOKEN_CLOSE){
                            compiler_panic(ctx->compiler, sources[i], "Expected field name and type after ',' in field list");
                            for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                            ast_types_free_fully(field_types, field_count - field_backfill);
                            free(struct_name);
                            free(field_names);
                            return 1;
                        }
                    } else if(tokens[i].id != TOKEN_CLOSE){
                        compiler_panic(ctx->compiler, sources[i], "Expected ',' after field name and type");
                        for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                        ast_types_free_fully(field_types, field_count - field_backfill);
                        free(struct_name);
                        free(field_names);
                        return 1;
                    }
                }

                if(field_backfill != 0){
                    compiler_panic(ctx->compiler, sources[i], "Expected field type before end of struct");
                    for(length_t n = 0; n != field_count; n++) free(field_names[n]);
                    ast_types_free_fully(field_types, field_count - field_backfill);
                    free(struct_name);
                    free(field_names);
                    return 1;
                }

                if(ast->structs_length == ast->structs_capacity){
                    ast->structs_capacity *= 2;
                    ast_struct_t *new_structs = malloc(sizeof(ast_struct_t) * ast->structs_capacity);
                    memcpy(new_structs, ast->structs, sizeof(ast_struct_t) * ast->structs_length);
                    free(ast->structs);
                    ast->structs = new_structs;
                }

                ast_struct_t *structure = &ast->structs[ast->structs_length++];

                structure->name = struct_name;
                structure->field_names = field_names;
                structure->field_types = field_types;
                structure->field_count = field_count;
                structure->traits = is_packed ? AST_STRUCT_PACKED : TRAIT_NONE;
                structure->source = source;

                state = PARSE_STATE_IDLE;
            }
            break;
        case PARSE_STATE_GLOBAL: {
                source = sources[i - 1];

                // Is it a constant? "CONSTANT_NAME == <some value>"
                if(tokens[i].id == TOKEN_EQUALS){
                    char *constant_name = (char*) tokens[i++ - 1].data;
                    ast_expr_t *constant_expr;

                    if(parse_expr(ctx, &constant_expr)) return 1;

                    if(ast->constants_length == ast->constants_capacity){
                        if(ast->constants_capacity == 0){
                            ast->constants = malloc(sizeof(ast_constant_t) * 8);
                            ast->constants_capacity = 8;
                        } else {
                            ast->constants_capacity *= 2;
                            ast_constant_t *new_constants = malloc(sizeof(ast_constant_t) * ast->constants_capacity);
                            memcpy(new_constants, ast->constants, sizeof(ast_constant_t) * ast->constants_length);
                            free(ast->constants);
                            ast->constants = new_constants;
                        }
                    }

                    ast_constant_t *constant = &ast->constants[ast->constants_length++];
                    constant->name = constant_name;
                    constant->expression = constant_expr;
                    constant->traits = TRAIT_NONE;
                    constant->source = source;
                    state = PARSE_STATE_IDLE;
                    break;
                }

                // It is a global variable
                char *global_name = (char*) tokens[i - 1].data;
                ast_expr_t *global_initial = NULL;
                ast_type_t global_type;

                if(parse_type(ctx, &global_type)) return 1;

                if(tokens[i].id == TOKEN_ASSIGN){
                    i++; if(tokens[i].id == TOKEN_UNDEF){
                        i++; // 'undef' does nothing for globals, so pretend like this is a plain definition
                    } else if(parse_expr(ctx, &global_initial)){
                        ast_type_free(&global_type);
                        return 1;
                    }
                }

                if(ast->globals_length == ast->globals_capacity){
                    if(ast->globals_capacity == 0){
                        ast->globals = malloc(sizeof(ast_global_t) * 8);
                        ast->globals_capacity = 8;
                    } else {
                        ast->globals_capacity *= 2;
                        ast_global_t *new_globals = malloc(sizeof(ast_global_t) * ast->globals_capacity);
                        memcpy(new_globals, ast->globals, sizeof(ast_global_t) * ast->globals_length);
                        free(ast->globals);
                        ast->globals = new_globals;
                    }
                }

                ast_global_t *global = &ast->globals[ast->globals_length++];
                global->name = global_name;
                global->type = global_type;
                global->initial = global_initial;
                global->source = source;

                state = PARSE_STATE_IDLE;
            }
            break;
        case PARSE_STATE_ALIAS: {
                ast_type_t alias_type;
                source = sources[i - 1];

                char *alias_name = parse_eat_word(ctx, "Expected alias name after 'alias' keyword");
                if (alias_name == NULL) return 1;

                if(parse_eat(ctx, TOKEN_ASSIGN, "Expected '=' after alias name")) return 1;
                if(parse_type(ctx, &alias_type)) return 1;

                expand((void**) &ast->aliases, sizeof(ast_alias_t), ast->aliases_length, &ast->aliases_capacity, 1, 8);

                ast_alias_t *alias = &ast->aliases[ast->aliases_length++];
                alias->name = alias_name;
                alias->type = alias_type;
                alias->traits = TRAIT_NONE;
                alias->source = source;

                state = PARSE_STATE_IDLE;
            }
            break;
        default:
            compiler_panic(ctx->compiler, sources[i], "Unimplemented parse state");
            return 1;
        }
    }

    if(state != PARSE_STATE_IDLE){
        compiler_panic(ctx->compiler, sources[i - 1], "Unfinished code formation (Could be result of mismatched brackets)");
        return 1;
    }

    return 0;
}

int parse_type(parse_ctx_t *ctx, ast_type_t *out_type){
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
                return 1;
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
        return 1;
    }

    out_type->source = sources[start];
    out_type->elements_length++;
    return 0;
}

int parse_type_func(parse_ctx_t *ctx, ast_elem_func_t *out_func_elem){
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

    if(parse_eat(ctx, TOKEN_FUNC, "Expected 'func' keyword in function type")) return 1;
    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after 'func' keyword in type")) return 1;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(is_vararg){
            compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            return 1;
        }

        if(args_capacity == 0){
            out_func_elem->arg_types = malloc(sizeof(ast_type_t) * 4);
            out_func_elem->arg_flows = malloc(sizeof(char) * 4);
            args_capacity = 4;
        } else if(out_func_elem->arity == args_capacity){
            args_capacity *= 2;
            ast_type_t *new_arg_types = malloc(sizeof(ast_type_t) * args_capacity);
            char *new_arg_flows = malloc(sizeof(char) * args_capacity);
            memcpy(new_arg_types, out_func_elem->arg_types, sizeof(ast_type_t) * out_func_elem->arity);
            memcpy(new_arg_flows, out_func_elem->arg_flows, sizeof(char) * out_func_elem->arity);
            free(out_func_elem->arg_types);
            free(out_func_elem->arg_flows);
            out_func_elem->arg_types = new_arg_types;
            out_func_elem->arg_flows = new_arg_flows;
        }

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
                return 1;
            }
            out_func_elem->arity++;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
                ast_types_free(out_func_elem->arg_types, out_func_elem->arity);
                free(out_func_elem->arg_types);
                free(out_func_elem->arg_flows);
                return 1;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            if(is_vararg) compiler_panic(ctx->compiler, sources[*i], "Expected ')' after variadic argument");
            else compiler_panic(ctx->compiler, sources[*i], "Expected ',' after argument type");
            ast_types_free(out_func_elem->arg_types, out_func_elem->arity);
            free(out_func_elem->arg_types);
            free(out_func_elem->arg_flows);
            return 1;
        }
    }

    out_func_elem->return_type = malloc(sizeof(ast_type_t)); (*i)++;
    out_func_elem->return_type->elements = NULL;
    out_func_elem->return_type->elements_length = 0;
    out_func_elem->return_type->source.index = 0;
    out_func_elem->return_type->source.object_index = ctx->object->index;

    if(parse_type(ctx, out_func_elem->return_type)){
        ast_types_free_fully(out_func_elem->arg_types, out_func_elem->arity);
        ast_type_free_fully(out_func_elem->return_type);
        free(out_func_elem->arg_flows);
        return 1;
    }
    return 0;
}

int parse_expr(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Expects first token of expression to be pointed to by 'i'
    // NOTE: 'out_expr' is not guaranteed to be in the same state
    // NOTE: If successful, 'i' will point to the first token after the expression parsed

    if(parse_primary_expr(ctx, out_expr)) return 1;
    if(parse_op_expr(ctx, 0, out_expr, false)) return 1;
    return 0;
}

int parse_primary_expr(parse_ctx_t *ctx, ast_expr_t **out_expr){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    #define LITERAL_TO_EXPR(expr_type, expr_id, storage_type) { \
        *out_expr = malloc(sizeof(expr_type)); \
        ((expr_type*) *out_expr)->id = expr_id; \
        ((expr_type*) *out_expr)->value = *((storage_type*) tokens[*i].data); \
        ((expr_type*) *out_expr)->source = sources[(*i)++]; \
    }

    switch(tokens[*i].id){
    case TOKEN_BYTE:
        LITERAL_TO_EXPR(ast_expr_byte_t, EXPR_BYTE, char); break;
    case TOKEN_UBYTE:
        LITERAL_TO_EXPR(ast_expr_ubyte_t, EXPR_UBYTE, unsigned char); break;
    case TOKEN_SHORT:
        LITERAL_TO_EXPR(ast_expr_short_t, EXPR_SHORT, short); break;
    case TOKEN_USHORT:
        LITERAL_TO_EXPR(ast_expr_ushort_t, EXPR_USHORT, unsigned short); break;
    case TOKEN_INT:
        LITERAL_TO_EXPR(ast_expr_int_t, EXPR_INT, long long); break;
    case TOKEN_UINT:
        LITERAL_TO_EXPR(ast_expr_uint_t, EXPR_UINT, unsigned long long); break;
    case TOKEN_GENERIC_INT:
        LITERAL_TO_EXPR(ast_expr_generic_int_t, EXPR_GENERIC_INT, long long); break;
    case TOKEN_LONG:
        LITERAL_TO_EXPR(ast_expr_long_t, EXPR_LONG, long long); break;
    case TOKEN_ULONG:
        LITERAL_TO_EXPR(ast_expr_ulong_t, EXPR_ULONG, unsigned long long); break;
    case TOKEN_FLOAT:
        LITERAL_TO_EXPR(ast_expr_float_t, EXPR_FLOAT, float); break;
    case TOKEN_DOUBLE:
        LITERAL_TO_EXPR(ast_expr_double_t, EXPR_DOUBLE, double); break;
    case TOKEN_TRUE:
        *out_expr = malloc(sizeof(ast_expr_boolean_t));
        ((ast_expr_boolean_t*) *out_expr)->id = EXPR_BOOLEAN;
        ((ast_expr_boolean_t*) *out_expr)->value = true;
        ((ast_expr_boolean_t*) *out_expr)->source = sources[(*i)++];
        break;
    case TOKEN_FALSE:
        *out_expr = malloc(sizeof(ast_expr_boolean_t));
        ((ast_expr_boolean_t*) *out_expr)->id = EXPR_BOOLEAN;
        ((ast_expr_boolean_t*) *out_expr)->value = false;
        ((ast_expr_boolean_t*) *out_expr)->source = sources[(*i)++];
        break;
    case TOKEN_GENERIC_FLOAT:
        LITERAL_TO_EXPR(ast_expr_generic_float_t, EXPR_GENERIC_FLOAT, double); break;
    case TOKEN_CSTRING:
        *out_expr = malloc(sizeof(ast_expr_cstr_t));
        ((ast_expr_cstr_t*) *out_expr)->id = EXPR_CSTR;
        ((ast_expr_cstr_t*) *out_expr)->value = tokens[*i].data; // Token will live on
        ((ast_expr_cstr_t*) *out_expr)->source = sources[(*i)++];
        break;
    case TOKEN_NULL:
        *out_expr = malloc(sizeof(ast_expr_null_t));
        ((ast_expr_null_t*) *out_expr)->id = EXPR_NULL;
        ((ast_expr_null_t*) *out_expr)->source = sources[(*i)++];
        break;
    case TOKEN_WORD:
        if(tokens[*i + 1].id != TOKEN_OPEN){
            *out_expr = malloc(sizeof(ast_expr_variable_t));
            ((ast_expr_variable_t*) *out_expr)->id = EXPR_VARIABLE;
            ((ast_expr_variable_t*) *out_expr)->name = tokens[*i].data; // Token will live on
            ((ast_expr_variable_t*) *out_expr)->source = sources[(*i)++];
        } else {
            // Function call
            ast_expr_call_t *call_expr = malloc(sizeof(ast_expr_call_t));
            call_expr->id = EXPR_CALL;
            call_expr->name = tokens[*i].data; // Token will live on
            call_expr->source = sources[*i];
            call_expr->arity = 0;
            call_expr->args = NULL;

            ast_expr_t *arg_expr;
            length_t args_capacity = 0;
            *i += 2;

            while(tokens[*i].id != TOKEN_CLOSE){
                if(parse_expr(ctx, &arg_expr)){
                    ast_exprs_free_fully(call_expr->args, call_expr->arity);
                    free(call_expr);
                    return 1;
                }

                // Allocate room for more arguments if necessary
                if(call_expr->arity == args_capacity){
                    if(args_capacity == 0){
                        call_expr->args = malloc(sizeof(ast_expr_t*) * 4);
                        args_capacity = 4;
                    } else {
                        args_capacity *= 2;
                        ast_expr_t **new_args = malloc(sizeof(ast_expr_t*) * args_capacity);
                        memcpy(new_args, call_expr->args, sizeof(ast_expr_t*) * call_expr->arity);
                        free(call_expr->args);
                        call_expr->args = new_args;
                    }
                }

                call_expr->args[call_expr->arity++] = arg_expr;
                if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")) return 1;

                if(tokens[*i].id == TOKEN_NEXT) (*i)++;
                else if(tokens[*i].id != TOKEN_CLOSE){
                    compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
                    return 1;
                }
            }

            *out_expr = (ast_expr_t*) call_expr;
            (*i)++;
        }
        break;
    case TOKEN_OPEN:
        (*i)++;
        if(parse_expr(ctx, out_expr) != 0) return 1;
        if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after expression")) return 1;
        break;
    case TOKEN_ADDRESS: {
            ast_expr_address_t *addr_expr = malloc(sizeof(ast_expr_address_t));
            addr_expr->id = EXPR_ADDRESS;
            addr_expr->source = sources[(*i)++];
            if(parse_primary_expr(ctx, &addr_expr->value) || parse_op_expr(ctx, 0, &addr_expr->value, true)){
                free(addr_expr);
                return 1;
            }
            if(!EXPR_IS_MUTABLE(addr_expr->value->id)){
                compiler_panic(ctx->compiler, addr_expr->value->source, "The '&' operator requires the operand to be mutable");
                free(addr_expr);
                return 1;
            }
            *out_expr = (ast_expr_t*) addr_expr;
        }
        break;
    case TOKEN_FUNC: { // Function address operator
            ast_expr_func_addr_t *func_addr_expr = malloc(sizeof(ast_expr_func_addr_t));

            func_addr_expr->id = EXPR_FUNC_ADDR;
            func_addr_expr->source = sources[(*i)++];
            func_addr_expr->traits = TRAIT_NONE;
            func_addr_expr->match_args = NULL;
            func_addr_expr->match_args_length = 0;

            if(parse_eat(ctx, TOKEN_ADDRESS, "Expected '&' after 'func' keyword in expression")){
                free(func_addr_expr);
                return 1;
            }

            func_addr_expr->name = parse_eat_word(ctx, "Expected function name after 'func &' operator");

            if(func_addr_expr->name == NULL){
                free(func_addr_expr);
                return 1;
            }

            // TODO: Add support for match args
            compiler_warn(ctx->compiler, sources[*i], "Match args not supported yet so 'func &' might return wrong function");
            *out_expr = (ast_expr_t*) func_addr_expr;
        }
        break;
    case TOKEN_MULTIPLY: { // Dereference
            ast_expr_deref_t *deref_expr = malloc(sizeof(ast_expr_deref_t));
            deref_expr->id = EXPR_DEREFERENCE;
            deref_expr->source = sources[(*i)++];
            if(parse_primary_expr(ctx, &deref_expr->value) || parse_op_expr(ctx, 0, &deref_expr->value, true)){
                free(deref_expr);
                return 1;
            }
            *out_expr = (ast_expr_t*) deref_expr;
        }
        break;
    case TOKEN_CAST: {
            ast_expr_cast_t *cast_expr = malloc(sizeof(ast_expr_cast_t));
            ast_type_t to_type;
            ast_expr_t *from_value;

            cast_expr->id = EXPR_CAST;
            cast_expr->source = sources[(*i)++];

            if(parse_type(ctx, &to_type)){
                free(cast_expr);
                return 1;
            }

            if(tokens[*i].id == TOKEN_OPEN){
                // cast <type> (value)
                (*i)++; if(parse_expr(ctx, &from_value)){
                    ast_type_free(&to_type);
                    free(cast_expr);
                    return 1;
                }
                if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after expression given to 'cast'")){
                    ast_type_free(&to_type);
                    free(cast_expr);
                    return 1;
                }
            } else if(parse_primary_expr(ctx, &from_value)){ // cast <type> value
                ast_type_free(&to_type);
                free(cast_expr);
                return 1;
            }

            cast_expr->to = to_type;
            cast_expr->from = from_value;
            *out_expr = (ast_expr_t*) cast_expr;
        }
        break;
    case TOKEN_SIZEOF: {
            ast_expr_sizeof_t *sizeof_expr = malloc(sizeof(ast_expr_sizeof_t));
            sizeof_expr->id = EXPR_SIZEOF;
            sizeof_expr->source = sources[(*i)++];

            if(parse_type(ctx, &sizeof_expr->type)){
                free(sizeof_expr);
                return 1;
            }

            *out_expr = (ast_expr_t*) sizeof_expr;
        }
        break;
    case TOKEN_NOT: {
            ast_expr_not_t *not_expr = malloc(sizeof(ast_expr_not_t));
            not_expr->id = EXPR_NOT;
            not_expr->source = sources[(*i)++];

            if(parse_primary_expr(ctx, &not_expr->value) != 0) return 1;
            *out_expr = (ast_expr_t*) not_expr;
        }
        break;
    case TOKEN_NEW: {
            ast_expr_new_t *new_expr = malloc(sizeof(ast_expr_new_t));
            new_expr->id = EXPR_NEW;
            new_expr->source = sources[(*i)++];
            new_expr->amount = NULL;

            if(parse_type(ctx, &new_expr->type)){
                free(new_expr);
                return 1;
            }

            if(tokens[*i].id == TOKEN_MULTIPLY){
                (*i)++; if(parse_primary_expr(ctx, &new_expr->amount)){
                    ast_type_free(&new_expr->type);
                    free(new_expr);
                    return 1;
                }
            }

            *out_expr = (ast_expr_t*) new_expr;
        }
        break;
    default:
        parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unexpected token '%s' in expression");
        return 1;
    }

    // Handle [] and '.' operators
    bool parsing_post_primary = true;

    while(parsing_post_primary){
        switch(tokens[*i].id){
        case TOKEN_BRACKET_OPEN: {
                ast_expr_t *index_expr;
                ast_expr_array_access_t *array_access_expr = malloc(sizeof(ast_expr_array_access_t));
                array_access_expr->source = sources[(*i)++];

                if(parse_expr(ctx, &index_expr)){
                    ast_expr_free_fully(*out_expr);
                    free(array_access_expr);
                    return 1;
                }

                if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']' after array index expression")){
                    ast_expr_free_fully(*out_expr);
                    ast_expr_free_fully(index_expr);
                    free(array_access_expr);
                    return 1;
                }

                array_access_expr->id = EXPR_ARRAY_ACCESS;
                array_access_expr->value = *out_expr;
                array_access_expr->index = index_expr;
                *out_expr = (ast_expr_t*) array_access_expr;
            }
            break;
        case TOKEN_MEMBER: {
                if(tokens[++(*i)].id != TOKEN_WORD){
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected identifier after '.' operator");
                    ast_expr_free_fully(*out_expr);
                    return 1;
                }

                if(tokens[*i + 1].id == TOKEN_OPEN){
                    // Method call expression
                    ast_expr_call_method_t *call_expr = malloc(sizeof(ast_expr_call_method_t));
                    call_expr->id = EXPR_CALL_METHOD;
                    call_expr->value = *out_expr;
                    call_expr->name = (char*) tokens[*i].data;
                    call_expr->source = sources[*i - 2];
                    call_expr->arity = 0;
                    call_expr->args = NULL;
                    *i += 2;

                    // value.method(arg1, arg2, ...)
                    //              ^

                    ast_expr_t *arg_expr;
                    length_t args_capacity = 0;

                    while(tokens[*i].id != TOKEN_CLOSE){
                        if(parse_ignore_newlines(ctx, "Expected method argument")){
                            ast_expr_free_fully(*out_expr);
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return 1;
                        }

                        if(parse_expr(ctx, &arg_expr)){
                            ast_expr_free_fully(*out_expr);
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return 1;
                        }

                        // Allocate room for more arguments if necessary
                        if(call_expr->arity == args_capacity){
                            if(args_capacity == 0){
                                call_expr->args = malloc(sizeof(ast_expr_t*) * 4);
                                args_capacity = 4;
                            } else {
                                args_capacity *= 2;
                                ast_expr_t **new_args = malloc(sizeof(ast_expr_t*) * args_capacity);
                                memcpy(new_args, call_expr->args, sizeof(ast_expr_t*) * call_expr->arity);
                                free(call_expr->args);
                                call_expr->args = new_args;
                            }
                        }

                        call_expr->args[call_expr->arity++] = arg_expr;
                        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")){
                            ast_expr_free_fully(*out_expr);
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return 1;
                        }

                        if(tokens[*i].id == TOKEN_NEXT) (*i)++;
                        else if(tokens[*i].id != TOKEN_CLOSE){
                            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
                            ast_expr_free_fully(*out_expr);
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return 1;
                        }
                    }

                    *out_expr = (ast_expr_t*) call_expr;
                    (*i)++;
                } else {
                    // Member access expression
                    ast_expr_member_t *memb_expr = malloc(sizeof(ast_expr_member_t));
                    memb_expr->id = EXPR_MEMBER;
                    memb_expr->value = *out_expr;
                    memb_expr->member = (char*) tokens[*i].data;
                    memb_expr->source = sources[*i - 1];
                    tokens[(*i)++].data = NULL; // Take ownership
                    *out_expr = (ast_expr_t*) memb_expr;
                }
            }
            break;
        default: parsing_post_primary = false;
        }
    }

    #undef LITERAL_TO_EXPR
    return 0;
}

int parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t** inout_left, bool keep_mutable){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    ast_expr_t *right, *expr;

    while(*i != ctx->tokenlist->length) {
        int operator_precedence =  parse_get_precedence(tokens[*i].id);

        if(operator_precedence < precedence) return 0;
        int operator = tokens[*i].id;
        source_t source = sources[*i];

        if(keep_mutable) return 0;

        if(operator == TOKEN_NEWLINE || operator == TOKEN_END || operator == TOKEN_CLOSE
            || operator == TOKEN_NEXT || operator == TOKEN_TERMINATE_JOIN
            || operator == TOKEN_BEGIN || operator == TOKEN_BRACKET_CLOSE
            || operator == TOKEN_ASSIGN || operator == TOKEN_ADDASSIGN
            || operator == TOKEN_SUBTRACTASSIGN || operator == TOKEN_MULTIPLYASSIGN
            || operator == TOKEN_DIVIDEASSIGN || operator == TOKEN_MODULUSASSIGN
            || operator == TOKEN_ELSE) return 0;

        #define BUILD_MATH_EXPR_MACRO(new_built_expr_id) { \
            if(parse_rhs_expr(ctx, inout_left, &right, operator_precedence)) return 1; \
            expr = malloc(sizeof(ast_expr_math_t)); \
            ((ast_expr_math_t*) expr)->id = new_built_expr_id; \
            ((ast_expr_math_t*) expr)->a = *inout_left; \
            ((ast_expr_math_t*) expr)->b = right; \
            ((ast_expr_math_t*) expr)->source = source; \
            *inout_left = expr; \
        }

        switch(operator){
        case TOKEN_ADD:           BUILD_MATH_EXPR_MACRO(EXPR_ADD);       break;
        case TOKEN_SUBTRACT:      BUILD_MATH_EXPR_MACRO(EXPR_SUBTRACT);  break;
        case TOKEN_MULTIPLY:      BUILD_MATH_EXPR_MACRO(EXPR_MULTIPLY);  break;
        case TOKEN_DIVIDE:        BUILD_MATH_EXPR_MACRO(EXPR_DIVIDE);    break;
        case TOKEN_MODULUS:       BUILD_MATH_EXPR_MACRO(EXPR_MODULUS);   break;
        case TOKEN_EQUALS:        BUILD_MATH_EXPR_MACRO(EXPR_EQUALS);    break;
        case TOKEN_NOTEQUALS:     BUILD_MATH_EXPR_MACRO(EXPR_NOTEQUALS); break;
        case TOKEN_GREATERTHAN:   BUILD_MATH_EXPR_MACRO(EXPR_GREATER);   break;
        case TOKEN_LESSTHAN:      BUILD_MATH_EXPR_MACRO(EXPR_LESSER);    break;
        case TOKEN_GREATERTHANEQ: BUILD_MATH_EXPR_MACRO(EXPR_GREATEREQ); break;
        case TOKEN_LESSTHANEQ:    BUILD_MATH_EXPR_MACRO(EXPR_LESSEREQ);  break;
        case TOKEN_AND:
        case TOKEN_UBERAND:       BUILD_MATH_EXPR_MACRO(EXPR_AND);       break;
        case TOKEN_OR:
        case TOKEN_UBEROR:        BUILD_MATH_EXPR_MACRO(EXPR_OR);        break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unrecognized operator '%s' in expression");
            ast_expr_free_fully(*inout_left);
            return 1;
        }

        #undef BUILD_MATH_EXPR_MACRO
    }

    return 0;
}

int parse_rhs_expr(parse_ctx_t *ctx, ast_expr_t **left, ast_expr_t **out_right, int op_prec){
    // Expects from 'ctx': compiler, object, tokenlist, i

    // NOTE: Handles all of the work with parsing the right hand side of an expression
    // All errors are taken care of inside this function
    // Freeing of 'left' expression performed on error
    // Expects 'i' to point to the operator token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    (*i)++; // Skip over operator token

    if(parse_ignore_newlines(ctx, "Unexpected expression termination")) return 1;

    if(parse_primary_expr(ctx, out_right) || (op_prec < parse_get_precedence(tokens[*i].id) && parse_op_expr(ctx, op_prec + 1, out_right, false))){
        ast_expr_free_fully(*left);
        return 1;
    }
    return 0;
}

int parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_expr_list_t *defer_list, trait_t mode){
    // NOTE: Outputs statements to stmt_list
    // NOTE: Ends on 'i' pointing to a '}' token
    // NOTE: Even if this function returns 1, statements appended to stmt_list still must be freed

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    source_t source;

    source.index = 0;
    source.object_index = ctx->object->index;

    while(tokens[*i].id != TOKEN_END){
        if(parse_ignore_newlines(ctx, "Unexpected expression termination")) return 1;
        expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, 1, 8);

        // Parse a statement into the statement list
        switch(tokens[*i].id){
        case TOKEN_END:
            return 0;
        case TOKEN_RETURN: {
                ast_expr_t *return_expression;
                source = sources[(*i)++]; // Pass over return keyword

                if(tokens[*i].id == TOKEN_NEWLINE) return_expression = NULL;
                else if(parse_expr(ctx, &return_expression)) return 1;

                ast_expr_return_t *stmt = malloc(sizeof(ast_expr_return_t));
                stmt->id = EXPR_RETURN;
                stmt->source = source;
                stmt->value = return_expression;

                // Unravel all remaining defer statements
                parse_unravel_defer_stmts(stmt_list, defer_list, 0);

                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_WORD: {
                source = sources[(*i)++]; // Read ahead to see what type of statement this is

                switch(tokens[*i].id){
                case TOKEN_OPEN:
                    (*i)++; if(parse_stmt_call(ctx, stmt_list)) return 1;
                    break;
                case TOKEN_WORD: case TOKEN_FUNC:
                case TOKEN_STDCALL: case TOKEN_NEXT:
                case TOKEN_GENERIC_INT: /*fixed array*/ case TOKEN_MULTIPLY: /*pointer*/
                    (*i)--; if(parse_stmt_declare(ctx, stmt_list)) return 1;
                    break;
                default: { // Assume assign statement if not one of the above
                        (*i)--;
                        ast_expr_t *mutable_expression;
                        if(parse_expr(ctx, &mutable_expression)) return 1;

                        // If it's a call method expression, bypass and treat as statement
                        if(mutable_expression->id == EXPR_CALL_METHOD){
                            stmt_list->statements[stmt_list->length++] = (ast_expr_t*) mutable_expression;
                            break;
                        }

                        unsigned int id = tokens[(*i)++].id;
                        if(id != TOKEN_ASSIGN && id != TOKEN_ADDASSIGN
                        && id != TOKEN_SUBTRACTASSIGN && id != TOKEN_MULTIPLYASSIGN
                        && id != TOKEN_DIVIDEASSIGN && id != TOKEN_MODULUSASSIGN){
                            compiler_panic(ctx->compiler, sources[(*i) - 1], "Expected assignment operator after expression");
                            ast_expr_free_fully(mutable_expression);
                            return 1;
                        }

                        if(!EXPR_IS_MUTABLE(mutable_expression->id)){
                            compiler_panic(ctx->compiler, sources[*i], "Can't modify expression because it is immutable");
                            ast_expr_free_fully(mutable_expression);
                            return 1;
                        }

                        ast_expr_t *value_expression;
                        if(parse_expr(ctx, &value_expression)){
                            ast_expr_free_fully(mutable_expression);
                            return 1;
                        }

                        unsigned int stmt_id;
                        switch(id){
                        case TOKEN_ASSIGN:         stmt_id = EXPR_ASSIGN;         break;
                        case TOKEN_ADDASSIGN:      stmt_id = EXPR_ADDASSIGN;      break;
                        case TOKEN_SUBTRACTASSIGN: stmt_id = EXPR_SUBTRACTASSIGN; break;
                        case TOKEN_MULTIPLYASSIGN: stmt_id = EXPR_MULTIPLYASSIGN; break;
                        case TOKEN_DIVIDEASSIGN:   stmt_id = EXPR_DIVIDEASSIGN;   break;
                        case TOKEN_MODULUSASSIGN:  stmt_id = EXPR_MODULUSASSIGN;  break;
                        default:
                            compiler_panic(ctx->compiler, sources[*i], "INTERNAL ERROR: parse_stmts() came across unknown assignment operator");
                            ast_expr_free_fully(mutable_expression);
                            ast_expr_free_fully(value_expression);
                            return 1;
                        }

                        ast_expr_assign_t *stmt = malloc(sizeof(ast_expr_assign_t));
                        stmt->id = stmt_id;
                        stmt->source = source;
                        stmt->destination = mutable_expression;
                        stmt->value = value_expression;
                        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                    }
                    break;
                }
            }
            break;
        case TOKEN_MULTIPLY: case TOKEN_OPEN: {
                source = sources[*i];
                ast_expr_t *mutable_expression;
                if(parse_expr(ctx, &mutable_expression)) return 1;

                unsigned int id = tokens[(*i)++].id;
                if(id != TOKEN_ASSIGN && id != TOKEN_ADDASSIGN && id != TOKEN_SUBTRACTASSIGN &&
                        id != TOKEN_MULTIPLYASSIGN && id != TOKEN_DIVIDEASSIGN && id != TOKEN_MODULUSASSIGN){
                    compiler_panic(ctx->compiler, sources[(*i) - 1], "Expected assignment operator after expression");
                    ast_expr_free_fully(mutable_expression);
                    return 1;
                }

                if(!EXPR_IS_MUTABLE(mutable_expression->id)){
                    compiler_panic(ctx->compiler, sources[*i], "Can't modify expression because it is immutable");
                    ast_expr_free_fully(mutable_expression);
                    return 1;
                }

                ast_expr_t *value_expression;
                if(parse_expr(ctx, &value_expression)){
                    ast_expr_free_fully(mutable_expression);
                    return 1;
                }

                unsigned int stmt_id;
                switch(id){
                case TOKEN_ASSIGN: stmt_id = EXPR_ASSIGN; break;
                case TOKEN_ADDASSIGN: stmt_id = EXPR_ADDASSIGN; break;
                case TOKEN_SUBTRACTASSIGN: stmt_id = EXPR_SUBTRACTASSIGN; break;
                case TOKEN_MULTIPLYASSIGN: stmt_id = EXPR_MULTIPLYASSIGN; break;
                case TOKEN_DIVIDEASSIGN: stmt_id = EXPR_DIVIDEASSIGN; break;
                case TOKEN_MODULUSASSIGN: stmt_id = EXPR_MODULUSASSIGN; break;
                default:
                    compiler_panic(ctx->compiler, sources[*i], "INTERNAL ERROR: parse_stmts() came across unknown assignment operator");
                    ast_expr_free_fully(mutable_expression);
                    return 1;
                }

                ast_expr_assign_t *stmt = malloc(sizeof(ast_expr_assign_t));
                stmt->id = stmt_id;
                stmt->source = source;
                stmt->destination = mutable_expression;
                stmt->value = value_expression;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_IF: case TOKEN_UNLESS: {
                unsigned int conditional_type = tokens[*i].id;
                source = sources[(*i)++];
                ast_expr_t *conditional;
                trait_t stmts_mode;

                if(parse_expr(ctx, &conditional)) return 1;

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after conditional expression");
                    ast_expr_free_fully(conditional);
                    return 1;
                }

                ast_expr_list_t if_stmt_list;
                if_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                if_stmt_list.length = 0;
                if_stmt_list.capacity = 4;

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &if_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(if_stmt_list.statements, if_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    return 1;
                }

                // Unravel all defer statements added in the block
                parse_unravel_defer_stmts(&if_stmt_list, defer_list, defer_unravel_length);

                if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;

                // Read ahead of newlines to check for 'else'
                length_t i_readahead = *i;
                while(tokens[i_readahead].id == TOKEN_NEWLINE) i_readahead++;

                if(tokens[i_readahead].id == TOKEN_ELSE){
                    *i = i_readahead;

                    switch(tokens[++(*i)].id){
                    case TOKEN_NEXT:
                        stmts_mode = PARSE_STMTS_SINGLE;
                        (*i)++; break;
                    case TOKEN_BEGIN:
                        stmts_mode = PARSE_STMTS_STANDARD;
                        (*i)++; break;
                    default:
                        stmts_mode = PARSE_STMTS_SINGLE;
                    }

                    ast_expr_list_t else_stmt_list;
                    else_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                    else_stmt_list.length = 0;
                    else_stmt_list.capacity = 4;

                    defer_unravel_length = defer_list->length;

                    if(parse_stmts(ctx, &else_stmt_list, defer_list, stmts_mode)){
                        ast_free_statements_fully(else_stmt_list.statements, else_stmt_list.length);
                        ast_free_statements_fully(if_stmt_list.statements, if_stmt_list.length);
                        ast_expr_free_fully(conditional);
                        return 1;
                    }

                    // Unravel all defer statements added in the block
                    parse_unravel_defer_stmts(&else_stmt_list, defer_list, defer_unravel_length);

                    if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;
                    else if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;

                    ast_expr_ifelse_t *stmt = malloc(sizeof(ast_expr_ifelse_t));
                    stmt->id = (conditional_type == TOKEN_UNLESS) ? EXPR_UNLESSELSE : EXPR_IFELSE;
                    stmt->source = source;
                    stmt->label = NULL;
                    stmt->value = conditional;
                    stmt->statements = if_stmt_list.statements;
                    stmt->statements_length = if_stmt_list.length;
                    stmt->statements_capacity = if_stmt_list.capacity;
                    stmt->else_statements = else_stmt_list.statements;
                    stmt->else_statements_length = else_stmt_list.length;
                    stmt->else_statements_capacity = else_stmt_list.capacity;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;
                    ast_expr_if_t *stmt = malloc(sizeof(ast_expr_if_t));
                    stmt->id = (conditional_type == TOKEN_UNLESS) ? EXPR_UNLESS : EXPR_IF;
                    stmt->source = source;
                    stmt->label = NULL;
                    stmt->value = conditional;
                    stmt->statements = if_stmt_list.statements;
                    stmt->statements_length = if_stmt_list.length;
                    stmt->statements_capacity = if_stmt_list.capacity;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                }
            }
            break;
        case TOKEN_WHILE: case TOKEN_UNTIL: {
                unsigned int conditional_type = tokens[*i].id;
                source = sources[(*i)++];
                ast_expr_t *conditional = NULL;
                trait_t stmts_mode;
                char *label = NULL;

                if(tokens[*i].id == TOKEN_BREAK || tokens[*i].id == TOKEN_CONTINUE){
                    // 'while continue' or 'until break' loop
                    unsigned int condition_break_or_continue = tokens[*i].id;

                    if(conditional_type == TOKEN_WHILE && condition_break_or_continue != TOKEN_CONTINUE){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Did you mean to use 'while continue'? There is no such conditional as 'while break'");
                        return 1;
                    } else if(conditional_type == TOKEN_UNTIL && condition_break_or_continue != TOKEN_BREAK){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Did you mean to use 'until break'? There is no such conditional as 'until continue'");
                        return 1;
                    }

                    if(tokens[++(*i)].id == TOKEN_WORD){
                        label = tokens[(*i)++].data;
                    }
                } else {
                    // standard 'while <expr>' or 'until <expr>' loop
                    if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                        label = tokens[*i].data; *i += 2;
                    }

                    if(parse_expr(ctx, &conditional)) return 1;
                }


                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after conditional expression");
                    ast_expr_free_fully(conditional);
                    return 1;
                }

                ast_expr_list_t while_stmt_list;
                while_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                while_stmt_list.length = 0;
                while_stmt_list.capacity = 4;

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &while_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(while_stmt_list.statements, while_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    return 1;
                }

                // Unravel all defer statements added in the block
                parse_unravel_defer_stmts(&while_stmt_list, defer_list, defer_unravel_length);

                if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;
                else if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;

                if(conditional == NULL){
                    // 'while continue' or 'until break' loop
                    ast_expr_whilecontinue_t *stmt = malloc(sizeof(ast_expr_whilecontinue_t));
                    stmt->id = (conditional_type == TOKEN_UNTIL) ? EXPR_UNTILBREAK : EXPR_WHILECONTINUE;
                    stmt->source = source;
                    stmt->label = label;
                    stmt->statements = while_stmt_list.statements;
                    stmt->statements_length = while_stmt_list.length;
                    stmt->statements_capacity = while_stmt_list.capacity;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    // 'while <expr>' or 'until <expr>' loop
                    ast_expr_while_t *stmt = malloc(sizeof(ast_expr_while_t));
                    stmt->id = (conditional_type == TOKEN_UNTIL) ? EXPR_UNTIL : EXPR_WHILE;
                    stmt->source = source;
                    stmt->label = label;
                    stmt->value = conditional;
                    stmt->statements = while_stmt_list.statements;
                    stmt->statements_length = while_stmt_list.length;
                    stmt->statements_capacity = while_stmt_list.capacity;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                }
            }
            break;
        case TOKEN_EACH: {
                source = sources[(*i)++];
                ast_expr_t *length_limit = NULL;
                ast_expr_t *low_array = NULL;
                trait_t stmts_mode;
                char *label = NULL;

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data; *i += 2;
                }

                if(parse_expr(ctx, &length_limit)) return 1;

                if(tokens[*i].id != TOKEN_FOR){
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected 'for' after length expression in 'each for' loop");
                    ast_expr_free_fully(length_limit);
                    return 1;
                }

                if(parse_expr(ctx, &low_array)){
                    ast_expr_free_fully(length_limit);
                    return 1;
                }

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after 'each for' expression");
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(low_array);
                    return 1;
                }

                ast_expr_list_t each_for_stmt_list;
                each_for_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                each_for_stmt_list.length = 0;
                each_for_stmt_list.capacity = 4;

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &each_for_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(each_for_stmt_list.statements, each_for_stmt_list.length);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(low_array);
                    return 1;
                }

                // Unravel all defer statements added in the block
                parse_unravel_defer_stmts(&each_for_stmt_list, defer_list, defer_unravel_length);

                if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;
                else if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;

                ast_expr_each_for_t *stmt = malloc(sizeof(ast_expr_each_for_t));
                stmt->id = EXPR_EACH_FOR;
                stmt->source = source;
                stmt->label = label;
                stmt->length = length_limit;
                stmt->low_array = low_array;
                stmt->statements = each_for_stmt_list.statements;
                stmt->statements_length = each_for_stmt_list.length;
                stmt->statements_capacity = each_for_stmt_list.capacity;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_DEFER: {
                (*i)++; // Skip over 'defer' keyword
                if(parse_stmts(ctx, defer_list, defer_list, PARSE_STMTS_SINGLE)) return 1;
                (*i)--; // Go back to newline because we used PARSE_STMTS_SINGLE
            }
            break;
        case TOKEN_DELETE: {
                ast_expr_delete_t *stmt = malloc(sizeof(ast_expr_delete_t));
                stmt->id = EXPR_DELETE;
                stmt->source = sources[(*i)++];

                if(parse_primary_expr(ctx, &stmt->value) != 0){
                    free(stmt);
                    return 1;
                }
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_BREAK: {
                if(tokens[++(*i)].id == TOKEN_WORD){
                    ast_expr_break_to_t *stmt = malloc(sizeof(ast_expr_break_to_t));
                    stmt->id = EXPR_BREAK_TO;
                    stmt->source = sources[*i - 1];
                    stmt->label_source = sources[*i];
                    stmt->label = tokens[(*i)++].data;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    ast_expr_break_t *stmt = malloc(sizeof(ast_expr_break_t));
                    stmt->id = EXPR_BREAK;
                    stmt->source = sources[*i - 1];
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                }
            }
            break;
        case TOKEN_CONTINUE: {
                if(tokens[++(*i)].id == TOKEN_WORD){
                    ast_expr_continue_to_t *stmt = malloc(sizeof(ast_expr_continue_to_t));
                    stmt->id = EXPR_CONTINUE_TO;
                    stmt->source = sources[*i - 1];
                    stmt->label_source = sources[*i];
                    stmt->label = tokens[(*i)++].data;
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    ast_expr_continue_t *stmt = malloc(sizeof(ast_expr_continue_t));
                    stmt->id = EXPR_CONTINUE;
                    stmt->source = sources[*i - 1];
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                }
            }
            break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at beginning of statement");
            return 1;
        }

        if(tokens[*i].id == TOKEN_TERMINATE_JOIN){
            // Bypass single statement flag by "joining" 2+ statements
            (*i)++; continue;
        }

        // Continue over newline token
        if(tokens[*i].id != TOKEN_ELSE && tokens[(*i)++].id != TOKEN_NEWLINE){
            parse_panic_token(ctx, sources[*i - 1], tokens[*i - 1].id, "Encountered unexpected token '%s' at end of statement");
            return 1;
        }

        if(mode & PARSE_STMTS_SINGLE) return 0;
    }

    return 0; // '}' was reached
}

int parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // Expects from 'ctx': compiler, object, tokenlist, i

    // <function_name>( <arguments> )
    //                  ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    ast_expr_t *arg_expr;
    ast_expr_call_t *stmt;
    length_t args_capacity = 0;

    stmt = malloc(sizeof(ast_expr_call_t));
    stmt->id = EXPR_CALL;
    stmt->source = sources[*i - 2];
    stmt->name = (char*) tokens[*i - 2].data;
    stmt->arity = 0;
    stmt->args = NULL;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected function argument")) return 1;

        if(parse_expr(ctx, &arg_expr)){
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt);
            return 1;
        }

        // Allocate room for more arguments if necessary
        if(stmt->arity == args_capacity){
            if(args_capacity == 0){
                stmt->args = malloc(sizeof(ast_expr_t*) * 4);
                args_capacity = 4;
            } else {
                args_capacity *= 2;
                ast_expr_t **new_args = malloc(sizeof(ast_expr_t*) * args_capacity);
                memcpy(new_args, stmt->args, sizeof(ast_expr_t*) * stmt->arity);
                free(stmt->args);
                stmt->args = new_args;
            }
        }

        stmt->args[stmt->arity++] = arg_expr;

        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")) return 1;

        if(tokens[*i].id == TOKEN_NEXT) (*i)++;
        else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt);
            return 1;
        }
    }

    (*i)++;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    return 0;
}

int parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // Expects from 'ctx': compiler, object, tokenlist, i, func

    // <variable_name> <variable_type> [ = expression ]
    //       ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    char **decl_names = NULL;
    source_t *decl_sources = NULL;
    length_t length = 0;
    length_t capacity = 0;

    ast_expr_t *decl_value = NULL;
    ast_type_t decl_type;

    unsigned int declare_stmt_type = EXPR_DECLARE;

    // Collect all variable names & sources into arrays
    while(true){
        coexpand((void**) &decl_names, sizeof(const char*), (void**) &decl_sources,
            sizeof(source_t), length, &capacity, 1, 4);

        if(tokens[*i].id != TOKEN_WORD){
            compiler_panic(ctx->compiler, sources[*i], "Expected variable name");
            free(decl_names);
            free(decl_sources);
            return 1;
        }

        decl_names[length] = tokens[*i].data;
        decl_sources[length++] = sources[(*i)++];

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++; continue;
        } else break;
    }

    // Parse the type for the variable(s)
    if(parse_type(ctx, &decl_type)){
        free(decl_names);
        free(decl_sources);
        return 1;
    }

    // Parse the initial value for the variable(s) (if an initial value is given)
    if(tokens[*i].id == TOKEN_ASSIGN){
        (*i)++; // Skip over 'assign' token

        // Handle either 'undef' or an expression
        if(tokens[*i].id == TOKEN_UNDEF){
            declare_stmt_type = EXPR_DECLAREUNDEF;
            (*i)++;
        } else if(parse_expr(ctx, &decl_value)){
            ast_type_free(&decl_type);
            free(decl_names);
            free(decl_sources);
            return 1;
        }
    }

    // Add each variable to the necessary data sets
    for(length_t v = 0; v != length; v++){
        ast_expr_declare_t *stmt = malloc(sizeof(ast_expr_declare_t));
        stmt->id = declare_stmt_type;
        stmt->source = decl_sources[v];
        stmt->name = decl_names[v];

        if(v + 1 == length){
            stmt->type = decl_type;
            stmt->value = decl_value;
        } else {
            stmt->type = ast_type_clone(&decl_type);
            stmt->value = decl_value == NULL ? NULL : ast_expr_clone(decl_value);
        }

        // Append the created declare statement
        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    }

    free(decl_names);
    free(decl_sources);
    return 0;
}

void parse_unravel_defer_stmts(ast_expr_list_t *stmts, ast_expr_list_t *defer_list, length_t unravel_length){
    // This function will spit out each statement in 'defer_list' into 'stmts' in reverse order
    //     so that defer_list->length will be back at a previously sampled 'unravel_length'

    // ------------------------------------------
    // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
    // -----------------------------------------
    //         ^                               ^
    //  unravel_length (2)         defer_list->length (10)

    // Will spit out statements 9, 8, 7, 6, 5, 4, 3, and 2 in above example (8 in total)

    if(defer_list->length == 0) return;
    expand((void**) &stmts->statements, sizeof(ast_expr_t*), stmts->length, &stmts->capacity, defer_list->length - unravel_length, 8);

    for(length_t len_idx = defer_list->length; len_idx != unravel_length; len_idx--){
        // Spit out statement before 'len_idx' while it hasn't reached the target length of 'unravel_length'
        stmts->statements[stmts->length++] = defer_list->statements[len_idx - 1];
    }

    defer_list->length = unravel_length;
}

int parse_ignore_newlines(parse_ctx_t *ctx, const char *error_message){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    length_t length = ctx->tokenlist->length;

    while(tokens[*i].id == TOKEN_NEWLINE) if(length == (*i)++){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], error_message);
        return 1;
    }

    return 0;
}

int parse_get_precedence(unsigned int id){
    switch(id){
    case TOKEN_UBERAND: case TOKEN_UBEROR:
        return 1;
    case TOKEN_AND: case TOKEN_OR:
        return 2;
    case TOKEN_EQUALS: case TOKEN_NOTEQUALS:
    case TOKEN_LESSTHAN: case TOKEN_GREATERTHAN:
    case TOKEN_LESSTHANEQ: case TOKEN_GREATERTHANEQ:
        return 3;
    case TOKEN_ADD: case TOKEN_SUBTRACT: case TOKEN_WORD:
        return 4;
    case TOKEN_MULTIPLY: case TOKEN_DIVIDE: case TOKEN_MODULUS:
        return 5;
    default:
        return 0;
    }
}

void parse_panic_token(parse_ctx_t *ctx, source_t source, unsigned int token_id, const char *message){
    // Expects from 'ctx': compiler, object
    // Expects 'message' to have a '%s' to display the token name

    int line, column;
    length_t message_length = strlen(message);
    char *format = malloc(message_length + 11);

    if(ctx->object->traits & OBJECT_PACKAGE){
        memcpy(format, "%s: ", 4);
        memcpy(&format[4], message, message_length);
        memcpy(&format[4 + message_length], "!\n", 3);
        redprintf(format, filename_name_const(ctx->object->filename), global_token_name_table[token_id]);
    } else {
        lex_get_location(ctx->object->buffer, source.index, &line, &column);
        memcpy(format, "%s:%d:%d: ", 10);
        memcpy(&format[10], message, message_length);
        memcpy(&format[10 + message_length], "!\n", 3);
        redprintf(format, filename_name_const(ctx->object->filename), line, column, global_token_name_table[token_id]);
    }

    free(format);
    compiler_print_source(ctx->compiler, line, column, source);
}
