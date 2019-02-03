
#include "UTIL/util.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_type.h"

errorcode_t parse_expr(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Expects first token of expression to be pointed to by 'i'
    // NOTE: 'out_expr' is not guaranteed to be in the same state
    // NOTE: If successful, 'i' will point to the first token after the expression parsed

    if(parse_primary_expr(ctx, out_expr)) return FAILURE;
    if(parse_op_expr(ctx, 0, out_expr, false)) return FAILURE;
    return SUCCESS;
}

errorcode_t parse_primary_expr(parse_ctx_t *ctx, ast_expr_t **out_expr){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    #define LITERAL_TO_EXPR(expr_type, expr_id, storage_type){                \
        *out_expr = (ast_expr_t*) malloc(sizeof(expr_type));                  \
        ((expr_type *)*out_expr)->id = expr_id;                               \
        ((expr_type *)*out_expr)->value = *((storage_type *)tokens[*i].data); \
        ((expr_type *)*out_expr)->source = sources[(*i)++];                   \
    }

    switch (tokens[*i].id){
    case TOKEN_BYTE:
        LITERAL_TO_EXPR(ast_expr_byte_t, EXPR_BYTE, char);
        break;
    case TOKEN_UBYTE:
        LITERAL_TO_EXPR(ast_expr_ubyte_t, EXPR_UBYTE, unsigned char);
        break;
    case TOKEN_SHORT:
        LITERAL_TO_EXPR(ast_expr_short_t, EXPR_SHORT, short);
        break;
    case TOKEN_USHORT:
        LITERAL_TO_EXPR(ast_expr_ushort_t, EXPR_USHORT, unsigned short);
        break;
    case TOKEN_INT:
        LITERAL_TO_EXPR(ast_expr_int_t, EXPR_INT, long long);
        break;
    case TOKEN_UINT:
        LITERAL_TO_EXPR(ast_expr_uint_t, EXPR_UINT, unsigned long long);
        break;
    case TOKEN_GENERIC_INT:
        LITERAL_TO_EXPR(ast_expr_generic_int_t, EXPR_GENERIC_INT, long long);
        break;
    case TOKEN_LONG:
        LITERAL_TO_EXPR(ast_expr_long_t, EXPR_LONG, long long);
        break;
    case TOKEN_ULONG:
        LITERAL_TO_EXPR(ast_expr_ulong_t, EXPR_ULONG, unsigned long long);
        break;
    case TOKEN_FLOAT:
        LITERAL_TO_EXPR(ast_expr_float_t, EXPR_FLOAT, float);
        break;
    case TOKEN_DOUBLE:
        LITERAL_TO_EXPR(ast_expr_double_t, EXPR_DOUBLE, double);
        break;
    case TOKEN_GENERIC_FLOAT:
        LITERAL_TO_EXPR(ast_expr_generic_float_t, EXPR_GENERIC_FLOAT, double);
        break;
    case TOKEN_TRUE:
        ast_expr_create_bool(out_expr, true, sources[(*i)++]);
        break;
    case TOKEN_FALSE:
        ast_expr_create_bool(out_expr, false, sources[(*i)++]);
        break;
    case TOKEN_CSTRING:
        ast_expr_create_cstring(out_expr, (char*) tokens[*i].data, sources[*i]);
        *i += 1;
        break;
    case TOKEN_STRING: {
            token_string_data_t *string_data = (token_string_data_t*) tokens[*i].data;
            ast_expr_create_string(out_expr, string_data->array, string_data->length, sources[*i]);
            *i += 1;
        }
        break;
    case TOKEN_NULL:
        ast_expr_create_null(out_expr, sources[(*i)++]);
        break;
    case TOKEN_WORD:
        if(parse_expr_word(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_OPEN:
        (*i)++;
        if(parse_expr(ctx, out_expr) != 0) return FAILURE;
        if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after expression")) return FAILURE;
        break;
    case TOKEN_ADDRESS:
        if(parse_expr_address(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_FUNC:
        if(parse_expr_func_address(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_MULTIPLY:
        if(parse_expr_dereference(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_CAST:
        if(parse_expr_cast(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_SIZEOF:
        if(parse_expr_sizeof(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_NOT:
        if(parse_expr_unary(ctx, EXPR_NOT, out_expr)) return FAILURE;
        break;
    case TOKEN_BIT_COMPLEMENT:
        if(parse_expr_unary(ctx, EXPR_BIT_COMPLEMENT, out_expr)) return FAILURE;
        break;
    case TOKEN_SUBTRACT:
        if(parse_expr_unary(ctx, EXPR_NEGATE, out_expr)) return FAILURE;
        break;
    case TOKEN_NEW:
        if(parse_expr_new(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_STATIC:
        if(parse_expr_static(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_DEF: case TOKEN_UNDEF:
        if(parse_expr_def(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_TYPEINFO:
        if(parse_expr_typeinfo(ctx, out_expr)) return FAILURE;
        break;
    default:
        parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unexpected token '%s' in expression");
        return FAILURE;
    }

    if(parse_expr_post(ctx, out_expr)){
        ast_expr_free_fully(*out_expr);
        return FAILURE;
    }

    #undef LITERAL_TO_EXPR
    return SUCCESS;
}

errorcode_t parse_expr_post(parse_ctx_t *ctx, ast_expr_t **inout_expr){
    // Handle [] and '.' operators etc.

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    while(true){
        switch(tokens[*i].id){
        case TOKEN_BRACKET_OPEN: {
                ast_expr_t *index_expr;
                ast_expr_array_access_t *array_access_expr = malloc(sizeof(ast_expr_array_access_t));
                array_access_expr->source = sources[(*i)++];

                if(parse_expr(ctx, &index_expr)){
                    free(array_access_expr);
                    return FAILURE;
                }

                if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']' after array index expression")){
                    ast_expr_free_fully(index_expr);
                    free(array_access_expr);
                    return FAILURE;
                }

                array_access_expr->id = EXPR_ARRAY_ACCESS;
                array_access_expr->value = *inout_expr;
                array_access_expr->index = index_expr;
                *inout_expr = (ast_expr_t*) array_access_expr;
            }
            break;
        case TOKEN_MEMBER: {
                if(tokens[++(*i)].id != TOKEN_WORD){
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected identifier after '.' operator");
                    return FAILURE;
                }

                if(tokens[*i + 1].id == TOKEN_OPEN){
                    // Method call expression
                    ast_expr_call_method_t *call_expr = malloc(sizeof(ast_expr_call_method_t));
                    call_expr->id = EXPR_CALL_METHOD;
                    call_expr->value = *inout_expr;
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
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }

                        if(parse_expr(ctx, &arg_expr)){
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
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
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }

                        if(tokens[*i].id == TOKEN_NEXT) (*i)++;
                        else if(tokens[*i].id != TOKEN_CLOSE){
                            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }
                    }

                    *inout_expr = (ast_expr_t*) call_expr;
                    (*i)++;
                } else {
                    // Member access expression
                    ast_expr_member_t *memb_expr = malloc(sizeof(ast_expr_member_t));
                    memb_expr->id = EXPR_MEMBER;
                    memb_expr->value = *inout_expr;
                    memb_expr->member = (char*) tokens[*i].data;
                    memb_expr->source = sources[*i - 1];
                    tokens[(*i)++].data = NULL; // Take ownership
                    *inout_expr = (ast_expr_t*) memb_expr;
                }
            }
            break;
        case TOKEN_AT: {
                ast_expr_t *index_expr;
                ast_expr_array_access_t *at_expr = malloc(sizeof(ast_expr_array_access_t));
                at_expr->source = sources[(*i)++];

                if(parse_primary_expr(ctx, &index_expr)){
                    free(at_expr);
                    return FAILURE;
                }

                at_expr->id = EXPR_AT;
                at_expr->value = *inout_expr;
                at_expr->index = index_expr;
                *inout_expr = (ast_expr_t*) at_expr;
            }
            break;
        default:
            return SUCCESS;
        }
    }

    // Should never be reached
    return SUCCESS;
}

errorcode_t parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t **inout_left, bool keep_mutable){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    ast_expr_t *right, *expr;

    while(*i != ctx->tokenlist->length) {
        int operator_precedence =  parse_get_precedence(tokens[*i].id);

        if(operator_precedence < precedence) return SUCCESS;
        int operator = tokens[*i].id;
        source_t source = sources[*i];

        if(keep_mutable) return SUCCESS;

        if(operator == TOKEN_NEWLINE || operator == TOKEN_END || operator == TOKEN_CLOSE
            || operator == TOKEN_NEXT || operator == TOKEN_TERMINATE_JOIN
            || operator == TOKEN_BEGIN || operator == TOKEN_BRACKET_CLOSE
            || operator == TOKEN_ASSIGN || operator == TOKEN_ADDASSIGN
            || operator == TOKEN_SUBTRACTASSIGN || operator == TOKEN_MULTIPLYASSIGN
            || operator == TOKEN_DIVIDEASSIGN || operator == TOKEN_MODULUSASSIGN
            || operator == TOKEN_ELSE) return SUCCESS;

        #define BUILD_MATH_EXPR_MACRO(new_built_expr_id) { \
            if(parse_rhs_expr(ctx, inout_left, &right, operator_precedence)) return FAILURE; \
            expr = malloc(sizeof(ast_expr_math_t)); \
            ((ast_expr_math_t*) expr)->id = new_built_expr_id; \
            ((ast_expr_math_t*) expr)->a = *inout_left; \
            ((ast_expr_math_t*) expr)->b = right; \
            ((ast_expr_math_t*) expr)->source = source; \
            *inout_left = expr; \
        }

        switch(operator){
        case TOKEN_ADD:            BUILD_MATH_EXPR_MACRO(EXPR_ADD);        break;
        case TOKEN_SUBTRACT:       BUILD_MATH_EXPR_MACRO(EXPR_SUBTRACT);   break;
        case TOKEN_MULTIPLY:       BUILD_MATH_EXPR_MACRO(EXPR_MULTIPLY);   break;
        case TOKEN_DIVIDE:         BUILD_MATH_EXPR_MACRO(EXPR_DIVIDE);     break;
        case TOKEN_MODULUS:        BUILD_MATH_EXPR_MACRO(EXPR_MODULUS);    break;
        case TOKEN_EQUALS:         BUILD_MATH_EXPR_MACRO(EXPR_EQUALS);     break;
        case TOKEN_NOTEQUALS:      BUILD_MATH_EXPR_MACRO(EXPR_NOTEQUALS);  break;
        case TOKEN_GREATERTHAN:    BUILD_MATH_EXPR_MACRO(EXPR_GREATER);    break;
        case TOKEN_LESSTHAN:       BUILD_MATH_EXPR_MACRO(EXPR_LESSER);     break;
        case TOKEN_GREATERTHANEQ:  BUILD_MATH_EXPR_MACRO(EXPR_GREATEREQ);  break;
        case TOKEN_LESSTHANEQ:     BUILD_MATH_EXPR_MACRO(EXPR_LESSEREQ);   break;
        case TOKEN_BIT_AND:        BUILD_MATH_EXPR_MACRO(EXPR_BIT_AND);    break;
        case TOKEN_BIT_OR:         BUILD_MATH_EXPR_MACRO(EXPR_BIT_OR);     break;
        case TOKEN_BIT_XOR:        BUILD_MATH_EXPR_MACRO(EXPR_BIT_XOR);    break;
        case TOKEN_BIT_LSHIFT:     BUILD_MATH_EXPR_MACRO(EXPR_BIT_LSHIFT); break;
        case TOKEN_BIT_RSHIFT:     BUILD_MATH_EXPR_MACRO(EXPR_BIT_RSHIFT); break;
        case TOKEN_BIT_LGC_LSHIFT: BUILD_MATH_EXPR_MACRO(EXPR_BIT_LGC_LSHIFT); break;
        case TOKEN_BIT_LGC_RSHIFT: BUILD_MATH_EXPR_MACRO(EXPR_BIT_LGC_RSHIFT); break;
        case TOKEN_AND:
        case TOKEN_UBERAND:        BUILD_MATH_EXPR_MACRO(EXPR_AND);        break;
        case TOKEN_OR:
        case TOKEN_UBEROR:         BUILD_MATH_EXPR_MACRO(EXPR_OR);         break;
        case TOKEN_AS: if(parse_expr_as(ctx, inout_left)) return FAILURE;       break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unrecognized operator '%s' in expression");
            ast_expr_free_fully(*inout_left);
            return FAILURE;
        }

        #undef BUILD_MATH_EXPR_MACRO
    }

    return SUCCESS;
}

errorcode_t parse_rhs_expr(parse_ctx_t *ctx, ast_expr_t **left, ast_expr_t **out_right, int op_prec){
    // Expects from 'ctx': compiler, object, tokenlist, i

    // NOTE: Handles all of the work with parsing the right hand side of an expression
    // All errors are taken care of inside this function
    // Freeing of 'left' expression performed on error
    // Expects 'i' to point to the operator token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    (*i)++; // Skip over operator token

    if(parse_ignore_newlines(ctx, "Unexpected expression termination")) return FAILURE;

    if(parse_primary_expr(ctx, out_right) || (op_prec < parse_get_precedence(tokens[*i].id) && parse_op_expr(ctx, op_prec + 1, out_right, false))){
        ast_expr_free_fully(*left);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_expr_word(parse_ctx_t *ctx, ast_expr_t **out_expr){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    if(tokens[*i + 1].id == TOKEN_OPEN){
        return parse_expr_call(ctx, out_expr);
    }

    if(tokens[*i + 1].id == TOKEN_NAMESPACE){
        return parse_expr_enum_value(ctx, out_expr);
    }

    weak_cstr_t variable_name = tokens[*i].data;
    ast_expr_create_variable(out_expr, variable_name, ctx->tokenlist->sources[(*i)++]);
    return SUCCESS;
}

errorcode_t parse_expr_call(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes name and open token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    weak_cstr_t name = tokens[*i].data;
    source_t source = sources[*i];
    length_t arity = 0;
    ast_expr_t **args = NULL;

    ast_expr_t *arg;
    length_t max_arity = 0;

    // Assume that '(' token follows
    *i += 2;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_expr(ctx, &arg)){
            ast_exprs_free_fully(args, arity);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &args, sizeof(ast_expr_t*), arity, &max_arity, 1, 4);
        args[arity++] = arg;
        
        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")) return FAILURE;

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
            return FAILURE;
        }
    }

    ast_expr_create_call(out_expr, name, arity, args, source);
    *i += 1;
    return SUCCESS;
}

errorcode_t parse_expr_enum_value(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes name and open token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    weak_cstr_t enum_name = tokens[*i].data;
    source_t source = ctx->tokenlist->sources[(*i)++];

    // (Shouldn't fail unless something weird is going on)
    if(parse_eat(ctx, TOKEN_NAMESPACE, "Expected namespace '::' operator for enum value")) return FAILURE;

    weak_cstr_t kind_name = parse_eat_word(ctx, "Expected enum value name after '::' operator");
    if(kind_name == NULL) return FAILURE;

    ast_expr_create_enum_value(out_expr, enum_name, kind_name, source);
    return SUCCESS;
}

errorcode_t parse_expr_address(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_unary_t *addr_expr = malloc(sizeof(ast_expr_unary_t));
    addr_expr->id = EXPR_ADDRESS;
    addr_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_primary_expr(ctx, &addr_expr->value) || parse_op_expr(ctx, 0, &addr_expr->value, true)){
        free(addr_expr);
        return FAILURE;
    }

    if(!EXPR_IS_MUTABLE(addr_expr->value->id)){
        compiler_panic(ctx->compiler, addr_expr->value->source, "The '&' operator requires the operand to be mutable");
        ast_expr_free_fully((ast_expr_t*) addr_expr);
        return FAILURE;
    }
    
    *out_expr = (ast_expr_t*) addr_expr;
    return SUCCESS;
}

int parse_expr_func_address(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_func_addr_t *func_addr_expr = malloc(sizeof(ast_expr_func_addr_t));

    func_addr_expr->id = EXPR_FUNC_ADDR;
    func_addr_expr->source = ctx->tokenlist->sources[(*ctx->i)++];
    func_addr_expr->traits = TRAIT_NONE;
    func_addr_expr->match_args = NULL;
    func_addr_expr->match_args_length = 0;

    if(parse_eat(ctx, TOKEN_ADDRESS, "Expected '&' after 'func' keyword in expression")){
        free(func_addr_expr);
        return FAILURE;
    }

    func_addr_expr->name = parse_eat_word(ctx, "Expected function name after 'func &' operator");

    if(func_addr_expr->name == NULL){
        free(func_addr_expr);
        return FAILURE;
    }

    // TODO: Add support for match args
    compiler_warn(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Match args not supported yet so 'func &' might return wrong function");
    *out_expr = (ast_expr_t*) func_addr_expr;
    return SUCCESS;
}

errorcode_t parse_expr_dereference(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_unary_t *deref_expr = malloc(sizeof(ast_expr_unary_t));
    deref_expr->id = EXPR_DEREFERENCE;
    deref_expr->source = ctx->tokenlist->sources[(*ctx->i)++];
    
    if(parse_primary_expr(ctx, &deref_expr->value) || parse_op_expr(ctx, 0, &deref_expr->value, true)){
        free(deref_expr);
        return FAILURE;
    }

    *out_expr = (ast_expr_t*) deref_expr;
    return SUCCESS;
}

errorcode_t parse_expr_cast(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // cast <type> (value)
    //   ^

    length_t *i = ctx->i;

    ast_type_t to;
    ast_expr_t *from;

    // Assume that expression starts with 'cast' keyword
    source_t source = ctx->tokenlist->sources[(*i)++];

    if(parse_type(ctx, &to)) return FAILURE;

    if(ctx->tokenlist->tokens[*i].id == TOKEN_OPEN){
        (*i)++;
        
        if(parse_expr(ctx, &from)){
            ast_type_free(&to);
            return FAILURE;
        }

        if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after expression given to 'cast'")){
            ast_type_free(&to);
            return FAILURE;
        }
    } else if(parse_primary_expr(ctx, &from)){ // cast <type> value
        ast_type_free(&to);
        return FAILURE;
    }

    ast_expr_cast_t *cast_expr = malloc(sizeof(ast_expr_cast_t));
    cast_expr->id = EXPR_CAST;
    cast_expr->source = source;

    cast_expr->to = to;
    cast_expr->from = from;
    *out_expr = (ast_expr_t*) cast_expr;
    return SUCCESS;
}

errorcode_t parse_expr_as(parse_ctx_t *ctx, ast_expr_t **inout_expr){
    // (value) as <type>
    //         ^

    // Assume that expression starts with 'as' keyword
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    ast_type_t to;
    if(parse_type(ctx, &to)) return FAILURE;

    ast_expr_create_cast(inout_expr, to, *inout_expr, source);
    return SUCCESS;
}

errorcode_t parse_expr_sizeof(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_sizeof_t *sizeof_expr = malloc(sizeof(ast_expr_sizeof_t));
    sizeof_expr->id = EXPR_SIZEOF;
    sizeof_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_type(ctx, &sizeof_expr->type)){
        free(sizeof_expr);
        return FAILURE;
    }

    *out_expr = (ast_expr_t*) sizeof_expr;
    return SUCCESS;
}

errorcode_t parse_expr_unary(parse_ctx_t *ctx, unsigned int expr_id, ast_expr_t **out_expr){
    ast_expr_unary_t *unary_expr = malloc(sizeof(ast_expr_unary_t));
    unary_expr->id = expr_id;
    unary_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_primary_expr(ctx, &unary_expr->value)){
        free(unary_expr);
        return FAILURE;
    }

    *out_expr = (ast_expr_t*) unary_expr;
    return SUCCESS;
}

errorcode_t parse_expr_new(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes current token is 'new' keyword

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    if(tokens[*i + 1].id == TOKEN_CSTRING){
        // This is actually an 'ast_expr_new_cstring_t' expression,
        // instead of a 'ast_expr_new_t' expression

        ast_expr_new_cstring_t *new_cstring_expr = malloc(sizeof(ast_expr_new_cstring_t));
        new_cstring_expr->id = EXPR_NEW_CSTRING;
        new_cstring_expr->source = sources[(*i)++];
        new_cstring_expr->value = (char*) tokens[(*i)++].data;
        *out_expr = (ast_expr_t*) new_cstring_expr;
        return SUCCESS;
    }

    ast_expr_new_t *new_expr = malloc(sizeof(ast_expr_new_t));
    new_expr->id = EXPR_NEW;
    new_expr->source = sources[(*i)++];
    new_expr->amount = NULL;

    if(parse_type(ctx, &new_expr->type)){
        free(new_expr);
        return FAILURE;
    }

    if(tokens[*i].id == TOKEN_MULTIPLY){
        (*i)++;
        
        if(parse_primary_expr(ctx, &new_expr->amount)){
            ast_type_free(&new_expr->type);
            free(new_expr);
            return FAILURE;
        }
    }

    *out_expr = (ast_expr_t*) new_expr;
    return SUCCESS;
}

errorcode_t parse_expr_static(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes current token is 'static' keyword

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    ast_expr_static_data_t *static_array = malloc(sizeof(ast_expr_static_data_t));
    static_array->source = sources[(*i)++];

    if(parse_type(ctx, &static_array->type)){
        free(static_array);
        return FAILURE;
    }

    ast_expr_t *member;
    ast_expr_t **members = NULL;
    length_t members_length = 0;
    length_t members_capacity = 0;

    tokenid_t finish_token;
    tokenid_t static_kind = tokens[*i].id;

    if(static_kind != TOKEN_BEGIN && static_kind != TOKEN_OPEN){
        compiler_panic(ctx->compiler, sources[*i], "Expected '(' or '{' after given type");
        return FAILURE;
    }

    if(static_kind == TOKEN_BEGIN){
        static_array->id = EXPR_STATIC_ARRAY;
        finish_token = TOKEN_END;
    } else {
        static_array->id = EXPR_STATIC_STRUCT;
        finish_token = TOKEN_CLOSE;
    }

    (*i)++;

    while(tokens[*i].id != finish_token){
        if(parse_ignore_newlines(ctx, "Expected expression")){
            ast_exprs_free_fully(members, members_length);
            return FAILURE;
        }

        if(parse_expr(ctx, &member)){
            ast_exprs_free_fully(members, members_length);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &members, sizeof(ast_expr_t*), members_length, &members_capacity, 1, 4);
        members[members_length++] = member;
        
        if(parse_ignore_newlines(ctx, finish_token == TOKEN_END ? "Expected ',' or '}' after expression" : "Expected ',' or ')' after expression")){
            ast_exprs_free_fully(members, members_length);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != finish_token){
            compiler_panic(ctx->compiler, sources[*i], finish_token == TOKEN_END ? "Expected ',' or '}' after expression" : "Expected ',' or ')' after expression");
            ast_exprs_free_fully(members, members_length);
            return FAILURE;
        }
    }

    static_array->values = members;
    static_array->length = members_length;

    *i += 1;
    *out_expr = (ast_expr_t*) static_array;
    return SUCCESS;
}

errorcode_t parse_expr_def(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assume either 'def' or 'undef' keyword in expression

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    ast_expr_inline_declare_t *def = malloc(sizeof(ast_expr_inline_declare_t));
    def->id = (tokens[*i].id == TOKEN_UNDEF ? EXPR_ILDECLAREUNDEF : EXPR_ILDECLARE);
    def->source = sources[(*i)++];
    def->is_pod = false;
    def->is_assign_pod = false;

    def->name = parse_eat_word(ctx, "Expected variable name for inline declaration");
    if(def->name == NULL){
        free(def);
        return FAILURE;
    }

    if(tokens[*i].id == TOKEN_POD){
        def->is_pod = true;
        (*i)++;
    }

    if(parse_type(ctx, &def->type)){
        free(def);
        return FAILURE;
    }

    if(tokens[*i].id == TOKEN_ASSIGN){
        if(def->id == EXPR_ILDECLAREUNDEF){
            compiler_panic(ctx->compiler, sources[*i], "Can't initialize undefined inline variable");
            printf("\nDid you mean to use 'def' instead of 'undef'?\n");
            ast_type_free(&def->type);
            free(def);
            return FAILURE;
        }

        if(tokens[++(*i)].id == TOKEN_POD){
            def->is_assign_pod = true;
            (*i)++;
        }

        if(parse_expr(ctx, &def->value)){
            ast_type_free(&def->type);
            free(def);
            return FAILURE;
        }
    } else {
        def->value = NULL;
    }

    *out_expr = (ast_expr_t*) def;
    return SUCCESS;
}

errorcode_t parse_expr_typeinfo(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_typeinfo_t *typeinfo = malloc(sizeof(ast_expr_typeinfo_t));
    typeinfo->id = EXPR_TYPEINFO;
    typeinfo->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_type(ctx, &typeinfo->target)){
        free(typeinfo);
        return FAILURE;
    }

    *out_expr = (ast_expr_t*) typeinfo;
    return SUCCESS;
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
    case TOKEN_AS:
        return 6;
    default:
        return 0;
    }
}
