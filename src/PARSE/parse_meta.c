
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_dependency.h"

errorcode_t parse_meta(parse_ctx_t *ctx){
    // NOTE: Assumes (ctx->tokenlist->tokens[*ctx->i].id == TOKEN_META)

    length_t *i = ctx->i;
    tokenlist_t *tokenlist = ctx->tokenlist;
    source_t source = tokenlist->sources[*i];

    char *directive_name = tokenlist->tokens[*i].data;

    const char *standard_directives[] = {
        "default", "define", "elif", "else", "end", "error", "get", "halt", "if", "import", "input", "place", "place_error", "place_warning",
        "print", "print_error", "print_warning", "set", "unless"
    };

    #define META_DIRECTIVE_DEFAULT 0
    #define META_DIRECTIVE_DEFINE 1
    #define META_DIRECTIVE_ELIF 2
    #define META_DIRECTIVE_ELSE 3
    #define META_DIRECTIVE_END 4
    #define META_DIRECTIVE_ERROR 5
    #define META_DIRECTIVE_GET 6
    #define META_DIRECTIVE_HALT 7
    #define META_DIRECTIVE_IF 8
    #define META_DIRECTIVE_IMPORT 9
    #define META_DIRECTIVE_INPUT 10
    #define META_DIRECTIVE_PLACE 11
    #define META_DIRECTIVE_PLACE_ERROR 12
    #define META_DIRECTIVE_PLACE_WARNING 13
    #define META_DIRECTIVE_PRINT 14
    #define META_DIRECTIVE_PRINT_ERROR 15
    #define META_DIRECTIVE_PRINT_WARNING 16
    #define META_DIRECTIVE_SET 17
    #define META_DIRECTIVE_UNLESS 18

    maybe_index_t standard = binary_string_search(standard_directives, sizeof(standard_directives) / sizeof(char*), directive_name);

    if(standard == -1){
        compiler_panicf(ctx->compiler, source, "Unrecognized meta directive #%s", directive_name);
        return FAILURE;
    }

    switch(standard){
    case META_DIRECTIVE_DEFAULT: { // default
            char *definition_name = parse_grab_word(ctx, "Expected transcendent variable name after #default");
            if(!definition_name) return FAILURE;
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            meta_definition_t *existing = meta_definition_find(ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, definition_name);

            if(existing == NULL){
                meta_definition_add(&ctx->ast->meta_definitions, &ctx->ast->meta_definitions_length, &ctx->ast->meta_definitions_capacity, definition_name, value);
            } else {
                meta_expr_free_fully(value);
            }
        }
        break;
    case META_DIRECTIVE_ELIF: {
            // This gets triggered when #if was true

            // Ensure #elif is allowed here
            if(!parse_ctx_get_meta_else_allowed(ctx, ctx->meta_ends_expected)){
                compiler_panic(ctx->compiler, source, "Unexpected '#elif' meta directive");
                return FAILURE;
            }

            // Pass over #elif expression
            (*i)++;
            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            meta_expr_free_fully(value);

            // Skip over everything until #end
            length_t ends_needed = 1;

            while(++(*i) != tokenlist->length && ends_needed != 0){
                if(tokenlist->tokens[*i].id != TOKEN_META) continue;

                char *pass_over_directive_name = (char*) tokenlist->tokens[*i].data;
                maybe_index_t pass_id = binary_string_search(standard_directives, sizeof(standard_directives) / sizeof(char*), pass_over_directive_name);

                switch(pass_id){
                case META_DIRECTIVE_IF: case META_DIRECTIVE_UNLESS:
                    ends_needed++;
                    break;
                case META_DIRECTIVE_END:
                    ends_needed--;
                    break;
                case -1:
                    compiler_panicf(ctx->compiler, tokenlist->sources[*i], "Unrecognized meta directive #%s", pass_over_directive_name);
                    return FAILURE;
                }
            }

            if(*i == tokenlist->length){
                compiler_panic(ctx->compiler, source, "Expected '#end' meta directive before termination");
                return FAILURE;
            }
        }
        break;
    case META_DIRECTIVE_ELSE: {
            // This gets triggered when #if was true

            // Ensure #else if allowed here
            if(!parse_ctx_get_meta_else_allowed(ctx, ctx->meta_ends_expected)){
                compiler_panic(ctx->compiler, source, "Unexpected '#else' meta directive");
                return FAILURE;
            }

            // Skip over everything until #end
            length_t ends_needed = 1;

            while(++(*i) != tokenlist->length && ends_needed != 0){
                if(tokenlist->tokens[*i].id != TOKEN_META) continue;

                char *pass_over_directive_name = (char*) tokenlist->tokens[*i].data;
                maybe_index_t pass_id = binary_string_search(standard_directives, sizeof(standard_directives) / sizeof(char*), pass_over_directive_name);

                switch(pass_id){
                case META_DIRECTIVE_IF: case META_DIRECTIVE_UNLESS:
                    ends_needed++;
                    break;
                case META_DIRECTIVE_END:
                    ends_needed--;
                    break;
                case META_DIRECTIVE_ELSE:
                    if(ends_needed == 1){
                        compiler_panic(ctx->compiler, tokenlist->sources[*i], "Unexpected '#else' meta directive");
                        return FAILURE;
                    }
                    break;
                case -1:
                    compiler_panicf(ctx->compiler, tokenlist->sources[*i], "Unrecognized meta directive #%s", pass_over_directive_name);
                    return FAILURE;
                }
            }

            if(*i == tokenlist->length){
                compiler_panic(ctx->compiler, source, "Expected '#end' meta directive before termination");
                return FAILURE;
            }
        }
        break;
    case META_DIRECTIVE_END: // end
        if(ctx->meta_ends_expected-- == 0){
            compiler_panic(ctx->compiler, source, "Unexpected '#end' meta directive");
            return FAILURE;
        }
        (*i)++;
        break;
    case META_DIRECTIVE_ERROR: // error
        (*i)++;

        meta_expr_t *value;
        if(parse_meta_expr(ctx, &value)) return FAILURE;
        if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

        char *print_value = meta_expr_str(value);
        compiler_panicf(ctx->compiler, source, "%s", print_value);
        free(print_value);

        meta_expr_free_fully(value);
        
        #ifndef ADEPT_INSIGHT_BUILD
        return FAILURE;
        #else
        break;
        #endif
    case META_DIRECTIVE_HALT: // halt
        ctx->compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        return FAILURE;
    case META_DIRECTIVE_IF: case META_DIRECTIVE_UNLESS: { // if, unless
            bool is_unless = standard == META_DIRECTIVE_UNLESS;
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;

            bool whether;
            if(meta_expr_into_bool(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value, &whether)){
                meta_expr_free_fully(value);
                return FAILURE;
            }
            
            whether ^= is_unless;
            meta_expr_free_fully(value);

            // If true, continue parsing as normal
            if(whether){
                if(ctx->meta_ends_expected++ == 256){
                    compiler_panic(ctx->compiler, source, "Exceeded maximum meta conditional depth of 256");
                    return FAILURE;
                }
                parse_ctx_set_meta_else_allowed(ctx, ctx->meta_ends_expected, true);
                break;
            }

            // Otherwise, skip over this section until either #else, #elif, or #end
            length_t ends_needed = 1;

            while(++(*i) != tokenlist->length && ends_needed != 0){
                if(tokenlist->tokens[*i].id != TOKEN_META) continue;

                char *pass_over_directive_name = (char*) tokenlist->tokens[*i].data;
                maybe_index_t pass_id = binary_string_search(standard_directives, sizeof(standard_directives) / sizeof(char*), pass_over_directive_name);

                if(pass_id == META_DIRECTIVE_IF || pass_id == META_DIRECTIVE_UNLESS){
                    ends_needed++;
                } else if(pass_id == META_DIRECTIVE_END){
                    ends_needed--;
                } else if(pass_id == META_DIRECTIVE_ELIF){
                    if(ends_needed == 1){
                        (*i)++;

                        meta_expr_t *value;
                        if(parse_meta_expr(ctx, &value)) return FAILURE;

                        bool whether;
                        if(meta_expr_into_bool(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value, &whether)){
                            meta_expr_free_fully(value);
                            return FAILURE;
                        }

                        meta_expr_free_fully(value);

                        if(whether){   
                            if(ctx->meta_ends_expected++ == 256){
                                compiler_panic(ctx->compiler, tokenlist->sources[*i], "Exceeded maximum meta conditional depth of 256");
                                return FAILURE;
                            }
                            parse_ctx_set_meta_else_allowed(ctx, ctx->meta_ends_expected, true);
                            break;
                        }
                    }
                } else if(pass_id == META_DIRECTIVE_ELSE){
                    if(ends_needed == 1){
                        if(ctx->meta_ends_expected++ == 256){
                            compiler_panic(ctx->compiler, tokenlist->sources[*i], "Exceeded maximum meta conditional depth of 256");
                            return FAILURE;
                        }
                        parse_ctx_set_meta_else_allowed(ctx, ctx->meta_ends_expected, false);
                        break;
                    }
                } else if(pass_id == -1){
                    compiler_panicf(ctx->compiler, tokenlist->sources[*i], "Unrecognized meta directive #%s", pass_over_directive_name);
                    return FAILURE;
                }
            }

            if(*i == tokenlist->length){
                compiler_panic(ctx->compiler, source, "Expected '#end' meta directive before termination");
                return FAILURE;
            }
        }
        break;
    case META_DIRECTIVE_IMPORT: {
            length_t old_i = (*i)++;
            
            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            length_t new_i = *i;

            strong_cstr_t file;
            bool should_exit = meta_expr_into_string(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value, &file);
            meta_expr_free_fully(value);
            if(should_exit) return FAILURE;
            
            *i = old_i;
            
            maybe_null_strong_cstr_t target = parse_find_import(ctx, file, source, true);
            free(file);

            if(target == NULL){
                return FAILURE;
            }

            maybe_null_strong_cstr_t absolute = parse_resolve_import(ctx, target);
            if(absolute == NULL){
                free(target);
                return FAILURE;
            }

            if(already_imported(ctx, absolute)){
                free(target);
                free(absolute);
                return SUCCESS;
            }

            if(parse_import_object(ctx, target, absolute)) return FAILURE;
            *i = new_i;
        }
        break;
    case META_DIRECTIVE_INPUT: {
            char *definition_name = parse_grab_word(ctx, "Expected transcendent variable name after #set");
            if(!definition_name) return FAILURE;
            (*i)++;

            char input_str[512];
            fgets(input_str, 512, stdin);
            input_str[strcspn(input_str, "\n")] = '\0';

            meta_expr_str_t *value = malloc(sizeof(meta_expr_str_t));
            value->id = META_EXPR_STR;
            value->value = strclone(input_str);

            meta_definition_t *existing = meta_definition_find(ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, definition_name);

            if(existing){
                meta_expr_free_fully(existing->value);
                existing->value = (meta_expr_t*) value;
            } else {
                meta_definition_add(&ctx->ast->meta_definitions, &ctx->ast->meta_definitions_length, &ctx->ast->meta_definitions_capacity, definition_name, (meta_expr_t*) value);
            }
        }
        break;
    case META_DIRECTIVE_PLACE: { // place
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            printf("%s", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_PLACE_ERROR: { // place_error
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            redprintf("%s", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_PLACE_WARNING: { // place_warning
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            yellowprintf("%s", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_PRINT: { // print
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            printf("%s\n", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_PRINT_ERROR: { // print_error
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            redprintf("%s\n", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_PRINT_WARNING: { // print_warning
            (*i)++;

            meta_expr_t *value;
            if(parse_meta_expr(ctx, &value)) return FAILURE;
            if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;

            char *print_value = meta_expr_str(value);
            yellowprintf("%s\n", print_value);
            free(print_value);

            meta_expr_free_fully(value);
        }
        break;
    case META_DIRECTIVE_SET: case META_DIRECTIVE_DEFINE: { // set, define
            char *definition_name = parse_grab_word(ctx, "Expected transcendent variable name after #set");
            if(!definition_name) return FAILURE;

            meta_expr_t *value = NULL;

            if(tokenlist->tokens[++(*i)].id != TOKEN_NEWLINE || standard != META_DIRECTIVE_DEFINE){
                if(parse_meta_expr(ctx, &value)) return FAILURE;
                if(meta_collapse(ctx->compiler, ctx->object, ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, &value)) return FAILURE;
            }
            
            if(value == NULL){
                value = malloc(sizeof(meta_expr_t));
                value->id = META_EXPR_NULL;

                if(!(ctx->compiler->traits & COMPILER_UNSAFE_META)){
                    if(compiler_warnf(ctx->compiler, source, "WARNING: No value given for definition of '%s'", definition_name)){
                        meta_expr_free_fully(value);
                        return FAILURE;
                    }
                }
            }
            
            meta_definition_t *existing = meta_definition_find(ctx->ast->meta_definitions, ctx->ast->meta_definitions_length, definition_name);

            if(existing){
                meta_expr_free_fully(existing->value);
                existing->value = value;
            } else {
                meta_definition_add(&ctx->ast->meta_definitions, &ctx->ast->meta_definitions_length, &ctx->ast->meta_definitions_capacity, definition_name, value);
            }
        }
        break;
    case META_DIRECTIVE_GET: { // get
            compiler_panicf(ctx->compiler, source, "Meta directive #%s must be used in an inline expression", directive_name);
            return FAILURE;
        }
        break;
    default:
        compiler_panicf(ctx->compiler, source, "Unimplemented meta directive #%s", directive_name);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_meta_expr(parse_ctx_t *ctx, meta_expr_t **out_expr){
    if(parse_meta_primary_expr(ctx, out_expr)) return FAILURE;
    if(parse_meta_op_expr(ctx, 0, out_expr)) return FAILURE;
    return SUCCESS;
}

errorcode_t parse_meta_primary_expr(parse_ctx_t *ctx, meta_expr_t **out_expr){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    switch (tokens[*i].id){
    case TOKEN_TRUE:
        *out_expr = malloc(sizeof(meta_expr_t));
        (*out_expr)->id = META_EXPR_TRUE;
        (*i)++;
        break;
    case TOKEN_FALSE:
        *out_expr = malloc(sizeof(meta_expr_t));
        (*out_expr)->id = META_EXPR_FALSE;
        (*i)++;
        break;
    case TOKEN_NULL:
        *out_expr = malloc(sizeof(meta_expr_t));
        (*out_expr)->id = META_EXPR_NULL;
        (*i)++;
        break;
    case TOKEN_CSTRING:
        *out_expr = malloc(sizeof(meta_expr_str_t));
        (*out_expr)->id = META_EXPR_STR;
        ((meta_expr_str_t*) *out_expr)->value = strclone(tokens[*i].data);
        (*i)++;
        break;
    case TOKEN_STRING: {
            token_string_data_t *token_data = tokens[*i].data;
            meta_expr_str_t *str_expr = malloc(sizeof(meta_expr_str_t));
            str_expr->id = META_EXPR_STR;
            str_expr->value = malloc(token_data->length + 1);
            memcpy(str_expr->value, token_data->array, token_data->length);
            str_expr->value[token_data->length] = '\0';
            *out_expr = (meta_expr_t*) str_expr;
            (*i)++;
        }
        break;
    case TOKEN_WORD:
        *out_expr = malloc(sizeof(meta_expr_var_t));
        (*out_expr)->id = META_EXPR_VAR;
        ((meta_expr_var_t*) *out_expr)->name = strclone(tokens[*i].data);
        ((meta_expr_var_t*) *out_expr)->source = sources[*i];
        (*i)++;
        break;
    case TOKEN_GENERIC_INT:
        *out_expr = malloc(sizeof(meta_expr_int_t));
        (*out_expr)->id = META_EXPR_INT;
        ((meta_expr_int_t*) *out_expr)->value = *((long long*) tokens[*i].data);
        (*i)++;
        break;
    case TOKEN_GENERIC_FLOAT:
        *out_expr = malloc(sizeof(meta_expr_float_t));
        (*out_expr)->id = META_EXPR_FLOAT;
        ((meta_expr_float_t*) *out_expr)->value = *((double*) tokens[*i].data);
        (*i)++;
        break;
    case TOKEN_OPEN:
        (*i)++;
        if(parse_meta_expr(ctx, out_expr) != 0) return FAILURE;
        if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after meta expression")) return FAILURE;
        break;
    case TOKEN_NOT: {
            (*i)++;
            if(parse_meta_primary_expr(ctx, out_expr)) return FAILURE;
            meta_expr_t *not_expr = malloc(sizeof(meta_expr_not_t));
            ((meta_expr_not_t*) not_expr)->id = META_EXPR_NOT;
            ((meta_expr_not_t*) not_expr)->value = *out_expr;
            *out_expr = not_expr;
        }
        break;
    default:
        parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unexpected token '%s' in meta expression");
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_meta_op_expr(parse_ctx_t *ctx, int precedence, meta_expr_t **inout_left){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    meta_expr_t *right, *expr;

    while(*i != ctx->tokenlist->length) {
        int operator = tokens[*i].id;
        int operator_precedence;

        switch(operator){
        case TOKEN_UBERAND: case TOKEN_UBEROR:
            operator_precedence = 1;
            break;
        case TOKEN_AND: case TOKEN_OR:
            operator_precedence =  2;
            break;
        case TOKEN_BIT_XOR:
            operator_precedence = 3;
            break;
        case TOKEN_EQUALS: case TOKEN_NOTEQUALS:
        case TOKEN_LESSTHAN: case TOKEN_GREATERTHAN:
        case TOKEN_LESSTHANEQ: case TOKEN_GREATERTHANEQ:
            operator_precedence =  4;
            break;
        case TOKEN_ADD: case TOKEN_SUBTRACT: case TOKEN_WORD:
            operator_precedence =  5;
            break;
        case TOKEN_DIVIDE: case TOKEN_MODULUS:
            operator_precedence =  6;
            break;
        case TOKEN_MULTIPLY:
            operator_precedence = (*i + 1 != ctx->tokenlist->length && tokens[*i + 1].id == TOKEN_MULTIPLY) ? 7 : 6;
            break;
        default:
            operator_precedence =  0;
        }

        if(operator_precedence < precedence) return SUCCESS;
        //source_t source = sources[*i];

        if(operator == TOKEN_NEWLINE || operator == TOKEN_CLOSE || operator == TOKEN_NEXT
            || operator == TOKEN_BEGIN || operator == TOKEN_BRACKET_CLOSE) return SUCCESS;

        #define BUILD_MATH_META_EXPR_MACRO(new_built_expr_id) { \
            if(meta_parse_rhs_expr(ctx, inout_left, &right, operator_precedence)) return FAILURE; \
            expr = malloc(sizeof(meta_expr_math_t)); \
            ((meta_expr_math_t*) expr)->id = new_built_expr_id; \
            ((meta_expr_math_t*) expr)->a = *inout_left; \
            ((meta_expr_math_t*) expr)->b = right; \
            *inout_left = expr; \
        }

        switch(operator){
        case TOKEN_ADD:            BUILD_MATH_META_EXPR_MACRO(META_EXPR_ADD);        break;
        case TOKEN_SUBTRACT:       BUILD_MATH_META_EXPR_MACRO(META_EXPR_SUB);        break;
        case TOKEN_MULTIPLY:
            if(*i + 1 != ctx->tokenlist->length && tokens[*i + 1].id == TOKEN_MULTIPLY){    
                (*i)++;
                BUILD_MATH_META_EXPR_MACRO(META_EXPR_POW);
                break;
            }
            BUILD_MATH_META_EXPR_MACRO(META_EXPR_MUL);
            break;
        case TOKEN_DIVIDE:         BUILD_MATH_META_EXPR_MACRO(META_EXPR_DIV);        break;
        case TOKEN_MODULUS:        BUILD_MATH_META_EXPR_MACRO(META_EXPR_MOD);        break;
        case TOKEN_AND:
        case TOKEN_UBERAND:        BUILD_MATH_META_EXPR_MACRO(META_EXPR_AND);        break;
        case TOKEN_OR:
        case TOKEN_UBEROR:         BUILD_MATH_META_EXPR_MACRO(META_EXPR_OR);         break;
        case TOKEN_BIT_XOR:        BUILD_MATH_META_EXPR_MACRO(META_EXPR_XOR);        break;
        case TOKEN_EQUALS:         BUILD_MATH_META_EXPR_MACRO(META_EXPR_EQ);         break;
        case TOKEN_NOTEQUALS:      BUILD_MATH_META_EXPR_MACRO(META_EXPR_NEQ);        break;
        case TOKEN_LESSTHAN:       BUILD_MATH_META_EXPR_MACRO(META_EXPR_LT);         break;
        case TOKEN_GREATERTHAN:    BUILD_MATH_META_EXPR_MACRO(META_EXPR_GT);         break;
        case TOKEN_LESSTHANEQ:     BUILD_MATH_META_EXPR_MACRO(META_EXPR_LTE);        break;
        case TOKEN_GREATERTHANEQ:  BUILD_MATH_META_EXPR_MACRO(META_EXPR_GTE);        break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Unrecognized operator '%s' in meta expression");
            meta_expr_free_fully(*inout_left);
            return FAILURE;
        }

        #undef BUILD_MATH_META_EXPR_MACRO
    }

    return SUCCESS;
}

errorcode_t meta_parse_rhs_expr(parse_ctx_t *ctx, meta_expr_t **left, meta_expr_t **out_right, int op_prec){
    // Expects from 'ctx': compiler, object, tokenlist, i

    // NOTE: Handles all of the work with parsing the right hand side of a meta expression
    // All errors are taken care of inside this function
    // Freeing of 'left' expression performed on error
    // Expects 'i' to point to the operator token

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    (*i)++; // Skip over operator token

    if(parse_ignore_newlines(ctx, "Unexpected meta expression termination")) return FAILURE;

    if(parse_meta_primary_expr(ctx, out_right) || (op_prec < parse_get_precedence(tokens[*i].id) && parse_meta_op_expr(ctx, op_prec + 1, out_right))){
        meta_expr_free_fully(*left);
        return FAILURE;
    }

    return SUCCESS;
}
