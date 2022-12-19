
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_expr.h"
#include "AST/ast_named_expression.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_global.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_stmt.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"
#include "UTIL/string_builder.h"

defer_scope_t defer_scope_create(defer_scope_t *parent, weak_cstr_t label, trait_t traits){
    return (defer_scope_t){
        .list = (ast_expr_list_t){0},
        .parent = parent,
        .label = label,
        .traits = traits,
    };
}

void defer_scope_free(defer_scope_t *defer_scope){
    ast_expr_list_free(&defer_scope->list);
}

length_t defer_scope_total(defer_scope_t *defer_scope){
    length_t total = 0;

    while(defer_scope){
        total += defer_scope->list.length;
        defer_scope = defer_scope->parent;
    }

    return total;
}

void defer_scope_fulfill(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list){
    for(length_t r = defer_scope->list.length; r != 0; r--){
        ast_expr_list_append(stmt_list, defer_scope->list.statements[r - 1]);
    }

    defer_scope->list.length = 0;
}

void defer_scope_fulfill_by_cloning(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list){
    for(length_t r = defer_scope->list.length; r != 0; r--){
        ast_expr_list_append(stmt_list, ast_expr_clone(defer_scope->list.statements[r - 1]));
    }
}

void defer_scope_rewind(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list, trait_t scope_trait, weak_cstr_t label){
    defer_scope_fulfill(defer_scope, stmt_list);
    
    while((!(defer_scope->traits & scope_trait) || (label && defer_scope->label && !streq(defer_scope->label, label))) && defer_scope->parent != NULL){
        defer_scope = defer_scope->parent;
        defer_scope_fulfill_by_cloning(defer_scope, stmt_list);
    }
}

ast_expr_list_t defer_scope_unwind_completely(defer_scope_t *defer_scope){
    ast_expr_list_t list = ast_expr_list_create(defer_scope_total(defer_scope));

    defer_scope_fulfill(defer_scope, &list);

    // Duplicate defer statements of ancestors
    for(defer_scope_t *traverse = defer_scope->parent; traverse; traverse = traverse->parent){
        defer_scope_fulfill_by_cloning(traverse, &list);
    }

    return list;
}

errorcode_t parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope, trait_t mode){
    // NOTE: Outputs statements to stmt_list
    // NOTE: Ends on 'i' pointing to a '}' token
    // NOTE: Even if this function returns FAILURE, statements appended to stmt_list still must be freed

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    source_t source = (source_t){
        .index = 0,
        .object_index = ctx->object->index,
        .stride = 0,
    };

    while(parse_ctx_peek(ctx) != TOKEN_END){
        if(parse_ignore_newlines(ctx, "Unexpected expression termination")) return FAILURE;

        expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, 1, 8);

        // Parse a statement into the statement list
        switch(parse_ctx_peek(ctx)){
        case TOKEN_END:
            return SUCCESS;
        case TOKEN_RETURN: {
                ast_expr_t *return_expression;
                source = sources[(*i)++]; // Pass over return keyword

                if(tokens[*i].id == TOKEN_NEWLINE){
                    return_expression = NULL;
                } else if(parse_expr(ctx, &return_expression)){
                    return FAILURE;
                }

                ast_expr_list_append_unchecked(stmt_list, ast_expr_create_return(source, return_expression, defer_scope_unwind_completely(defer_scope)));
            }
            break;
        case TOKEN_STRING: {
                // Allow statements such as: "This is a string".doSomething()

                ast_expr_t *expression;
                if(parse_expr(ctx, &expression)) return FAILURE;
                
                if(expression->id != EXPR_CALL_METHOD){
                    compiler_panicf(ctx->compiler, expression->source, "Expression not supported as a statement");
                    ast_expr_free(expression);
                    return FAILURE;
                }

                ast_expr_list_append_unchecked(stmt_list, expression);
            }
            break;
        case TOKEN_DEFINE:
            if(parse_local_named_expression_declaration(ctx, stmt_list, sources[*i])) return FAILURE;
            break;
        case TOKEN_WORD: {
                source = sources[(*i)++]; // Read ahead to see what type of statement this is

                switch(parse_ctx_peek(ctx)){
                case TOKEN_MAYBE:
                    *i += 1;

                    if(parse_eat(ctx, TOKEN_OPEN, NULL) == SUCCESS){
                        if(parse_stmt_call(ctx, stmt_list, true)) return FAILURE;
                    } else {
                        // TODO: Have a better error message here
                        parse_panic_token(ctx, parse_ctx_peek_source(ctx), parse_ctx_peek(ctx), "Encountered unexpected token '%s' at beginning of statement");
                        return FAILURE;
                    }
                    break;
                case TOKEN_OPEN:
                    *i += 1;
                    if(parse_stmt_call(ctx, stmt_list, false)) return FAILURE;
                    break;
                case TOKEN_WORD: case TOKEN_FUNC:
                case TOKEN_STDCALL: case TOKEN_NEXT: case TOKEN_POD:
                case TOKEN_GENERIC_INT: /*fixed array*/ case TOKEN_MULTIPLY: /*pointer*/
                case TOKEN_LESSTHAN: case TOKEN_BIT_LSHIFT: case TOKEN_BIT_LGC_LSHIFT: /*generics*/
                case TOKEN_POLYMORPH: /*polymorphic type*/ case TOKEN_COLON: /*experimental : type syntax*/
                case TOKEN_POLYCOUNT: /*polymorphic count*/
                case TOKEN_STRUCT: case TOKEN_PACKED: case TOKEN_UNION: /* anonymous composites */
                    *i -= 1;
                    if(parse_stmt_declare(ctx, stmt_list)) return FAILURE;
                    break;
                case TOKEN_BRACKET_OPEN: /* ambiguous case between declaration and usage */
                    *i -= 1;
                    if(parse_ambiguous_open_bracket(ctx, stmt_list)) return FAILURE;
                    break;
                default:
                    // Assume mutable expression operation statement if not one of the above
                    *i -= 1;
                    if(parse_mutable_expr_operation(ctx, stmt_list)) return FAILURE;
                    break;
                }
            }
            break;
        case TOKEN_CONST: case TOKEN_STATIC:
            if(parse_stmt_declare(ctx, stmt_list)) return FAILURE;
            break;
        case TOKEN_MULTIPLY: case TOKEN_OPEN: case TOKEN_INCREMENT: case TOKEN_DECREMENT:
        case TOKEN_BIT_AND: case TOKEN_BIT_OR: case TOKEN_BIT_XOR: case TOKEN_BIT_LSHIFT: case TOKEN_BIT_RSHIFT:
        case TOKEN_BIT_LGC_LSHIFT: case TOKEN_BIT_LGC_RSHIFT: /* DUPLICATE: case TOKEN_ADDRESS: */
            if(parse_mutable_expr_operation(ctx, stmt_list)) return FAILURE;
            break;
        case TOKEN_IF: case TOKEN_UNLESS:
            if(parse_onetime_conditional(ctx, stmt_list, defer_scope)) return FAILURE;
            break;
        case TOKEN_WHILE: case TOKEN_UNTIL: {
                unsigned int conditional_type = parse_ctx_peek(ctx);
                source = parse_ctx_peek_source(ctx);
                ast_expr_t *conditional = NULL;
                trait_t stmts_mode;
                maybe_null_weak_cstr_t label = NULL;

                *i += 1;

                if(parse_ctx_peek(ctx) == TOKEN_BREAK || parse_ctx_peek(ctx) == TOKEN_CONTINUE){
                    // 'while continue' or 'until break' loop
                    unsigned int condition_break_or_continue = tokens[*i].id;

                    if(conditional_type == TOKEN_WHILE && condition_break_or_continue != TOKEN_CONTINUE){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Did you mean to use 'while continue'? There is no such conditional as 'while break'");
                        return FAILURE;
                    } else if(conditional_type == TOKEN_UNTIL && condition_break_or_continue != TOKEN_BREAK){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Did you mean to use 'until break'? There is no such conditional as 'until continue'");
                        return FAILURE;
                    }

                    if(tokens[++(*i)].id == TOKEN_WORD){
                        label = tokens[(*i)++].data;
                    }
                } else {
                    // standard 'while <expr>' or 'until <expr>' loop
                    if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                        label = tokens[*i].data;
                        *i += 2;
                    }

                    if(parse_expr(ctx, &conditional)) return FAILURE;
                }

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                if(parse_block_beginning(ctx, "conditional", &stmts_mode)){
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                ast_expr_list_t while_stmt_list = ast_expr_list_create(4);
                defer_scope_t while_defer_scope = defer_scope_create(defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &while_stmt_list, &while_defer_scope, stmts_mode)){
                    ast_expr_list_free(&while_stmt_list);
                    ast_expr_free_fully(conditional);
                    defer_scope_free(&while_defer_scope);
                    return FAILURE;
                }
                
                defer_scope_free(&while_defer_scope);
                
                if(stmts_mode & PARSE_STMTS_SINGLE){
                    *i -= 1;
                } else {
                    *i += 1;
                }

                if(conditional == NULL){
                    // 'while continue' or 'until break' loop
                    ast_expr_whilecontinue_t *stmt = malloc(sizeof(ast_expr_whilecontinue_t));
                    stmt->id = (conditional_type == TOKEN_UNTIL) ? EXPR_UNTILBREAK : EXPR_WHILECONTINUE;
                    stmt->source = source;
                    stmt->label = label;
                    stmt->value = NULL;
                    stmt->statements = while_stmt_list;
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
                } else {
                    // 'while <expr>' or 'until <expr>' loop
                    ast_expr_while_t *stmt = malloc(sizeof(ast_expr_while_t));
                    stmt->id = (conditional_type == TOKEN_UNTIL) ? EXPR_UNTIL : EXPR_WHILE;
                    stmt->source = source;
                    stmt->label = label;
                    stmt->value = conditional;
                    stmt->statements = while_stmt_list;
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
                }
            }
            break;
        case TOKEN_EACH: {
                source = sources[(*i)++];

                maybe_null_strong_cstr_t it_name = NULL;
                ast_type_t *it_type = NULL;
                trait_t stmts_mode;
                maybe_null_weak_cstr_t label = NULL;

                // Used for 'each in [array, length]'
                ast_expr_t *length_limit = NULL;
                ast_expr_t *low_array = NULL;

                // Used for 'each in list'
                ast_expr_t *list_expr = NULL;

                // Attach label if present
                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data; *i += 2;
                }
                
                // Record override variable name for 'it' if present
                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id != TOKEN_IN){
                    it_name = parse_take_word(ctx, "Expected name for 'it' variable");
                    if(!it_name) return FAILURE;
                }

                it_type = malloc(sizeof(ast_type_t));

                // Grab expected element type and pass over 'in' keyword
                if(parse_type(ctx, it_type) || parse_eat(ctx, TOKEN_IN, "Expected 'in' keyword")){
                    free(it_name);
                    free(it_type);
                    return FAILURE;
                }

                bool is_static = tokens[*i].id == TOKEN_STATIC;
                if(is_static) *i += 1;

                // Handle values given for 'each in [array, length]' statement
                if(tokens[*i].id == TOKEN_BRACKET_OPEN){
                    (*i)++;

                    if(parse_expr(ctx, &low_array)){
                        ast_type_free_fully(it_type);
                        free(it_name);
                        return FAILURE;
                    }

                    if(tokens[(*i)++].id != TOKEN_NEXT){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Expected ',' after low-level array data in 'each in' statement");
                        ast_type_free_fully(it_type);
                        free(it_name);
                        return FAILURE;
                    }

                    if(parse_expr(ctx, &length_limit)){
                        ast_type_free_fully(it_type);
                        ast_expr_free_fully(low_array);
                        free(it_name);
                        return FAILURE;
                    }

                    if(tokens[(*i)++].id != TOKEN_BRACKET_CLOSE){
                        compiler_panic(ctx->compiler, sources[*i - 1], "Expected ']' after low-level array data and length in 'each in' statement");
                        ast_type_free_fully(it_type);
                        ast_expr_free_fully(low_array);
                        ast_expr_free_fully(length_limit);
                        free(it_name);
                        return FAILURE;
                    }
                }

                // Handle values given for 'each in list' statement
                else if(parse_expr(ctx, &list_expr)){
                    ast_type_free_fully(it_type);
                    free(it_name);
                    return FAILURE;
                }

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(list_expr);
                    free(it_name);
                    return FAILURE;
                }

                if(parse_block_beginning(ctx, "'each in'", &stmts_mode)){
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(list_expr);
                    free(it_name);
                    return FAILURE;
                }

                ast_expr_list_t each_in_stmt_list = ast_expr_list_create(4);
                defer_scope_t each_in_defer_scope = defer_scope_create(defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &each_in_stmt_list, &each_in_defer_scope, stmts_mode)){
                    ast_expr_list_free(&each_in_stmt_list);
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(list_expr);
                    free(it_name);
                    defer_scope_free(&each_in_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&each_in_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE){
                    *i -= 1;
                } else {
                    *i += 1;
                }

                // 'each in list' or 'each in [array, length]
                ast_expr_each_in_t *stmt = malloc(sizeof(ast_expr_each_in_t));
                stmt->id = EXPR_EACH_IN;
                stmt->source = source;
                stmt->label = label;
                stmt->it_name = it_name;
                stmt->it_type = it_type;
                stmt->length = length_limit;
                stmt->low_array = low_array;
                stmt->list = list_expr;
                stmt->statements = each_in_stmt_list;
                stmt->is_static = is_static;
                ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
            }
            break;
        case TOKEN_REPEAT: {
                source = sources[(*i)++];
                ast_expr_t *limit = NULL;
                trait_t stmts_mode;
                maybe_null_weak_cstr_t label = NULL;
                maybe_null_weak_cstr_t idx_name = NULL;

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data;
                    *i += 2;
                }

                bool is_static = (parse_eat(ctx, TOKEN_STATIC, NULL) == SUCCESS);

                if(parse_expr(ctx, &limit)) return FAILURE;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                if(tokens[*i].id == TOKEN_USING){
                    idx_name = parse_grab_word(ctx, "Expected name for 'idx' variable after 'using' keyword");

                    if(idx_name == NULL){
                        ast_expr_free_fully(limit);
                        return FAILURE;
                    }

                    // Move past idx variable name
                    (*i)++;

                    if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                        ast_expr_free_fully(limit);
                        return FAILURE;
                    }
                }

                if(parse_block_beginning(ctx, "'repeat'", &stmts_mode)){
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                ast_expr_list_t repeat_stmt_list = ast_expr_list_create(4);
                defer_scope_t repeat_defer_scope = defer_scope_create(defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &repeat_stmt_list, &repeat_defer_scope, stmts_mode)){
                    ast_expr_list_free(&repeat_stmt_list);
                    ast_expr_free_fully(limit);
                    defer_scope_free(&repeat_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&repeat_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE){
                    *i -= 1;
                } else {
                    *i += 1;
                }

                ast_expr_repeat_t *stmt = malloc(sizeof(ast_expr_repeat_t));
                stmt->id = EXPR_REPEAT;
                stmt->source = source;
                stmt->label = label;
                stmt->limit = limit;
                stmt->statements = repeat_stmt_list;
                stmt->is_static = is_static;
                stmt->idx_name = idx_name;
                ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
            }
            break;
        case TOKEN_DEFER: {
                defer_scope_t defer_defer_scope = defer_scope_create(defer_scope, NULL, TRAIT_NONE);

                (*i)++; // Skip over 'defer' keyword
                if(parse_stmts(ctx, &defer_scope->list, &defer_defer_scope, PARSE_STMTS_SINGLE)){
                    defer_scope_free(&defer_defer_scope);
                    return FAILURE;
                }
                (*i)--; // Go back to newline because we used PARSE_STMTS_SINGLE

                defer_scope_free(&defer_defer_scope);
            }
            break;
        case TOKEN_DELETE: {
                source = sources[(*i)++];

                ast_expr_t *value;
                if(parse_primary_expr(ctx, &value)) return FAILURE;

                ast_expr_list_append_unchecked(stmt_list, ast_expr_create_unary(EXPR_DELETE, source, value));
            }
            break;
        case TOKEN_BREAK: {
                if(tokens[++(*i)].id == TOKEN_WORD){
                    ast_expr_break_to_t *stmt = malloc(sizeof(ast_expr_break_to_t));
                    stmt->id = EXPR_BREAK_TO;
                    stmt->source = sources[*i - 1];
                    stmt->label_source = sources[*i];
                    stmt->label = tokens[(*i)++].data;

                    defer_scope_rewind(defer_scope, stmt_list, BREAKABLE, stmt->label);
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
                } else {
                    ast_expr_break_t *stmt = malloc(sizeof(ast_expr_break_t));
                    stmt->id = EXPR_BREAK;
                    stmt->source = sources[*i - 1];

                    defer_scope_rewind(defer_scope, stmt_list, BREAKABLE, NULL);
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
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

                    defer_scope_rewind(defer_scope, stmt_list, CONTINUABLE, stmt->label);
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
                } else {
                    ast_expr_continue_t *stmt = malloc(sizeof(ast_expr_continue_t));
                    stmt->id = EXPR_CONTINUE;
                    stmt->source = sources[*i - 1];

                    defer_scope_rewind(defer_scope, stmt_list, CONTINUABLE, NULL);
                    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
                }
            }
            break;
        case TOKEN_FALLTHROUGH: {
                ast_expr_fallthrough_t *stmt = malloc(sizeof(ast_expr_fallthrough_t));

                *stmt = (ast_expr_fallthrough_t){
                    .id = EXPR_FALLTHROUGH,
                    .source = sources[(*i)++],
                };

                defer_scope_rewind(defer_scope, stmt_list, FALLTHROUGHABLE, NULL);
                ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
            }
            break;
        case TOKEN_META:
            if(parse_meta(ctx)) return FAILURE;
            break;
        case TOKEN_EXHAUSTIVE:
            *i += 1;
            if(parse_switch(ctx, stmt_list, defer_scope, true)) return FAILURE;
            break;
        case TOKEN_SWITCH:
            if(parse_switch(ctx, stmt_list, defer_scope, false)) return FAILURE;
            break;
        case TOKEN_VA_START: case TOKEN_VA_END: {
                unsigned int expr_id = parse_ctx_peek(ctx) == TOKEN_VA_START ? EXPR_VA_START : EXPR_VA_END;
                source = sources[(*i)++];

                ast_expr_t *value;
                if(parse_expr(ctx, &value)) return FAILURE;

                ast_expr_list_append_unchecked(stmt_list, ast_expr_create_unary(expr_id, source, value));
            }
            break;
        case TOKEN_VA_COPY: {
                // va_copy(dest, src)
                //    ^

                source = sources[(*i)++];

                ast_expr_t *dest_value = NULL;
                ast_expr_t *src_value = NULL;
                
                if( parse_eat(ctx, TOKEN_OPEN, "Expected '(' after va_copy keyword")           // Eat '('
                 || parse_expr(ctx, &dest_value)                                               // Parse destination value
                 || parse_eat(ctx, TOKEN_NEXT, "Expected ',' after first parameter to va_arg") // Eat ','
                 || parse_expr(ctx, &src_value)                                                // Parse source value
                 || parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after va_arg parameters")        // Eat ')'
                ){
                    ast_expr_free_fully(dest_value);
                    ast_expr_free_fully(src_value);
                    return FAILURE;
                }

                ast_expr_list_append_unchecked(stmt_list, ast_expr_create_va_copy(source, dest_value, src_value));
            }
            break;
        case TOKEN_FOR: {
                // for(stmt; condition; stmt) {...}    for stmt; condition; stmt {...}
                //  ^                                   ^

                ast_expr_t *condition = NULL;
                ast_expr_list_t before, after, statements;
                weak_cstr_t label = NULL;
                source = sources[*i];
                
                memset(&before, 0, sizeof(ast_expr_list_t));
                memset(&after, 0, sizeof(ast_expr_list_t));
                memset(&statements, 0, sizeof(ast_expr_list_t));

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data;
                    *i += 2;
                }

                defer_scope_t for_defer_scope = defer_scope_create(defer_scope, label, BREAKABLE | CONTINUABLE);

                // Skip over 'for' keyword
                (*i)++;

                // Eat '(' if it exists
                if(tokens[*i].id == TOKEN_OPEN) (*i)++;

                if(tokens[*i].id != TOKEN_TERMINATE_JOIN && parse_stmts(ctx, &before, &for_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_NO_JOINING | PARSE_STMTS_PARENT_DEFER_SCOPE)){
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                if(parse_eat(ctx, TOKEN_TERMINATE_JOIN, "Expected ';' after first part of 'for' statement")){
                    ast_exprs_free_fully(before.statements, before.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                if(tokens[*i].id != TOKEN_TERMINATE_JOIN && parse_expr(ctx, &condition)){
                    ast_exprs_free_fully(before.statements, before.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                if(parse_eat(ctx, TOKEN_TERMINATE_JOIN, "Expected ';' after second part of 'for' statement")){
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                if(tokens[*i].id != TOKEN_NEXT && tokens[*i].id != TOKEN_BEGIN && tokens[*i].id != TOKEN_NEWLINE && tokens[*i].id != TOKEN_CLOSE){
                    // Put the 'after' statement directly in the defer statements of the 'for' loop
                    if(parse_stmts(ctx, &after, &for_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_NO_JOINING | PARSE_STMTS_PARENT_DEFER_SCOPE)){
                        ast_expr_free(condition);
                        ast_exprs_free_fully(before.statements, before.length);
                        defer_scope_free(&for_defer_scope);
                        return FAILURE;
                    }
                }

                // Eat ')' if it exists
                if(tokens[*i].id == TOKEN_CLOSE) (*i)++;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    ast_exprs_free_fully(after.statements, after.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                // Eat '{' or ','
                unsigned int stmts_mode;
                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after beginning parts of 'for' loop");
                    ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    ast_exprs_free_fully(after.statements, after.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }
                
                // Parse statements
                if(parse_stmts(ctx, &statements, &for_defer_scope, stmts_mode)){
                    ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    ast_exprs_free_fully(after.statements, after.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&for_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE){
                    *i -= 1;
                } else {
                    *i += 1;
                }

                ast_expr_list_append_unchecked(stmt_list, ast_expr_create_for(source, label, before, after, condition, statements));
            }
            break;
        case TOKEN_LLVM_ASM:
            if(parse_llvm_asm(ctx, stmt_list)) return FAILURE;
            break;
        case TOKEN_BEGIN:
            if(parse_conditionless_block(ctx, stmt_list, defer_scope)) return FAILURE;
            break;
        case TOKEN_ASSERT:
            if(parse_assert(ctx, stmt_list)) return FAILURE;
            break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at beginning of statement");
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_TERMINATE_JOIN && !(mode & PARSE_STMTS_NO_JOINING)){
            // Bypass single statement flag by "joining" 2+ statements
            (*i)++;
            continue;
        }

        // Continue over newline token
        // TODO: INVESTIGATE: Investigate whether TOKEN_META #else/#elif should be having (*i)++ here
        tokenid_t ending = tokens[*i].id;
        const void *ending_data = tokens[*i].data;

        if(ending == TOKEN_NEWLINE || (ending == TOKEN_META && (streq(ending_data, "else") || streq(ending_data, "elif")))){
            (*i)++;
        } else if(ending != TOKEN_ELSE && ending != TOKEN_TERMINATE_JOIN && ending != TOKEN_CLOSE && ending != TOKEN_BEGIN && ending != TOKEN_NEXT){
            parse_panic_token(ctx, sources[*i], ending, "Encountered unexpected token '%s' at end of statement");
            return FAILURE;
        }

        if(mode & PARSE_STMTS_SINGLE){
            if(!(mode & PARSE_STMTS_PARENT_DEFER_SCOPE)){
                defer_scope_fulfill(defer_scope, stmt_list);
            }
            return SUCCESS;
        }
    }
    
    if(!(mode & PARSE_STMTS_PARENT_DEFER_SCOPE)){
        defer_scope_fulfill(defer_scope, stmt_list);
    }

    // Mark all top-level calls to not allow discarding values that come
    // from functions that are marked as "no discard"
    for(length_t i = 0; i < stmt_list->length; i++){
        ast_expr_t *stmt = stmt_list->statements[i];

        switch(stmt->id){
        case EXPR_CALL:
            ((ast_expr_call_t*) stmt)->no_discard = true;
            break;
        case EXPR_CALL_METHOD:
            ((ast_expr_call_method_t*) stmt)->no_discard = true;
            break;
        }
    }

    return SUCCESS; // '}' was reached
}

errorcode_t parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, bool is_tentative){
    // <function_name>( <arguments> )
    //                  ^
    
    // Rewind '*ctx->i' to beginning of call
    *ctx->i = *ctx->i - 2 - (int) is_tentative;

    // Parse the head tokens as a call expression with tentativeness allowed
    ast_expr_t *out_expr;
    errorcode_t errorcode = parse_expr_call(ctx, &out_expr, true);
    
    // Allow modifiers after initial call expression
    if(errorcode == SUCCESS){
        errorcode = parse_expr_post(ctx, &out_expr);

        if(errorcode != SUCCESS){
            ast_expr_free_fully(out_expr);
        }
        
        if(out_expr->id != EXPR_CALL && out_expr->id != EXPR_CALL_METHOD){
            compiler_panicf(ctx->compiler, out_expr->source, "Expression is not a statement");
            ast_expr_free_fully(out_expr);
            errorcode = FAILURE;
        }

        // If everything is successful, add the expression as a statement
        if(errorcode == SUCCESS){
            ast_expr_list_append_unchecked(stmt_list, out_expr);
        }
    }

    return errorcode;
}

errorcode_t parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // Expects from 'ctx': compiler, object, tokenlist, i, func

    // <variable_name> <variable_type> [ = expression ]
    //       ^

    weak_cstr_t *names = NULL;
    source_t *source_list = NULL;
    length_t length = 0;
    length_t capacity = 0;
    trait_t traits = TRAIT_NONE;

    // Handle variable modifiers
    if(parse_eat(ctx, TOKEN_CONST, NULL) == SUCCESS){
        // 'const' keyword
        traits |= AST_EXPR_DECLARATION_CONST;
    } else if(parse_eat(ctx, TOKEN_STATIC, NULL) == SUCCESS){
        // 'static' keyword
        traits |= AST_EXPR_DECLARATION_STATIC;
    }

    // Collect variable names and sources
    do {
        coexpand((void**) &names, sizeof(const char*), (void**) &source_list,
            sizeof(source_t), length, &capacity, 1, 4);

        if(parse_ctx_peek(ctx) != TOKEN_WORD){
            compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), "Expected variable name");
            goto failure;
        }

        names[length] = (char*) parse_ctx_peek_data(ctx);
        source_list[length++] = parse_ctx_peek_source(ctx);
        *ctx->i += 1;
    } while(parse_eat(ctx, TOKEN_NEXT, NULL) == SUCCESS);

    // Determine whether variable is POD
    if(parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS){
        traits |= AST_EXPR_DECLARATION_POD;
    }

    // Parse the type for the variable(s) and handle any initial assignment
    ast_type_t type;

    if(parse_type(ctx, &type)
    || parse_stmt_mid_declare(ctx, stmt_list, type, names, source_list, length, traits)){
        goto failure;
    }

    free(names);
    free(source_list);
    return SUCCESS;

failure:
    free(names);
    free(source_list);
    return FAILURE;
}

errorcode_t parse_stmt_mid_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_type_t master_type, weak_cstr_t *names, source_t *source_list, length_t length, trait_t traits){
    // DANGEROUS: Don't use this function unless you know what you're doing
    // NOTE: Ownership of 'master_type' is taken
    // NOTE: 'length' is used for the length of both 'names' and 'sources'
    // NOTE: No ownership is taken of the arrays 'names' and 'sources' themselves

    unsigned int declare_stmt_type = EXPR_DECLARE;
    ast_expr_t *initial_value = NULL;
    optional_ast_expr_list_t inputs = {0};

    // Handle initial value assignment
    if(parse_eat(ctx, TOKEN_ASSIGN, NULL) == SUCCESS){
        if(parse_eat(ctx, TOKEN_UNDEF, NULL) == SUCCESS){
            // Handle ' = undef'
            declare_stmt_type = EXPR_DECLAREUNDEF;
        } else {
            // Handle ' = value'

            if(parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS){
                traits |= AST_EXPR_DECLARATION_ASSIGN_POD;
            }

            if(parse_expr(ctx, &initial_value)){
                goto failure;
            }
        }
    } else if(parse_eat(ctx, TOKEN_OPEN, NULL) == SUCCESS){
        if(parse_expr_arguments(ctx, &inputs.value.statements, &inputs.value.length, &inputs.value.capacity)){
            return FAILURE;
        }

        inputs.has = true;
    }

    // Create variable declarations
    for(length_t i = 0; i != length; i++){
        bool is_last = (i + 1 == length);
        ast_type_t type = is_last ? master_type : ast_type_clone(&master_type);
        ast_expr_t *value = is_last ? initial_value : ast_expr_clone_if_not_null(initial_value);

        ast_expr_list_append(stmt_list, ast_expr_create_declaration(declare_stmt_type, source_list[i], names[i], type, traits, value, inputs));
    }

    return SUCCESS;

failure:
    ast_type_free(&master_type);
    return FAILURE;
}

errorcode_t parse_switch(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *parent_defer_scope, bool is_exhaustive){
    // switch <condition> { ... }
    //    ^

    length_t beginning_index = *ctx->i - (is_exhaustive ? 1 : 0);
    source_t source = ctx->tokenlist->sources[beginning_index];

    if(parse_eat(ctx, TOKEN_SWITCH, "Expected 'switch' keyword after 'exhaustive' keyword")) return FAILURE;

    ast_expr_t *value = NULL;
    ast_case_list_t cases = {0};
    ast_expr_list_t or_default = {0};
    ast_expr_list_t *list = &or_default;
    defer_scope_t current_defer_scope = defer_scope_create(parent_defer_scope, NULL, FALLTHROUGHABLE);

    if( parse_expr(ctx, &value)
     || parse_ignore_newlines(ctx, "Expected '{' after value given to 'switch' statement")
     || parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after value given to 'switch' statement")
    ){
        goto failure;
    }

    while(true){
        if(parse_ignore_newlines(ctx, "Expected '}' before end of file")) goto failure;

        switch(parse_ctx_peek(ctx)){
        case TOKEN_END:
            goto no_more_statements;
        case TOKEN_CASE: {
                ast_expr_t *condition;
                source_t case_source = parse_ctx_peek_source(ctx);
                
                if( parse_eat(ctx, TOKEN_CASE, "Expected 'case' keyword for switch case")
                 || parse_expr(ctx, &condition)
                 || parse_eat(ctx, TOKEN_NEXT, NULL) == ALT_FAILURE // Skip over ',' if present
                ){
                    goto failure;
                }

                defer_scope_fulfill(&current_defer_scope, list);
                defer_scope_free(&current_defer_scope);
                current_defer_scope = defer_scope_create(parent_defer_scope, NULL, FALLTHROUGHABLE);

                ast_case_t *newest_case = ast_case_list_append(&cases, (ast_case_t){
                    .condition = condition,
                    .source = case_source,
                    .statements = {0},
                });

                list = &newest_case->statements;
            }
            break;
        case TOKEN_DEFAULT:
            defer_scope_fulfill(&current_defer_scope, list);
            defer_scope_free(&current_defer_scope);
            current_defer_scope = defer_scope_create(parent_defer_scope, NULL, TRAIT_NONE);

            list = &or_default;
            is_exhaustive = false; // Disable exhaustive checking if default case is specified

            if(parse_eat(ctx, TOKEN_DEFAULT, "Expected 'default' keyword for switch default case")){
                goto failure;
            }

            break;
        default:
            if(parse_stmts(ctx, list, &current_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_PARENT_DEFER_SCOPE)){
                goto failure;
            }
        }
    }

no_more_statements:

    // Skip over '}'
    *ctx->i += 1;

    defer_scope_fulfill(&current_defer_scope, list);
    defer_scope_free(&current_defer_scope);

    ast_expr_list_append_unchecked(stmt_list, ast_expr_create_switch(source, value, cases, or_default, is_exhaustive));
    return SUCCESS;

failure:
    ast_expr_free_fully(value);
    ast_case_list_free(&cases);
    ast_expr_list_free(&or_default);
    defer_scope_free(&current_defer_scope);
    return FAILURE;
}

errorcode_t parse_conditionless_block(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope){
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    ast_expr_list_t block_stmt_list = ast_expr_list_create(4);
    defer_scope_t block_defer_scope = defer_scope_create(defer_scope, NULL, TRAIT_NONE);

    if( parse_stmts(ctx, &block_stmt_list, &block_defer_scope, PARSE_STMTS_STANDARD)
     || parse_eat(ctx, TOKEN_END, "Expected '}' to close condition-less block")
    ){
        ast_expr_list_free(&block_stmt_list);
        defer_scope_free(&block_defer_scope);
        return FAILURE;
    }

    defer_scope_free(&block_defer_scope);

    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) malloc_init(ast_expr_conditionless_block_t, {
        .id = EXPR_CONDITIONLESS_BLOCK,
        .source = source,
        .statements = block_stmt_list,
    }));

    return SUCCESS;
}

errorcode_t parse_assert(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    ast_expr_t *assertion = NULL, *message = NULL;

    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    if(parse_expr(ctx, &assertion)){
        goto failure;
    }

    if(parse_eat(ctx, TOKEN_NEXT, NULL) == SUCCESS && parse_expr(ctx, &message)){
        goto failure;
    }

    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) malloc_init(ast_expr_assert_t, {
        .id = EXPR_ASSERT,
        .source = source,
        .assertion = assertion,
        .message = message,
    }));

    return SUCCESS;

failure:
    ast_expr_free_fully(assertion);
    ast_expr_free_fully(message);
    return FAILURE;
}

errorcode_t parse_onetime_conditional(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope){
    token_t *tokens = ctx->tokenlist->tokens;
    length_t *i = ctx->i;

    unsigned int conditional_type = tokens[*i].id;
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    ast_expr_t *condition;
    trait_t stmts_mode;

    if(parse_expr(ctx, &condition)) return FAILURE;

    if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
        ast_expr_free_fully(condition);
        return FAILURE;
    }

    if(parse_block_beginning(ctx, "conditional", &stmts_mode)){
        ast_expr_free_fully(condition);
        return FAILURE;
    }

    ast_expr_list_t if_stmt_list = ast_expr_list_create(4);
    defer_scope_t if_defer_scope = defer_scope_create(defer_scope, NULL, TRAIT_NONE);

    if(parse_stmts(ctx, &if_stmt_list, &if_defer_scope, stmts_mode)){
        ast_expr_list_free(&if_stmt_list);
        ast_expr_free_fully(condition);
        defer_scope_free(&if_defer_scope);
        return FAILURE;
    }

    defer_scope_free(&if_defer_scope);

    if(!(stmts_mode & PARSE_STMTS_SINGLE)){
        *i += 1;
    }

    // Read ahead of newlines to check for 'else'
    length_t i_readahead = *i;
    while(tokens[i_readahead].id == TOKEN_NEWLINE && i_readahead != ctx->tokenlist->length){
        i_readahead++;
    }

    if(tokens[i_readahead].id == TOKEN_ELSE){
        *i = i_readahead;

        switch(tokens[++(*i)].id){
        case TOKEN_NEXT:
            stmts_mode = PARSE_STMTS_SINGLE;
            *i += 1;
            break;
        case TOKEN_BEGIN:
            stmts_mode = PARSE_STMTS_STANDARD;
            *i += 1;
            break;
        default:
            stmts_mode = PARSE_STMTS_SINGLE;
        }

        ast_expr_list_t else_stmt_list = ast_expr_list_create(4);
        defer_scope_t else_defer_scope = defer_scope_create(defer_scope, NULL, TRAIT_NONE);

        if(parse_stmts(ctx, &else_stmt_list, &else_defer_scope, stmts_mode)){
            ast_expr_list_free(&else_stmt_list);
            ast_expr_free_fully(condition);
            defer_scope_free(&else_defer_scope);
            return FAILURE;
        }

        defer_scope_free(&else_defer_scope);

        if(stmts_mode & PARSE_STMTS_SINGLE){
            *i -= 1;
        } else {
            *i += 1;
        }

        ast_expr_ifelse_t *stmt = malloc(sizeof(ast_expr_ifelse_t));
        stmt->id = (conditional_type == TOKEN_UNLESS) ? EXPR_UNLESSELSE : EXPR_IFELSE;
        stmt->source = source;
        stmt->label = NULL;
        stmt->value = condition;
        stmt->statements = if_stmt_list;
        stmt->else_statements = else_stmt_list;
        ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
    } else {
        if(stmts_mode & PARSE_STMTS_SINGLE){
            *i -= 1;
        }

        ast_expr_list_append_unchecked(
            stmt_list,
            ast_expr_create_simple_conditional(source, (conditional_type == TOKEN_UNLESS) ? EXPR_UNLESS : EXPR_IF, NULL, condition, if_stmt_list)
        );
    }

    return SUCCESS;
}

errorcode_t parse_ambiguous_open_bracket(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // This function is used to disambiguate between the two following syntaxes:
    // variable[value] ... 
    // variable [value] Type

    // And then injects the values along the way into the proper context
    // Must also handle cases like:
    // variable[value][value] ...
    // variable [value] [value] Type

    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    length_t *i = ctx->i;
    source_t source = sources[*i];

    // word [
    //   ^

    // Get starting word
    maybe_null_weak_cstr_t word = parse_eat_word(ctx, "INTERNAL ERROR: parse_ambiguous_open_bracket() expected word");
    if(word == NULL) return FAILURE;

    // Setup temporary heap arrays
    source_t *expr_source_list = malloc(sizeof(source_t) * 4);
    length_t expr_source_list_length = 0;
    length_t expr_source_list_capacity = 4;

    ast_expr_list_t expr_list = ast_expr_list_create(4);

    // Parse ahead
    while(tokens[*i].id == TOKEN_BRACKET_OPEN){
        expand((void**) &expr_source_list, sizeof(source_t), expr_source_list_length, &expr_source_list_capacity, 1, 4);

        expr_source_list[expr_source_list_length++] = sources[*i];
        (*i)++;

        ast_expr_t *sub_expr;
        if(parse_expr(ctx, &sub_expr)) {
            ast_expr_list_free(&expr_list);
            free(expr_source_list);
            free(word);
            return FAILURE;
        }

        ast_expr_list_append(&expr_list, sub_expr);

        // Eat ']'
        if(parse_eat(ctx, TOKEN_BRACKET_CLOSE, "Expected ']'")){
            ast_expr_list_free(&expr_list);
            free(expr_source_list);
            free(word);
            return FAILURE;
        }
    }

    // Determine if this statement should be treated as a declaration,
    // or as a mutable value operation
    tokenid_t id = tokens[*i].id;
    bool is_declaration = parse_can_type_start_with(id, false) || id == TOKEN_NEXT;

    if(is_declaration){
        // If there were any already known traits for the declaration,
        // we would not be going down this execution path
        trait_t traits = TRAIT_NONE;

        // Parse rest of type
        ast_type_t type;
        if(parse_type(ctx, &type)){
            ast_expr_list_free(&expr_list);
            free(expr_source_list);
            free(word);
            return FAILURE;
        }

        // Manually prepend fixed array elements that we had already encountered
        // DANGEROUS: Manually setting AST type elements
        ast_elem_t **new_elements = malloc(sizeof(ast_elem_t*) * (type.elements_length + expr_list.length));
        memcpy(&new_elements[expr_list.length], type.elements, sizeof(ast_elem_t*) * type.elements_length);

        for(length_t i = 0; i != expr_list.length; i++){
            ast_elem_var_fixed_array_t **element = (ast_elem_var_fixed_array_t**) &new_elements[i];
            *element = malloc(sizeof(ast_elem_var_fixed_array_t));
            (*element)->id = AST_ELEM_VAR_FIXED_ARRAY;
            (*element)->source = expr_source_list[i];
            (*element)->length = expr_list.statements[i];
        }

        free(type.elements);
        type.elements = new_elements;
        type.elements_length += expr_list.length;

        // Source list no longer needed
        free(expr_source_list);        

        // Free temporary array after we're done holding the expressions
        free(expr_list.statements);

        return parse_stmt_mid_declare(ctx, stmt_list, type, &word, &source, 1, traits);
    } else {
        // Turn encountered values into AST expression
        ast_expr_t *mutable_expr = ast_expr_create_variable(word, source);

        for(length_t i = 0; i != expr_list.length; i++){
            ast_expr_t *index_expr = expr_list.statements[i];
            mutable_expr = ast_expr_create_access(mutable_expr, index_expr, expr_source_list[i]);
        }

        // Source list no longer needed
        free(expr_source_list);        

        // Free temporary array after we're done holding the expressions
        free(expr_list.statements);

        if(parse_expr_post(ctx, &mutable_expr) || parse_op_expr(ctx, 0, &mutable_expr, true)){
            ast_expr_free_fully(mutable_expr);
            return FAILURE;
        }
        if(parse_mid_mutable_expr_operation(ctx, stmt_list, mutable_expr, source)) return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_mutable_expr_operation(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    ast_expr_t *mutable_expr;
    source_t source = parse_ctx_peek_source(ctx);
    return parse_expr(ctx, &mutable_expr) || parse_mid_mutable_expr_operation(ctx, stmt_list, mutable_expr, source);
}

errorcode_t parse_mid_mutable_expr_operation(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_expr_t *mutable_expr, source_t source){
    // Parses a statement that begins with a mutable expression
    // e.g.  variable = value     or    my_array[index].doSomething()
    // NOTE: Assumes 'stmt_list' has enough space for another statement
    // NOTE: expand() should've already been used on stmt_list to make room
    // NOTE: Takes ownership of 'mutable_expr' and will free it in the case of failure

    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    length_t *i = ctx->i;
    
    // For some expressions, bypass and treat as statement
    switch(mutable_expr->id){
    case EXPR_CALL_METHOD:
    case EXPR_POSTINCREMENT:
    case EXPR_POSTDECREMENT:
    case EXPR_PREINCREMENT:
    case EXPR_PREDECREMENT:
    case EXPR_TOGGLE:
        ast_expr_list_append_unchecked(stmt_list, mutable_expr);
        return SUCCESS;
    }

    // NOTE: This is not the only place which assignment operators are handled
    unsigned int id = tokens[(*i)++].id;

    // Otherwise, it must be some type of assignment
    switch(id){
    case TOKEN_ASSIGN:
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
        break;
    default:
        compiler_panic(ctx->compiler, sources[(*i) - 1], "Expected assignment operator after expression");
        ast_expr_free_fully(mutable_expr);
        return FAILURE;
    }

    bool is_pod = (parse_eat(ctx, TOKEN_POD, NULL) == SUCCESS);

    if(!expr_is_mutable(mutable_expr)){
        compiler_panic(ctx->compiler, sources[*i], "Can't modify expression because it is immutable");
        ast_expr_free_fully(mutable_expr);
        return FAILURE;
    }

    ast_expr_t *value_expression;
    if(parse_expr(ctx, &value_expression)){
        ast_expr_free_fully(mutable_expr);
        return FAILURE;
    }

    unsigned int stmt_id;
    switch(id){
    case TOKEN_ASSIGN:                stmt_id = EXPR_ASSIGN;              break;
    case TOKEN_ADD_ASSIGN:            stmt_id = EXPR_ADD_ASSIGN;          break;
    case TOKEN_SUBTRACT_ASSIGN:       stmt_id = EXPR_SUBTRACT_ASSIGN;     break;
    case TOKEN_MULTIPLY_ASSIGN:       stmt_id = EXPR_MULTIPLY_ASSIGN;     break;
    case TOKEN_DIVIDE_ASSIGN:         stmt_id = EXPR_DIVIDE_ASSIGN;       break;
    case TOKEN_MODULUS_ASSIGN:        stmt_id = EXPR_MODULUS_ASSIGN;      break;
    case TOKEN_BIT_AND_ASSIGN:        stmt_id = EXPR_AND_ASSIGN;          break;
    case TOKEN_BIT_OR_ASSIGN:         stmt_id = EXPR_OR_ASSIGN;           break;
    case TOKEN_BIT_XOR_ASSIGN:        stmt_id = EXPR_XOR_ASSIGN;          break;
    case TOKEN_BIT_LSHIFT_ASSIGN:     stmt_id = EXPR_LSHIFT_ASSIGN;       break;
    case TOKEN_BIT_RSHIFT_ASSIGN:     stmt_id = EXPR_RSHIFT_ASSIGN;       break;
    case TOKEN_BIT_LGC_LSHIFT_ASSIGN: stmt_id = EXPR_LGC_LSHIFT_ASSIGN;   break;
    case TOKEN_BIT_LGC_RSHIFT_ASSIGN: stmt_id = EXPR_LGC_RSHIFT_ASSIGN;   break;
    default:
        compiler_panic(ctx->compiler, sources[*i], "INTERNAL ERROR: parse_stmts() came across unknown assignment operator");
        ast_expr_free_fully(mutable_expr);
        ast_expr_free_fully(value_expression);
        return FAILURE;
    }

    ast_expr_list_append_unchecked(stmt_list, ast_expr_create_assignment(stmt_id, source, mutable_expr, value_expression, is_pod));
    return SUCCESS;
}

errorcode_t parse_llvm_asm(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // llvm_asm dialect { ... } "constraints" (values)
    //    ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t source = ctx->tokenlist->sources[*i];

    // Eat 'llvm_asm'
    if(parse_eat(ctx, TOKEN_LLVM_ASM, "Expected 'llvm_asm' keyword for inline LLVM assembly code")) return FAILURE;

    maybe_null_weak_cstr_t maybe_dialect = parse_eat_word(ctx, NULL);
    troolean is_intel = TROOLEAN_UNKNOWN;

    if(maybe_dialect == NULL){
        compiler_panicf(ctx->compiler, source, "Expected either intel or att after llvm_asm keyword");
        return FAILURE;
    } else if(streq(maybe_dialect, "intel")){
        is_intel = TROOLEAN_TRUE;
    } else if(streq(maybe_dialect, "att")){
        is_intel = TROOLEAN_FALSE;
    } else {
        compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Expected either intel or att for LLVM inline assembly dialect");
        return FAILURE;
    }

    bool has_side_effects = false;
    bool is_stack_align = false;

    maybe_null_weak_cstr_t additional_info;
    
    while((additional_info = parse_eat_word(ctx, NULL))){
        if(streq(additional_info, "side_effects")){
            has_side_effects = true;
        } else if(streq(additional_info, "stack_align")){
            is_stack_align = true;
        } else {
            compiler_panicf(ctx->compiler, parse_ctx_peek_source(ctx), "Unrecognized assembly trait '%s', valid traits are: 'side_effects', 'stack_align'", additional_info);
            return FAILURE;
        }
    }

    string_builder_t builder;
    string_builder_init(&builder);

    if(parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after llvm_asm dialect")) return FAILURE;

    while(tokens[*i].id != TOKEN_END){
        switch(tokens[*i].id){
        case TOKEN_STRING: {
                token_string_data_t *string_data = (token_string_data_t*) tokens[*i].data;
                string_builder_append_view(&builder, string_data->array, string_data->length);
                string_builder_append_char(&builder, '\n');
            }
            break;
        case TOKEN_CSTRING: {
                string_builder_append(&builder, (const char*) tokens[*i].data);
                string_builder_append_char(&builder, '\n');
            }
            break;
        case TOKEN_NEXT:
        case TOKEN_NEWLINE:
            break;
        default:
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Expected string or ',' while inside { ... } for inline LLVM assembly");
            string_builder_abandon(&builder);
            return FAILURE;
        }
        
        if(++(*i) == ctx->tokenlist->length){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Expected '}' for inline LLVM assembly before end-of-file");
            string_builder_abandon(&builder);
            return FAILURE;
        }
    }

    ast_expr_list_t args = {0};
    strong_cstr_t assembly = string_builder_finalize(&builder);
    maybe_null_weak_cstr_t constraints = parse_grab_string(ctx, "Expected constraints string after '}' for llvm_asm");


    if(constraints == NULL){
        free(assembly);
        return FAILURE;
    }

    // Move past constraints string
    *i += 1;

    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' for beginning of LLVM assembly arguments")){
        free(assembly);
        return FAILURE;
    }


    while(tokens[*i].id != TOKEN_CLOSE){
        ast_expr_t *arg;

        if( parse_ignore_newlines(ctx, "Expected argument to LLVM assembly")
         || parse_expr(ctx, &arg)
         || parse_ignore_newlines(ctx, "Expected ',' or ')' after argument to LLVM assembly")
        ){
            goto failure;
        }
        
        ast_expr_list_append(&args, arg);

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' or ')' after after argument to LLVM assembly");
            goto failure;
        }
    }

    // Move past closing ')'
    *i += 1;

    ast_expr_llvm_asm_t *stmt = malloc(sizeof(ast_expr_llvm_asm_t));
    stmt->id = EXPR_LLVM_ASM;
    stmt->source = source;
    stmt->assembly = strong_cstr_empty_if_null(assembly);
    stmt->constraints = constraints;
    stmt->args = args.expressions;
    stmt->arity = args.length;
    stmt->has_side_effects = has_side_effects;
    stmt->is_stack_align = is_stack_align;
    stmt->is_intel = (is_intel == TROOLEAN_TRUE);
    ast_expr_list_append_unchecked(stmt_list, (ast_expr_t*) stmt);
    return SUCCESS;

failure:
    ast_expr_list_free(&args);
    free(assembly);
    return FAILURE;
}

errorcode_t parse_local_named_expression_declaration(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, source_t source){
    // NOTE: Assumes 'stmt_list' has enough space for another statement
    // NOTE: expand() should've already been used on stmt_list to make room

    // Parse named expression
    ast_named_expression_t named_expression;
    if(parse_named_expression_definition(ctx, &named_expression)) return FAILURE;

    // Turn into declare named expression statement
    ast_expr_list_append_unchecked(stmt_list, ast_expr_create_declare_named_expression(source, named_expression));
    return SUCCESS;
}

errorcode_t parse_block_beginning(parse_ctx_t *ctx, weak_cstr_t block_readable_mother, unsigned int *out_stmts_mode){
    switch(parse_ctx_peek(ctx)){
    case TOKEN_BEGIN:
        *out_stmts_mode = PARSE_STMTS_STANDARD;
        (*ctx->i)++;
        return SUCCESS;
    case TOKEN_NEXT:
        *out_stmts_mode = PARSE_STMTS_SINGLE;
        (*ctx->i)++;
        return SUCCESS;
    case TOKEN_WORD:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
    case TOKEN_DEFER:
    case TOKEN_DELETE:
    case TOKEN_ELSE:
    case TOKEN_EXHAUSTIVE:
    case TOKEN_FALLTHROUGH:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_REPEAT:
    case TOKEN_RETURN:
    case TOKEN_SWITCH:
    case TOKEN_UNLESS:
    case TOKEN_UNTIL:
    case TOKEN_VA_ARG:
    case TOKEN_VA_END:
    case TOKEN_VA_START:
    case TOKEN_WHILE:
        // Specially allowed expression terminators
        *out_stmts_mode = PARSE_STMTS_SINGLE;
        return SUCCESS;
    default:
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i - 1], "Expected '{' or ',' after %s expression", block_readable_mother);
        return FAILURE;
    }
    return FAILURE;
}
