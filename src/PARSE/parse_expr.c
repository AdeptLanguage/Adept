
#include "UTIL/util.h"
#include "UTIL/search.h"
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
        LITERAL_TO_EXPR(ast_expr_byte_t, EXPR_BYTE, adept_byte);
        break;
    case TOKEN_UBYTE:
        LITERAL_TO_EXPR(ast_expr_ubyte_t, EXPR_UBYTE, adept_ubyte);
        break;
    case TOKEN_SHORT:
        LITERAL_TO_EXPR(ast_expr_short_t, EXPR_SHORT, adept_short);
        break;
    case TOKEN_USHORT:
        LITERAL_TO_EXPR(ast_expr_ushort_t, EXPR_USHORT, adept_ushort);
        break;
    case TOKEN_INT:
        LITERAL_TO_EXPR(ast_expr_int_t, EXPR_INT, adept_int);
        break;
    case TOKEN_UINT:
        LITERAL_TO_EXPR(ast_expr_uint_t, EXPR_UINT, adept_uint);
        break;
    case TOKEN_GENERIC_INT:
        LITERAL_TO_EXPR(ast_expr_generic_int_t, EXPR_GENERIC_INT, adept_generic_int);
        break;
    case TOKEN_LONG:
        LITERAL_TO_EXPR(ast_expr_long_t, EXPR_LONG, adept_long);
        break;
    case TOKEN_ULONG:
        LITERAL_TO_EXPR(ast_expr_ulong_t, EXPR_ULONG, adept_ulong);
        break;
    case TOKEN_USIZE:
        LITERAL_TO_EXPR(ast_expr_usize_t, EXPR_USIZE, adept_usize);
        break;
    case TOKEN_FLOAT:
        LITERAL_TO_EXPR(ast_expr_float_t, EXPR_FLOAT, adept_float);
        break;
    case TOKEN_DOUBLE:
        LITERAL_TO_EXPR(ast_expr_double_t, EXPR_DOUBLE, adept_double);
        break;
    case TOKEN_GENERIC_FLOAT:
        LITERAL_TO_EXPR(ast_expr_generic_float_t, EXPR_GENERIC_FLOAT, adept_generic_float);
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
        if(parse_ignore_newlines(ctx, "Expected ')' after expression")) return FAILURE;

        // Ignore newline termination within group
        ctx->ignore_newlines_in_expr_depth++;

        if(parse_expr(ctx, out_expr) != 0){
            ctx->ignore_newlines_in_expr_depth--;
            return FAILURE;
        }

        ctx->ignore_newlines_in_expr_depth--;
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
    case TOKEN_INCREMENT:
        if(parse_expr_preincrement(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_DECREMENT:
        if(parse_expr_predecrement(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_META: {
            weak_cstr_t directive = tokens[(*i)++].data;

            if(strcmp(directive, "get") != 0){
                compiler_panicf(ctx->compiler, sources[*i - 1], "Unexpected meta directive '%s' in expression", directive);
                return FAILURE;
            }

            // Parse name of transcendant variable to get
            weak_cstr_t transcendant_name = parse_eat_word(ctx, "Expected transcendant variable name after '#get'");
            if(transcendant_name == NULL) return FAILURE;

            meta_expr_t *value;
            meta_expr_t *special_result = meta_get_special_variable(ctx->compiler, transcendant_name, sources[*i - 1]);

            if(special_result){
                value = special_result;
            } else {
                meta_definition_t *definition = meta_definition_find(ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, transcendant_name);

                if(definition == NULL){
                    compiler_panicf(ctx->compiler, sources[*i - 1], "Transcendant variable '%s' does not exist", transcendant_name);
                    return FAILURE;
                }

                value = definition->value;
            }

            if(!IS_META_EXPR_ID_COLLAPSED(value->id)){
                compiler_panicf(ctx->compiler, sources[*i - 1], "INTERNAL ERROR: Meta expression expected to be collapsed");
                return FAILURE;
            }

            switch(value->id){
            case META_EXPR_TRUE:
                ast_expr_create_bool(out_expr, true, sources[*i - 1]);
                break;
            case META_EXPR_FALSE:
                ast_expr_create_bool(out_expr, false, sources[*i - 1]);
                break;
            case META_EXPR_STR: {
                    meta_expr_str_t *str = (meta_expr_str_t*) value;
                    ast_expr_create_string(out_expr, str->value, strlen(str->value), sources[*i - 1]);
                }
                break;
            case META_EXPR_INT: {
                    meta_expr_int_t *integer = (meta_expr_int_t*) value;
                    ast_expr_create_long(out_expr, integer->value, sources[*i - 1]);
                }
                break;
            case META_EXPR_FLOAT: {
                    meta_expr_float_t *floating_point = (meta_expr_float_t*) value;
                    ast_expr_create_double(out_expr, floating_point->value, sources[*i - 1]);
                }
                break;
            default:
                compiler_panicf(ctx->compiler, sources[*i - 1], "INTERNAL ERROR: '#get %s' failed to morph transcendant value into literal\n", transcendant_name);
                return FAILURE;
            }
            break;
        }
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
                bool is_tentative = tokens[*i + 1].id == TOKEN_MAYBE;
                if(is_tentative) (*i)++;

                if(parse_ignore_newlines(ctx, "Unexpected statement termination")){
                    return FAILURE;
                }

                if(tokens[++(*i)].id != TOKEN_WORD){
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected identifier after '.' operator");
                    return FAILURE;
                }

                if(parse_ignore_newlines(ctx, "Unexpected statement termination")){
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
                    call_expr->is_tentative = is_tentative;
                    *i += 2;

                    // value.method(arg1, arg2, ...)
                    //              ^

                    ast_expr_t *arg_expr;
                    length_t args_capacity = 0;

                    // Ignore newline termination within children expressions
                    ctx->ignore_newlines_in_expr_depth++;

                    while(tokens[*i].id != TOKEN_CLOSE){
                        if(parse_ignore_newlines(ctx, "Expected method argument")){
                            ctx->ignore_newlines_in_expr_depth--;
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }

                        if(parse_expr(ctx, &arg_expr)){
                            ctx->ignore_newlines_in_expr_depth--;
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
                            ctx->ignore_newlines_in_expr_depth--;
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }

                        if(tokens[*i].id == TOKEN_NEXT) (*i)++;
                        else if(tokens[*i].id != TOKEN_CLOSE){
                            ctx->ignore_newlines_in_expr_depth--;
                            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr);
                            return FAILURE;
                        }
                    }

                    ctx->ignore_newlines_in_expr_depth--;
                    *inout_expr = (ast_expr_t*) call_expr;
                    (*i)++;
                } else {
                    if(is_tentative){
                        compiler_panic(ctx->compiler, sources[*i - 2], "Cannot have tentative field access");
                        return FAILURE;
                    }

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
        case TOKEN_AT:
            if(parse_expr_at(ctx, inout_expr)) return FAILURE;
            break;
        case TOKEN_INCREMENT: case TOKEN_DECREMENT: {
                if(!expr_is_mutable(*inout_expr)){
                    compiler_panicf(ctx->compiler, sources[*i], "Can only %s mutable values", tokens[*i].id == TOKEN_INCREMENT ? "increment" : "decrement");
                    return FAILURE;
                }

                ast_expr_unary_t *increment = (ast_expr_unary_t*) malloc(sizeof(ast_expr_unary_t));
                increment->id = tokens[*i].id == TOKEN_INCREMENT ? EXPR_POSTINCREMENT : EXPR_POSTDECREMENT;
                increment->source = sources[(*i)++];
                increment->value = *inout_expr;
                *inout_expr = (ast_expr_t*) increment;
            }
            break;
        case TOKEN_TOGGLE: {
                if(!expr_is_mutable(*inout_expr)){
                    compiler_panic(ctx->compiler, sources[*i], "Cannot perform '!!' operator on immutable values");
                    return FAILURE;
                }

                ast_expr_unary_t *toggle = (ast_expr_unary_t*) malloc(sizeof(ast_expr_unary_t));
                toggle->id = EXPR_TOGGLE;
                toggle->source = sources[(*i)++];
                toggle->value = *inout_expr;
                *inout_expr = (ast_expr_t*) toggle;
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
        int operator;
        source_t source;

        // Await possible termination operators
        while(true){
            operator = tokens[*i].id;
            source = sources[*i];
            
            // NOTE: Must be sorted
            const static int op_termination_tokens[] = {
                TOKEN_ASSIGN,             // 0x00000008
                TOKEN_CLOSE,              // 0x00000011
                TOKEN_BEGIN,              // 0x00000012
                TOKEN_END,                // 0x00000013
                TOKEN_NEWLINE,            // 0x00000014
                TOKEN_NEXT,               // 0x00000022
                TOKEN_BRACKET_CLOSE,      // 0x00000024
                TOKEN_ADD_ASSIGN,         // 0x00000028
                TOKEN_SUBTRACT_ASSIGN,    // 0x00000029
                TOKEN_MULTIPLY_ASSIGN,    // 0x0000002A
                TOKEN_DIVIDE_ASSIGN,      // 0x0000002B
                TOKEN_MODULUS_ASSIGN,     // 0x0000002C
                TOKEN_BIT_AND_ASSIGN,     // 0x0000002D
                TOKEN_BIT_OR_ASSIGN,      // 0x0000002E
                TOKEN_BIT_XOR_ASSIGN,     // 0x0000002F
                TOKEN_BIT_LS_ASSIGN,      // 0x00000030
                TOKEN_BIT_RS_ASSIGN,      // 0x00000031
                TOKEN_BIT_LGC_LS_ASSIGN,  // 0x00000032 
                TOKEN_BIT_LGC_RS_ASSIGN,  // 0x00000033
                TOKEN_TERMINATE_JOIN,     // 0x00000037
                TOKEN_COLON,              // 0x00000038
                TOKEN_ELSE,               // 0x0000005E
            };

            // Terminate operator expression portion if termination operator encountered
            maybe_index_t op_termination = binary_int_search(op_termination_tokens, sizeof(op_termination_tokens) / sizeof(int), operator);
            if(op_termination != -1){
                // Always terminate if not newline
                if(op_termination_tokens[op_termination] != TOKEN_NEWLINE) return SUCCESS;
                
                // Terminate for newlines if not ignoring them
                if(ctx->ignore_newlines_in_expr_depth == 0) return SUCCESS;

                // Otherwise skip over newlines
                if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;
                continue;
            }

            break;
        }

        int operator_precedence =  parse_get_precedence(operator);
        if(operator_precedence < precedence || keep_mutable) return SUCCESS;

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
        case TOKEN_ADD:            BUILD_MATH_EXPR_MACRO(EXPR_ADD);            break;
        case TOKEN_SUBTRACT:       BUILD_MATH_EXPR_MACRO(EXPR_SUBTRACT);       break;
        case TOKEN_MULTIPLY:       BUILD_MATH_EXPR_MACRO(EXPR_MULTIPLY);       break;
        case TOKEN_DIVIDE:         BUILD_MATH_EXPR_MACRO(EXPR_DIVIDE);         break;
        case TOKEN_MODULUS:        BUILD_MATH_EXPR_MACRO(EXPR_MODULUS);        break;
        case TOKEN_EQUALS:         BUILD_MATH_EXPR_MACRO(EXPR_EQUALS);         break;
        case TOKEN_NOTEQUALS:      BUILD_MATH_EXPR_MACRO(EXPR_NOTEQUALS);      break;
        case TOKEN_GREATERTHAN:    BUILD_MATH_EXPR_MACRO(EXPR_GREATER);        break;
        case TOKEN_LESSTHAN:       BUILD_MATH_EXPR_MACRO(EXPR_LESSER);         break;
        case TOKEN_GREATERTHANEQ:  BUILD_MATH_EXPR_MACRO(EXPR_GREATEREQ);      break;
        case TOKEN_LESSTHANEQ:     BUILD_MATH_EXPR_MACRO(EXPR_LESSEREQ);       break;
        case TOKEN_BIT_AND:        BUILD_MATH_EXPR_MACRO(EXPR_BIT_AND);        break;
        case TOKEN_BIT_OR:         BUILD_MATH_EXPR_MACRO(EXPR_BIT_OR);         break;
        case TOKEN_BIT_XOR:        BUILD_MATH_EXPR_MACRO(EXPR_BIT_XOR);        break;
        case TOKEN_BIT_LSHIFT:     BUILD_MATH_EXPR_MACRO(EXPR_BIT_LSHIFT);     break;
        case TOKEN_BIT_RSHIFT:     BUILD_MATH_EXPR_MACRO(EXPR_BIT_RSHIFT);     break;
        case TOKEN_BIT_LGC_LSHIFT: BUILD_MATH_EXPR_MACRO(EXPR_BIT_LGC_LSHIFT); break;
        case TOKEN_BIT_LGC_RSHIFT: BUILD_MATH_EXPR_MACRO(EXPR_BIT_LGC_RSHIFT); break;
        case TOKEN_AND:
        case TOKEN_UBERAND:        BUILD_MATH_EXPR_MACRO(EXPR_AND);            break;
        case TOKEN_OR:
        case TOKEN_UBEROR:         BUILD_MATH_EXPR_MACRO(EXPR_OR);             break;
        case TOKEN_AS:
            if(parse_expr_as(ctx, inout_left) || parse_expr_post(ctx, inout_left)) return FAILURE;
            break;
        case TOKEN_AT:
            if(parse_expr_at(ctx, inout_left) || parse_expr_post(ctx, inout_left)) return FAILURE;
            break;
        case TOKEN_MAYBE: {
                (*i)++;
                if(parse_ignore_newlines(ctx, "Unexpected end of expression")) return FAILURE;
                
                ast_expr_t *expr_a, *expr_b;
                if(parse_expr(ctx, &expr_a)) return FAILURE;

                if(parse_ignore_newlines(ctx, "Unexpected end of expression")) return FAILURE;

                if(tokens[(*i)++].id != TOKEN_COLON){
                    compiler_panic(ctx->compiler, sources[*i - 1], "Ternary operator expected ':' after expression");
                    ast_expr_free_fully(*inout_left);
                    ast_expr_free_fully(expr_a);
                    return FAILURE;
                }

                if(parse_ignore_newlines(ctx, "Unexpected end of expression")) return FAILURE;

                if(parse_expr(ctx, &expr_b)){
                    ast_expr_free_fully(*inout_left);
                    ast_expr_free_fully(expr_a);
                    return FAILURE;
                }

                ast_expr_create_ternary(inout_left, *inout_left, expr_a, expr_b, source);
            }
            break;
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

    switch(tokens[*i + 1].id){
    case TOKEN_OPEN:      return parse_expr_call(ctx, out_expr);
    case TOKEN_NAMESPACE: return parse_expr_enum_value(ctx, out_expr);
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

    // Determine whether call is tentative, assume that '(' token follows
    bool is_tentative;
    (*i)++;

    if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;

    if(tokens[(*i)++].id == TOKEN_MAYBE){
        is_tentative = true;
        (*i)++;
    } else {
        is_tentative = false;
    }

    // Ignore newline termination within children expressions
    ctx->ignore_newlines_in_expr_depth++;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected function argument") || parse_expr(ctx, &arg)){
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(args, arity);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &args, sizeof(ast_expr_t*), arity, &max_arity, 1, 4);
        args[arity++] = arg;
        
        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")){
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(args, arity);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(args, arity);
            return FAILURE;
        }
    }

    if(is_tentative){
        compiler_panic(ctx->compiler, source, "Tentative calls cannot be used in expressions");
        ctx->ignore_newlines_in_expr_depth--;
        ast_exprs_free_fully(args, arity);
        return FAILURE;
    }

    ctx->ignore_newlines_in_expr_depth--;
    ast_expr_create_call(out_expr, name, arity, args, is_tentative, source);
    (*i)++;
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

    if(!expr_is_mutable(addr_expr->value)){
        compiler_panic(ctx->compiler, addr_expr->value->source, "The '&' operator requires the operand to be mutable");
        ast_expr_free_fully((ast_expr_t*) addr_expr);
        return FAILURE;
    }
    
    *out_expr = (ast_expr_t*) addr_expr;
    return SUCCESS;
}

int parse_expr_func_address(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_func_addr_t *func_addr_expr = malloc(sizeof(ast_expr_func_addr_t));

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    func_addr_expr->id = EXPR_FUNC_ADDR;
    func_addr_expr->source = ctx->tokenlist->sources[(*i)++];
    func_addr_expr->traits = TRAIT_NONE;
    func_addr_expr->match_args = NULL;
    func_addr_expr->match_args_length = 0;
    func_addr_expr->tentative = false;
    func_addr_expr->has_match_args = false;

    // Optionally enable tentative function lookup: 'func null &functionName'
    if(tokens[*i].id == TOKEN_NULL){
        func_addr_expr->tentative = true;
        (*i)++;
    }

    if(parse_eat(ctx, TOKEN_ADDRESS, "Expected '&' after 'func' keyword in expression")){
        free(func_addr_expr);
        return FAILURE;
    }

    func_addr_expr->name = parse_eat_word(ctx, "Expected function name after 'func &' operator");

    if(func_addr_expr->name == NULL){
        free(func_addr_expr);
        return FAILURE;
    }


    if(tokens[*i].id == TOKEN_OPEN){
        ast_type_t arg_type;

        ast_type_t *args = NULL;
        length_t arity = 0;
        
        func_addr_expr->has_match_args = true;
        (*i)++;

        while(*i != ctx->tokenlist->length && tokens[*i].id != TOKEN_CLOSE){
            grow((void*) &args, sizeof(ast_type_t), arity, arity + 1);

            if(parse_ignore_newlines(ctx, "Expected function argument") || parse_type(ctx, &arg_type)){
                ast_types_free_fully(args, arity);
                free(func_addr_expr);
                return FAILURE;
            }

            args[arity++] = arg_type;

            if(tokens[*i].id == TOKEN_NEXT){
                if(tokens[++(*i)].id == TOKEN_CLOSE){
                    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected type after ',' in argument list");
                    ast_types_free_fully(args, arity);
                    free(func_addr_expr);
                    return FAILURE;
                }
            } else if(tokens[*i].id != TOKEN_CLOSE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' after argument type");
                ast_types_free_fully(args, arity);
                free(func_addr_expr);
                return FAILURE;
            }
        }

        (*i)++;
        func_addr_expr->match_args = args;
        func_addr_expr->match_args_length = arity;
    }

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
    if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;

    if(ctx->tokenlist->tokens[*i].id == TOKEN_OPEN){
        // 'cast' will only apply to expression in parentheses if present.
        // If this behavior is undesired, use the newer 'as' operator instead
        (*i)++;

        // Ignore newlines before actual expression
        if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;
        
        if(parse_expr(ctx, &from)){
            ast_type_free(&to);
            return FAILURE;
        }

        if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after expression given to 'cast'")){
            ast_expr_free(from);
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

errorcode_t parse_expr_at(parse_ctx_t *ctx, ast_expr_t **inout_expr){
    // (value) at <index>
    //         ^

    ast_expr_t *index_expr;
    ast_expr_array_access_t *at_expr = malloc(sizeof(ast_expr_array_access_t));
    at_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_primary_expr(ctx, &index_expr)){
        free(at_expr);
        return FAILURE;
    }

    at_expr->id = EXPR_AT;
    at_expr->value = *inout_expr;
    at_expr->index = index_expr;
    *inout_expr = (ast_expr_t*) at_expr;
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
    new_expr->amount = NULL;
    new_expr->is_undef = false;
    new_expr->source = sources[*i];
    if(tokens[++*i].id == TOKEN_UNDEF){
        new_expr->is_undef = true;
        (*i)++;
    }

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

errorcode_t parse_expr_preincrement(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_unary_t *preincrement_expr = malloc(sizeof(ast_expr_unary_t));
    preincrement_expr->id = EXPR_PREINCREMENT;
    preincrement_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_primary_expr(ctx, &preincrement_expr->value) || parse_op_expr(ctx, 0, &preincrement_expr->value, true)){
        free(preincrement_expr);
        return FAILURE;
    }

    if(!expr_is_mutable(preincrement_expr->value)){
        compiler_panic(ctx->compiler, preincrement_expr->value->source, "The '++' operator requires the operand to be mutable");
        ast_expr_free_fully((ast_expr_t*) preincrement_expr);
        return FAILURE;
    }
    
    *out_expr = (ast_expr_t*) preincrement_expr;
    return SUCCESS;
}

errorcode_t parse_expr_predecrement(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_unary_t *predecrement_expr = malloc(sizeof(ast_expr_unary_t));
    predecrement_expr->id = EXPR_PREDECREMENT;
    predecrement_expr->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_primary_expr(ctx, &predecrement_expr->value) || parse_op_expr(ctx, 0, &predecrement_expr->value, true)){
        free(predecrement_expr);
        return FAILURE;
    }

    if(!expr_is_mutable(predecrement_expr->value)){
        compiler_panic(ctx->compiler, predecrement_expr->value->source, "The '--' operator requires the operand to be mutable");
        ast_expr_free_fully((ast_expr_t*) predecrement_expr);
        return FAILURE;
    }
    
    *out_expr = (ast_expr_t*) predecrement_expr;
    return SUCCESS;
}

int parse_get_precedence(unsigned int id){
    switch(id){
    case TOKEN_MAYBE:
        return 1;
    case TOKEN_UBERAND: case TOKEN_UBEROR:
        return 2;
    case TOKEN_AND: case TOKEN_OR:
        return 3;
    case TOKEN_EQUALS: case TOKEN_NOTEQUALS:
    case TOKEN_LESSTHAN: case TOKEN_GREATERTHAN:
    case TOKEN_LESSTHANEQ: case TOKEN_GREATERTHANEQ:
        return 4;
    case TOKEN_ADD: case TOKEN_SUBTRACT: case TOKEN_WORD:
        return 5;
    case TOKEN_MULTIPLY: case TOKEN_DIVIDE: case TOKEN_MODULUS:
        return 6;
    case TOKEN_AS:
        return 7;
    default:
        return 0;
    }
}
