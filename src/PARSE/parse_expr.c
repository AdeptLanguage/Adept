
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "AST/meta_directives.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/datatypes.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

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
    
    // TODO: CLEANUP: This code should be cleaned up
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
        *out_expr = ast_expr_create_bool(true, sources[(*i)++]);
        break;
    case TOKEN_FALSE:
        *out_expr = ast_expr_create_bool(false, sources[(*i)++]);
        break;
    case TOKEN_CSTRING:
        *out_expr = ast_expr_create_cstring((char*) tokens[*i].data, sources[*i]);
        *i += 1;
        break;
    case TOKEN_STRING: {
            token_string_data_t *string_data = (token_string_data_t*) tokens[*i].data;
            *out_expr = ast_expr_create_string(string_data->array, string_data->length, sources[*i]);
            *i += 1;
        }
        break;
    case TOKEN_NULL:
        *out_expr = ast_expr_create_null(sources[(*i)++]);
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
    case TOKEN_ALIGNOF:
        if(parse_expr_alignof(ctx, out_expr)) return FAILURE;
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
        if(parse_expr_mutable_unary_prefix(ctx, EXPR_PREINCREMENT, "++", out_expr)) return FAILURE;
        break;
    case TOKEN_DECREMENT:
        if(parse_expr_mutable_unary_prefix(ctx, EXPR_PREDECREMENT, "--", out_expr)) return FAILURE;
        break;
    case TOKEN_META: {
            weak_cstr_t directive = tokens[(*i)++].data;

            if(!streq(directive, "get")){
                compiler_panicf(ctx->compiler, sources[*i - 1], "Unexpected meta directive '%s' in expression", directive);
                return FAILURE;
            }

            // Parse name of transcendant variable to get
            weak_cstr_t transcendant_name = parse_eat_word(ctx, "Expected transcendant variable name after '#get'");
            if(transcendant_name == NULL) return FAILURE;

            meta_expr_t *value;
            meta_expr_t *special_result = meta_get_special_variable(ctx->compiler, ctx->object, transcendant_name, sources[*i - 1]);

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
             case META_EXPR_UNDEF: case META_EXPR_NULL:
                *out_expr = ast_expr_create_null(sources[*i - 1]);
                break;
            case META_EXPR_TRUE:
                *out_expr = ast_expr_create_bool(true, sources[*i - 1]);
                break;
            case META_EXPR_FALSE:
                *out_expr = ast_expr_create_bool(false, sources[*i - 1]);
                break;
            case META_EXPR_STR: {
                    meta_expr_str_t *str = (meta_expr_str_t*) value;
                    *out_expr = ast_expr_create_string(str->value, strlen(str->value), sources[*i - 1]);
                }
                break;
            case META_EXPR_INT: {
                    meta_expr_int_t *integer = (meta_expr_int_t*) value;
                    *out_expr = ast_expr_create_long(integer->value, sources[*i - 1]);
                }
                break;
            case META_EXPR_FLOAT: {
                    meta_expr_float_t *floating_point = (meta_expr_float_t*) value;
                    *out_expr = ast_expr_create_double(floating_point->value, sources[*i - 1]);
                }
                break;
            default:
                compiler_panicf(ctx->compiler, sources[*i - 1], "INTERNAL ERROR: '#get %s' failed to morph transcendant value into literal\n", transcendant_name);
                return FAILURE;
            }
            break;
        }
    case TOKEN_VA_ARG: {
            // va_arg(list, Type)
            //   ^
            
            source_t source = sources[(*i)++];

            // Eat '('
            if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after va_arg keyword")) return FAILURE;

            ast_expr_t *va_list_value;
            if(parse_expr(ctx, &va_list_value)) return FAILURE;

            // Eat ','
            if(parse_eat(ctx, TOKEN_NEXT, "Expected ',' after first parameter to va_arg")){
                ast_expr_free_fully(va_list_value);
                return FAILURE;
            }

            ast_type_t arg_type;
            if(parse_type(ctx, &arg_type)){
                ast_expr_free_fully(va_list_value);
                return FAILURE;
            }

            // Eat ')'
            if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after va_arg parameters")){
                ast_expr_free_fully(va_list_value);
                ast_type_free(&arg_type);
                return FAILURE;
            }

            *out_expr = ast_expr_create_va_arg(source, va_list_value, arg_type);
        }
        break;
    case TOKEN_BEGIN:
        if(parse_expr_initlist(ctx, out_expr)) return FAILURE;
        break;
    case TOKEN_POLYCOUNT:
        *out_expr = ast_expr_create_polycount(sources[*i], (strong_cstr_t) parse_ctx_peek_data_take(ctx));
        *i += 1;
        break;
    case TOKEN_TYPENAMEOF: {
            source_t source = sources[(*i)++];
            
            ast_type_t type;
            if(parse_type(ctx, &type)) return FAILURE;

            *out_expr = ast_expr_create_typenameof(type, source);
        }
        break;
    case TOKEN_EMBED: {
            source_t source = sources[(*i)++];

            maybe_null_weak_cstr_t filename = parse_eat_string(ctx, "Expected filename after 'embed' keyword");
            if(filename == NULL) return FAILURE;

            *out_expr = ast_expr_create_embed(filename_local(ctx->object->filename, filename), source);
        }
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

    // TODO: CLEANUP: Clean up this messy code
    while(true){
        switch(tokens[*i].id){
        case TOKEN_BRACKET_OPEN: {
                ast_expr_t *index_expr;
                source_t source = sources[(*i)++];
                
                if(parse_expr(ctx, &index_expr)) return FAILURE;

                if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']' after array index expression")){
                    ast_expr_free_fully(index_expr);
                    return FAILURE;
                }

                *inout_expr = ast_expr_create_access(*inout_expr, index_expr, source);
            }
            break;
        case TOKEN_MEMBER: {
                *i += 1;

                bool is_tentative = (parse_eat(ctx, TOKEN_MAYBE, NULL) == SUCCESS);

                if(parse_ignore_newlines(ctx, "Unexpected statement termination")){
                    return FAILURE;
                }

                source_t source = parse_ctx_peek_source(ctx);

                strong_cstr_t name = parse_take_word(ctx, "Expected identifier after '.' operator");
                if(name == NULL) return FAILURE;

                if(parse_eat(ctx, TOKEN_OPEN, NULL) == SUCCESS){
                    ast_expr_call_method_t *call_expr = (ast_expr_call_method_t*) ast_expr_create_call_method(name, *inout_expr, 0, NULL, is_tentative, false, NULL, source);

                    // value.method(arg1, arg2, ...)
                    //              ^

                    // Ignore newline termination in expression parsing
                    ctx->ignore_newlines_in_expr_depth++;

                    if(parse_expr_arguments(ctx, &call_expr->args, &call_expr->arity, NULL)){
                        ctx->ignore_newlines_in_expr_depth--;
                        free(call_expr->name);
                        free(call_expr);
                        return FAILURE;
                    }

                    // Un-Ignore newline termination in expression parsing
                    ctx->ignore_newlines_in_expr_depth--;

                    if(parse_eat(ctx, TOKEN_GIVES, NULL) == SUCCESS){
                        if(parse_type(ctx, &call_expr->gives)){
                            ast_exprs_free_fully(call_expr->args, call_expr->arity);
                            free(call_expr->name);
                            free(call_expr);
                            return FAILURE;
                        }
                    } else {
                        memset(&call_expr->gives, 0, sizeof(ast_type_t));
                    }

                    *inout_expr = (ast_expr_t*) call_expr;
                } else {
                    if(is_tentative){
                        compiler_panic(ctx->compiler, source, "Cannot have tentative field access");
                        return FAILURE;
                    }

                    // Member access expression
                    *inout_expr = ast_expr_create_member(*inout_expr, name, source);
                }
            }
            break;
        case TOKEN_AT:
            if(parse_expr_at(ctx, inout_expr)) return FAILURE;
            break;
        case TOKEN_INCREMENT: case TOKEN_DECREMENT: {
                source_t source = sources[*i];
                bool is_increment = parse_ctx_peek(ctx) == TOKEN_INCREMENT;

                *i += 1;

                if(!expr_is_mutable(*inout_expr)){
                    compiler_panicf(ctx->compiler, source, "Can only %s mutable values", is_increment ? "increment" : "decrement");
                    return FAILURE;
                }

                if(is_increment){
                    *inout_expr = ast_expr_create_post_increment(source, *inout_expr);
                } else {
                    *inout_expr = ast_expr_create_post_decrement(source, *inout_expr);
                }
            }
            break;
        case TOKEN_TOGGLE: {
                if(!expr_is_mutable(*inout_expr)){
                    compiler_panic(ctx->compiler, sources[*i], "Cannot perform '!!' operator on immutable values");
                    return FAILURE;
                }

                *inout_expr = ast_expr_create_toggle(sources[(*i)++], *inout_expr);
            }
            break;
        default:
            return SUCCESS;
        }
    }

    // Should never be reached
    return SUCCESS;
}

static errorcode_t parse_rhs_expr_into_math(parse_ctx_t *ctx, ast_expr_t **inout_left,
        unsigned int expr_kind, int op_prec, source_t source){
    
    // (lhs expression) + (rhs expression)
    //                  ^
    
    // Parse right hand side of math operation (operator will be skipped over in procedure)
    ast_expr_t *right;
    if(parse_rhs_expr(ctx, inout_left, &right, op_prec)) return FAILURE;

    *inout_left = ast_expr_create_math(source, expr_kind, *inout_left, right);
    return SUCCESS;
}

static tokenid_t parse_expr_has_terminating_token(tokenid_t id){
    // If the given token is an expression terminating token, it will be returned,
    // Otherwise TOKEN_NONE (value of 0) will be returned

    switch(id){
    case TOKEN_WORD:
    case TOKEN_ASSIGN:
    case TOKEN_CLOSE:
    case TOKEN_BEGIN:
    case TOKEN_END:
    case TOKEN_NEWLINE:
    case TOKEN_NEXT:
    case TOKEN_BRACKET_CLOSE:
    case TOKEN_ADD_ASSIGN:
    case TOKEN_SUBTRACT_ASSIGN:
    case TOKEN_MULTIPLY_ASSIGN:
    case TOKEN_DIVIDE_ASSIGN:
    case TOKEN_MODULUS_ASSIGN:
    case TOKEN_BIT_AND_ASSIGN:
    case TOKEN_BIT_OR_ASSIGN:
    case TOKEN_BIT_XOR_ASSIGN:
    case TOKEN_BIT_LSHIFT_ASSIGN:
    case TOKEN_BIT_RSHIFT_ASSIGN:
    case TOKEN_BIT_LGC_LSHIFT_ASSIGN:
    case TOKEN_BIT_LGC_RSHIFT_ASSIGN:
    case TOKEN_TERMINATE_JOIN:
    case TOKEN_COLON:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
    case TOKEN_DEFER:
    case TOKEN_DELETE:
    case TOKEN_EACH:
    case TOKEN_ELSE:
    case TOKEN_EXHAUSTIVE:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_REPEAT:
    case TOKEN_RETURN:
    case TOKEN_SWITCH:
    case TOKEN_UNLESS:
    case TOKEN_UNTIL:
    case TOKEN_USING:
    case TOKEN_VA_ARG:
    case TOKEN_VA_END:
    case TOKEN_VA_START:
    case TOKEN_WHILE:
        return id;
    }

    return TOKEN_NONE;
}

static errorcode_t parse_expr_ternary(parse_ctx_t *ctx, ast_expr_t **inout_condition, source_t source){
    // DANGEROUS:
    // NOTE: When first invoked, 'inout_expr' shall be a pointer to the "condition" expression.
    // If this function returns SUCCESS, 'inout_expr' will be overwritten to be a ternary expression
    // that has ownership of the condition.
    // When this function returns FAILURE, the condition, (*inout_condition), will be disposed of before
    // control resumes to the calling function.

    // Skip over '?'
    (*ctx->i)++;

    // "when true" expression
    ast_expr_t *expr_a;

    // Parse the "when true" expression
    // (allowing newlines between '?' and when true expression)
    if(parse_ignore_newlines(ctx, "Unexpected end of expression")
    || parse_expr(ctx, &expr_a)){
        ast_expr_free_fully(*inout_condition);
        return FAILURE;
    }

    // Allow newlines between when true expression and ':'
    if(parse_ignore_newlines(ctx, "Unexpected end of expression")){
        goto failure;
    }

    // Expect ':'
    if(parse_ctx_peek(ctx) != TOKEN_COLON){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Ternary operator expected ':' after expression");
        goto failure;
    }
    *ctx->i += 1;

    // Allow newlines between ':' and when false expression
    if(parse_ignore_newlines(ctx, "Unexpected end of expression")){
        goto failure;
    }

    // Parse the "when false" expression
    ast_expr_t *expr_b;
    if(parse_expr(ctx, &expr_b)){
        goto failure;
    }

    // Construct and yield ternary expression
    *inout_condition = ast_expr_create_ternary(*inout_condition, expr_a, expr_b, source);
    return SUCCESS;

failure:
    ast_expr_free_fully(*inout_condition);
    ast_expr_free_fully(expr_a);
    return FAILURE;
}

errorcode_t parse_op_expr(parse_ctx_t *ctx, int precedence, ast_expr_t **inout_left, bool keep_mutable){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    while(*i != ctx->tokenlist->length) {
        int operator;
        source_t source;

        // Await possible termination operators
        while(true){
            operator = tokens[*i].id;
            source = sources[*i];

            // If there is still more to the expression, keep going
            if(!parse_expr_has_terminating_token(operator)) break;

            // Otherwise...
            // Terminate unless the termination token is a newline and
            // we are allowing newlines within an existing expression
            if(operator != TOKEN_NEWLINE) return SUCCESS;
            if(ctx->ignore_newlines_in_expr_depth == 0) return SUCCESS;

            // We are allowing this newline since it is within an existing
            // expression. Ignore it and and any immediately following newlines
            if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;
            continue;
        }

        int op_prec = parse_get_precedence(operator);
        if(op_prec < precedence || keep_mutable) return SUCCESS;

        switch(operator){
        case TOKEN_ADD:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_ADD, op_prec, source)) return FAILURE;
            break;
        case TOKEN_SUBTRACT:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_SUBTRACT, op_prec, source)) return FAILURE;
            break;
        case TOKEN_MULTIPLY:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_MULTIPLY, op_prec, source)) return FAILURE;
            break;
        case TOKEN_DIVIDE:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_DIVIDE, op_prec, source)) return FAILURE;
            break;
        case TOKEN_MODULUS:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_MODULUS, op_prec, source)) return FAILURE;
            break;
        case TOKEN_EQUALS:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_EQUALS, op_prec, source)) return FAILURE;
            break;
        case TOKEN_NOTEQUALS:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_NOTEQUALS, op_prec, source)) return FAILURE;
            break;
        case TOKEN_GREATERTHAN:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_GREATER, op_prec, source)) return FAILURE;
            break;
        case TOKEN_LESSTHAN:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_LESSER, op_prec, source)) return FAILURE;
            break;
        case TOKEN_GREATERTHANEQ:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_GREATEREQ, op_prec, source)) return FAILURE;
            break;
        case TOKEN_LESSTHANEQ:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_LESSEREQ, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_AND:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_AND, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_OR:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_OR, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_XOR:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_XOR, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_LSHIFT:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_LSHIFT, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_RSHIFT:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_RSHIFT, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_LGC_LSHIFT:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_LGC_LSHIFT, op_prec, source)) return FAILURE;
            break;
        case TOKEN_BIT_LGC_RSHIFT:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_BIT_LGC_RSHIFT, op_prec, source)) return FAILURE;
            break;
        case TOKEN_AND: case TOKEN_UBERAND:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_AND, op_prec, source)) return FAILURE;
            break;
        case TOKEN_OR: case TOKEN_UBEROR:
            if(parse_rhs_expr_into_math(ctx, inout_left, EXPR_OR, op_prec, source)) return FAILURE;
            break;
        case TOKEN_AS:
            if(parse_expr_as(ctx, inout_left) || parse_expr_post(ctx, inout_left)) return FAILURE;
            break;
        case TOKEN_AT:
            if(parse_expr_at(ctx, inout_left) || parse_expr_post(ctx, inout_left)) return FAILURE;
            break;
        case TOKEN_MAYBE:
            if(parse_expr_ternary(ctx, inout_left, source)) return FAILURE;
            break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unrecognized operator '%s' in expression");
            ast_expr_free_fully(*inout_left);
            return FAILURE;
        }
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
    case TOKEN_OPEN:      return parse_expr_call(ctx, out_expr, false);
    case TOKEN_ASSOCIATE: return parse_expr_enum_value(ctx, out_expr);
    }

    weak_cstr_t variable_name = tokens[*i].data;
    *out_expr = ast_expr_create_variable(variable_name, ctx->tokenlist->sources[(*i)++]);
    return SUCCESS;
}

errorcode_t parse_expr_call(parse_ctx_t *ctx, ast_expr_t **out_expr, bool allow_tentative){
    // NOTE: Assumes name and open token

    source_t source = ctx->tokenlist->sources[*ctx->i];

    strong_cstr_t name = parse_take_word(ctx, "Expected function name");
    if(name == NULL) return FAILURE;

    if(parse_ignore_newlines(ctx, "Unexpected statement termination")){
        free(name);
        return FAILURE;
    }

    // Determine whether call is tentative
    bool is_tentative = parse_eat(ctx, TOKEN_MAYBE, NULL) == SUCCESS;

    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after function name for function call")){
        free(name);
        return FAILURE;
    }

    // Ignore newline termination in expression parsing
    ctx->ignore_newlines_in_expr_depth++;

    ast_expr_t **args;
    length_t arity, max_arity;

    if(parse_expr_arguments(ctx, &args, &arity, &max_arity)){
        ctx->ignore_newlines_in_expr_depth--;
        free(name);
        return FAILURE;
    }

    // Un-ignore newline termination in expression parsing
    ctx->ignore_newlines_in_expr_depth--;

    if(is_tentative && !allow_tentative){
        compiler_panic(ctx->compiler, source, "Tentative calls cannot be used in expressions");
        ast_exprs_free_fully(args, arity);
        free(name);
        return FAILURE;
    }

    ast_type_t gives;

    if(parse_eat(ctx, TOKEN_GIVES, NULL) == SUCCESS){
        if(parse_type(ctx, &gives)){
            ast_exprs_free_fully(args, arity);
            free(name);
            return FAILURE;
        }
    } else {
        memset(&gives, 0, sizeof(ast_type_t));
    }

    *out_expr = ast_expr_create_call(name, arity, args, is_tentative, &gives, source);
    return SUCCESS;
}

errorcode_t parse_expr_arguments(parse_ctx_t *ctx, ast_expr_t ***out_args, length_t *out_arity, length_t *out_capacity){
    // (arg1, arg2, arg3)
    //  ^

    token_t *tokens = ctx->tokenlist->tokens;
    length_t *i = ctx->i;

    ast_expr_list_t args = ast_expr_list_create(0);

    while(tokens[*i].id != TOKEN_CLOSE){
        ast_expr_t *arg_expr;

        if(parse_ignore_newlines(ctx, "Expected argument") || parse_expr(ctx, &arg_expr)){
            goto failure;
        }

        ast_expr_list_append(&args, arg_expr);

        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")){
            goto failure;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' or ')' after expression");
            goto failure;
        }
    }

    // Skip over closing ')' token
    (*i)++;

    *out_args = args.statements;
    *out_arity = args.length;
    if(out_capacity != NULL) *out_capacity = args.capacity;
    return SUCCESS;

failure:
    ast_expr_list_free(&args);
    return FAILURE;
}

errorcode_t parse_expr_enum_value(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes name and open token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    weak_cstr_t enum_name = tokens[*i].data;
    source_t source = ctx->tokenlist->sources[(*i)++];

    // (Shouldn't fail unless something weird is going on)
    if(parse_eat(ctx, TOKEN_ASSOCIATE, "Expected '::' operator for enum value")) return FAILURE;

    weak_cstr_t kind_name = parse_eat_word(ctx, "Expected enum value name after '::' operator");
    if(kind_name == NULL) return FAILURE;

    *out_expr = ast_expr_create_enum_value(enum_name, kind_name, source);
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

    ast_type_t to;
    ast_expr_t *from;

    // Assume that expression starts with 'cast' keyword
    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    if(parse_type(ctx, &to)) return FAILURE;
    if(parse_ignore_newlines(ctx, "Unexpected statement termination")) return FAILURE;
    
    if(parse_eat(ctx, TOKEN_OPEN, NULL) == SUCCESS){
        // 'cast' will only apply to expression in parentheses if present.
        // If this behavior is undesired, use the newer 'as' operator instead

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

    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    ast_type_t to;
    if(parse_type(ctx, &to)) return FAILURE;

    *inout_expr = ast_expr_create_cast(to, *inout_expr, source);
    return SUCCESS;
}

errorcode_t parse_expr_at(parse_ctx_t *ctx, ast_expr_t **inout_expr){
    // (value) at <index>
    //         ^

    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    ast_expr_t *index_expr;
    if(parse_primary_expr(ctx, &index_expr)) return FAILURE;

    ast_expr_array_access_t *at_expr = malloc(sizeof(ast_expr_array_access_t));
    at_expr->id = EXPR_AT;
    at_expr->source = source;
    at_expr->value = *inout_expr;
    at_expr->index = index_expr;
    *inout_expr = (ast_expr_t*) at_expr;
    return SUCCESS;
}

errorcode_t parse_expr_sizeof(parse_ctx_t *ctx, ast_expr_t **out_expr){
    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    if(parse_ctx_peek(ctx) == TOKEN_OPEN){
        // sizeof (value)

        ast_expr_t *value;
        if(parse_primary_expr(ctx, &value)) return FAILURE;

        ast_expr_sizeof_value_t *sizeof_value_expr = malloc(sizeof(ast_expr_sizeof_value_t));
        sizeof_value_expr->id = EXPR_SIZEOF_VALUE;
        sizeof_value_expr->source = source;
        sizeof_value_expr->value = value;
        *out_expr = (ast_expr_t*) sizeof_value_expr;
    } else {
        // sizeof Type

        ast_type_t type;
        if(parse_type(ctx, &type)) return FAILURE;

        ast_expr_sizeof_t *sizeof_expr = malloc(sizeof(ast_expr_sizeof_t));
        sizeof_expr->id = EXPR_SIZEOF;
        sizeof_expr->source = source;
        sizeof_expr->type = type;
        *out_expr = (ast_expr_t*) sizeof_expr;
    }

    return SUCCESS;
}

errorcode_t parse_expr_alignof(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // alignof Type

    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    ast_type_t type;
    if(parse_type(ctx, &type)) return FAILURE;

    ast_expr_alignof_t *alignof_expr = malloc(sizeof(ast_expr_alignof_t));
    alignof_expr->id = EXPR_ALIGNOF;
    alignof_expr->source = source;
    alignof_expr->type = type;
    *out_expr = (ast_expr_t*) alignof_expr;
    return SUCCESS;
}

errorcode_t parse_expr_unary(parse_ctx_t *ctx, unsigned int expr_id, ast_expr_t **out_expr){
    source_t source = parse_ctx_peek_source(ctx);
    *ctx->i += 1;

    ast_expr_t *value;
    if(parse_primary_expr(ctx, &value)) return FAILURE;

    ast_expr_unary_t *unary_expr = malloc(sizeof(ast_expr_unary_t));
    unary_expr->id = expr_id;
    unary_expr->source = source;
    unary_expr->value = value;
    *out_expr = (ast_expr_t*) unary_expr;
    return SUCCESS;
}

errorcode_t parse_expr_new(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes current token is 'new' keyword

    length_t *i = ctx->i;
    source_t source = ctx->tokenlist->sources[*i];

    // Skip over 'new' keyword
    *i += 1;

    if(parse_ctx_peek(ctx) == TOKEN_CSTRING){
        // This is actually an 'ast_expr_new_cstring_t' expression,
        // instead of a 'ast_expr_new_t' expression

        ast_expr_new_cstring_t *new_cstring_expr = malloc(sizeof(ast_expr_new_cstring_t));

        *new_cstring_expr = (ast_expr_new_cstring_t){
            .id = EXPR_NEW_CSTRING,
            .source = source,
            .value = (char*) parse_ctx_peek_data(ctx),
        };

        *i += 1;
        *out_expr = (ast_expr_t*) new_cstring_expr;
        return SUCCESS;
    }

    ast_expr_new_t *new_expr = malloc(sizeof(ast_expr_new_t));

    *new_expr = (ast_expr_new_t){
        .id = EXPR_NEW,
        .type = (ast_type_t){0},
        .amount = NULL,
        .is_undef = false,
        .source = source,
        .inputs = (optional_ast_expr_list_t){0},
    };

    if(parse_eat(ctx, TOKEN_UNDEF, NULL) == SUCCESS){
        new_expr->is_undef = true;
    }

    if(parse_type(ctx, &new_expr->type)){
        goto failure;
    }

    if(parse_eat(ctx, TOKEN_OPEN, NULL) == SUCCESS){
        ast_expr_list_t *list = &new_expr->inputs.value;

        if(parse_expr_arguments(ctx, &list->expressions, &list->length, &list->capacity)){
            goto failure;
        }

        new_expr->inputs.has = true;
    }

    if(parse_eat(ctx, TOKEN_MULTIPLY, NULL) == SUCCESS){
        if(parse_primary_expr(ctx, &new_expr->amount)){
            goto failure;
        }
    }

    *out_expr = (ast_expr_t*) new_expr;
    return SUCCESS;

failure:
    ast_expr_free_fully((ast_expr_t*) new_expr);
    return FAILURE;
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
    trait_t traits = TRAIT_NONE;

    unsigned int expr_id = (tokens[*i].id == TOKEN_UNDEF ? EXPR_ILDECLAREUNDEF : EXPR_ILDECLARE);
    source_t source = sources[(*i)++];

    maybe_null_weak_cstr_t name = parse_eat_word(ctx, "Expected variable name for inline declaration");
    if(name == NULL) return FAILURE;

    if(parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS){
        traits |= AST_EXPR_DECLARATION_POD;
    }

    ast_type_t type;
    if(parse_type(ctx, &type)) return FAILURE;

    ast_expr_t *value = NULL;

    if(parse_eat(ctx, TOKEN_ASSIGN, NULL) == SUCCESS){
        if(expr_id == EXPR_ILDECLAREUNDEF){
            compiler_panic(ctx->compiler, sources[*i - 1], "Can't initialize undefined inline variable");
            printf("\nDid you mean to use 'def' instead of 'undef'?\n");
            ast_type_free(&type);
            return FAILURE;
        }

        if(parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS){
            traits |= AST_EXPR_DECLARATION_ASSIGN_POD;
        }

        if(parse_expr(ctx, &value)){
            ast_type_free(&type);
            return FAILURE;
        }
    }

    *out_expr = ast_expr_create_declaration(expr_id, source, name, type, traits, value, NO_AST_EXPR_LIST);
    return SUCCESS;
}

errorcode_t parse_expr_typeinfo(parse_ctx_t *ctx, ast_expr_t **out_expr){
    ast_expr_typeinfo_t *typeinfo = malloc(sizeof(ast_expr_typeinfo_t));
    typeinfo->id = EXPR_TYPEINFO;
    typeinfo->source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_type(ctx, &typeinfo->type)){
        free(typeinfo);
        return FAILURE;
    }

    *out_expr = (ast_expr_t*) typeinfo;
    return SUCCESS;
}

errorcode_t parse_expr_mutable_unary_prefix(parse_ctx_t *ctx, unsigned int unary_expr_id, const char *readable_operator, ast_expr_t **out_expr){
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    ast_expr_t *value;

    if(parse_primary_expr(ctx, &value) || parse_op_expr(ctx, 0, &value, true)){
        return FAILURE;
    }

    if(!expr_is_mutable(value)){
        compiler_panicf(ctx->compiler, value->source, "The '%s' operator requires the operand to be mutable", readable_operator);
        return FAILURE;
    }
    
    *out_expr = ast_expr_create_unary(unary_expr_id, source, value);
    return SUCCESS;
}

errorcode_t parse_expr_initlist(parse_ctx_t *ctx, ast_expr_t **out_expr){
    // NOTE: Assumes first token is '{'
    
    ast_expr_list_t values = (ast_expr_list_t){0};
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_ignore_newlines(ctx, "Expected '}' or ',' in initializer list before end of file")){
        goto failure;
    }

    while(parse_ctx_peek(ctx) != TOKEN_END){
        // Parse single value
        ast_expr_t *element;
        if(parse_expr(ctx, &element)) goto failure;

        ast_expr_list_append(&values, element);

        if(parse_ignore_newlines(ctx, "Expected '}' or ',' in initializer list before end of file")){
            goto failure;
        }

        if(parse_eat(ctx, TOKEN_NEXT, NULL) == SUCCESS){
            if(parse_ignore_newlines(ctx, "Expected '}' or ',' in initializer list before end of file")){
                goto failure;
            }
        } else if(parse_ctx_peek(ctx) != TOKEN_END){
            compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), "Expected '}' or ',' in initializer list");
            goto failure;
        }
    }

    // Skip over '}' token
    *ctx->i += 1;

    *out_expr = ast_expr_create_initlist(source, values.expressions, values.length);
    return SUCCESS;

failure:
    ast_expr_list_free(&values);
    return FAILURE;
}

int parse_get_precedence(unsigned int id){
    switch(id){
    case TOKEN_MAYBE:
        return 1;
    case TOKEN_UBERAND:
    case TOKEN_UBEROR:
        return 2;
    case TOKEN_AND:
    case TOKEN_OR:
        return 3;
    case TOKEN_EQUALS:
    case TOKEN_NOTEQUALS:
    case TOKEN_LESSTHAN:
    case TOKEN_GREATERTHAN:
    case TOKEN_LESSTHANEQ:
    case TOKEN_GREATERTHANEQ:
        return 4;
    case TOKEN_ADD:
    case TOKEN_SUBTRACT:
    case TOKEN_WORD:
        return 5;
    case TOKEN_MULTIPLY:
    case TOKEN_DIVIDE:
    case TOKEN_MODULUS:
        return 6;
    case TOKEN_AS:
        return 7;
    default:
        return 0;
    }
}
