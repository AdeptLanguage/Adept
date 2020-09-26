
#include "UTIL/util.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_stmt.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_global.h"

void defer_scope_init(defer_scope_t *defer_scope, defer_scope_t *parent, weak_cstr_t label, trait_t traits){
    defer_scope->list.statements = NULL;
    defer_scope->list.length = 0;
    defer_scope->list.capacity = 0;
    defer_scope->parent = parent;
    defer_scope->label = label;
    defer_scope->traits = traits;
}

void defer_scope_free(defer_scope_t *defer_scope){
    ast_free_statements_fully(defer_scope->list.statements, defer_scope->list.length);
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
    defer_scope_fulfill_into(defer_scope, &stmt_list->statements, &stmt_list->length, &stmt_list->capacity);
}

void defer_scope_fulfill_into(defer_scope_t *defer_scope, ast_expr_t ***statements, length_t *length, length_t *capacity){
    expand((void**) statements, sizeof(ast_expr_t*), *length, capacity, defer_scope->list.length, defer_scope->list.length);
    for(length_t r = defer_scope->list.length; r != 0; r--){
        (*statements)[(*length)++] = defer_scope->list.statements[r - 1];
    }
    defer_scope->list.length = 0;
}

void defer_scope_rewind(defer_scope_t *defer_scope, ast_expr_list_t *stmt_list, trait_t scope_trait, weak_cstr_t label){
    defer_scope_fulfill_into(defer_scope, &stmt_list->statements, &stmt_list->length, &stmt_list->capacity);
    
    while((!(defer_scope->traits & scope_trait) || (label && defer_scope->label && strcmp(defer_scope->label, label) != 0)) && defer_scope->parent != NULL){
        defer_scope = defer_scope->parent;

        expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, defer_scope->list.length, defer_scope->list.length);
        for(length_t r = defer_scope->list.length; r != 0; r--){
            stmt_list->statements[stmt_list->length++] = ast_expr_clone(defer_scope->list.statements[r - 1]);
        }
    }
}

errorcode_t parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope, trait_t mode){
    // NOTE: Outputs statements to stmt_list
    // NOTE: Ends on 'i' pointing to a '}' token
    // NOTE: Even if this function returns 1, statements appended to stmt_list still must be freed

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;
    source_t source;

    source.index = 0;
    source.object_index = ctx->object->index;
    source.stride = 0;

    while(tokens[*i].id != TOKEN_END){
        if(parse_ignore_newlines(ctx, "Unexpected expression termination")) return FAILURE;
        expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, 1, 8);

        // Parse a statement into the statement list
        switch(tokens[*i].id){
        case TOKEN_END:
            return SUCCESS;
        case TOKEN_RETURN: {
                ast_expr_t *return_expression;
                source = sources[(*i)++]; // Pass over return keyword

                if(tokens[*i].id == TOKEN_NEWLINE) return_expression = NULL;
                else if(parse_expr(ctx, &return_expression)) return FAILURE;

                ast_expr_return_t *stmt = malloc(sizeof(ast_expr_return_t));
                stmt->id = EXPR_RETURN;
                stmt->source = source;
                stmt->value = return_expression;
                stmt->last_minute.capacity = defer_scope_total(defer_scope);
                stmt->last_minute.statements = malloc(stmt->last_minute.capacity * sizeof(ast_expr_t*));
                stmt->last_minute.length = 0;

                defer_scope_fulfill(defer_scope, &stmt->last_minute);

                // Duplicate defer statements of ancestors
                defer_scope_t *traverse = defer_scope->parent;

                while(traverse){
                    for(size_t r = traverse->list.length; r != 0; r--){
                        stmt->last_minute.statements[stmt->last_minute.length++] = ast_expr_clone(traverse->list.statements[r - 1]);
                    }
                    traverse = traverse->parent;
                }

                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_STRING: {
                ast_expr_t *expression;
                if(parse_expr(ctx, &expression)) return FAILURE;
                
                if(expression->id != EXPR_CALL_METHOD){
                    compiler_panicf(ctx->compiler, expression->source, "Expression not supported as a statement");
                    ast_expr_free(expression);
                    return FAILURE;
                }

                stmt_list->statements[stmt_list->length++] = expression;
            }
            break;
        case TOKEN_CONST:
            if(parse_local_constant_declaration(ctx, stmt_list, source)) return FAILURE;
            break;
        case TOKEN_WORD: {
                source = sources[(*i)++]; // Read ahead to see what type of statement this is

                switch(tokens[*i].id){
                case TOKEN_MAYBE:
                    if(tokens[++(*i)].id == TOKEN_OPEN){
                        (*i)++; if(parse_stmt_call(ctx, stmt_list, true)) return FAILURE;
                    } else {
                        // TODO: Have a better error message here
                        parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at beginning of statement");
                        return FAILURE;
                    }
                    break;
                case TOKEN_OPEN:
                    (*i)++; if(parse_stmt_call(ctx, stmt_list, false)) return FAILURE;
                    break;
                case TOKEN_WORD: case TOKEN_FUNC:
                case TOKEN_STDCALL: case TOKEN_NEXT: case TOKEN_POD:
                case TOKEN_GENERIC_INT: /*fixed array*/ case TOKEN_MULTIPLY: /*pointer*/
                case TOKEN_LESSTHAN: case TOKEN_BIT_LSHIFT: case TOKEN_BIT_LGC_LSHIFT: /*generics*/
                case TOKEN_POLYMORPH: /*polymorphic type*/ case TOKEN_COLON: /*experimental : type syntax*/
                    (*i)--; if(parse_stmt_declare(ctx, stmt_list)) return FAILURE;
                    break;
                default:
                    // Assume assign statement if not one of the above
                    (*i)--;
                    if(parse_assign(ctx, stmt_list)) return FAILURE;
                    break;
                }
            }
            break;
        case TOKEN_MULTIPLY: case TOKEN_OPEN: case TOKEN_INCREMENT: case TOKEN_DECREMENT:
        case TOKEN_BIT_AND: case TOKEN_BIT_OR: case TOKEN_BIT_XOR: case TOKEN_BIT_LSHIFT: case TOKEN_BIT_RSHIFT:
        case TOKEN_BIT_LGC_LSHIFT: case TOKEN_BIT_LGC_RSHIFT: /* DUPLICATE: case TOKEN_ADDRESS: */
            if(parse_assign(ctx, stmt_list)) return FAILURE;
            break;
        case TOKEN_IF: case TOKEN_UNLESS: {
                unsigned int conditional_type = tokens[*i].id;
                source = sources[(*i)++];
                ast_expr_t *conditional;
                trait_t stmts_mode;

                if(parse_expr(ctx, &conditional)) return FAILURE;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after conditional expression");
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                ast_expr_list_t if_stmt_list;
                if_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                if_stmt_list.length = 0;
                if_stmt_list.capacity = 4;

                defer_scope_t if_defer_scope;
                defer_scope_init(&if_defer_scope, defer_scope, NULL, TRAIT_NONE);

                if(parse_stmts(ctx, &if_stmt_list, &if_defer_scope, stmts_mode)){
                    ast_free_statements_fully(if_stmt_list.statements, if_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    defer_scope_free(&if_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&if_defer_scope);

                if(!(stmts_mode & PARSE_STMTS_SINGLE)) (*i)++;

                // Read ahead of newlines to check for 'else'
                length_t i_readahead = *i;
                while(tokens[i_readahead].id == TOKEN_NEWLINE && i_readahead != ctx->tokenlist->length) i_readahead++;

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

                    // Reuse 'if_defer_scope' for else defer scope
                    defer_scope_init(&if_defer_scope, defer_scope, NULL, TRAIT_NONE);

                    if(parse_stmts(ctx, &else_stmt_list, &if_defer_scope, stmts_mode)){
                        ast_free_statements_fully(else_stmt_list.statements, else_stmt_list.length);
                        ast_expr_free_fully(conditional);
                        defer_scope_free(&if_defer_scope);
                        return FAILURE;
                    }

                    defer_scope_free(&if_defer_scope);

                    if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--; else (*i)++;

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
                    if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--;
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
                maybe_null_weak_cstr_t label = NULL;

                if(tokens[*i].id == TOKEN_BREAK || tokens[*i].id == TOKEN_CONTINUE){
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
                        label = tokens[*i].data; *i += 2;
                    }

                    if(parse_expr(ctx, &conditional)) return FAILURE;
                }

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after conditional expression");
                    ast_expr_free_fully(conditional);
                    return FAILURE;
                }

                ast_expr_list_t while_stmt_list;
                while_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                while_stmt_list.length = 0;
                while_stmt_list.capacity = 4;

                defer_scope_t while_defer_scope;
                defer_scope_init(&while_defer_scope, defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &while_stmt_list, &while_defer_scope, stmts_mode)){
                    ast_free_statements_fully(while_stmt_list.statements, while_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    defer_scope_free(&while_defer_scope);
                    return FAILURE;
                }
                
                defer_scope_free(&while_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--; else (*i)++;

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

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after 'each in' expression");
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(list_expr);
                    free(it_name);
                    return FAILURE;
                }

                ast_expr_list_t each_in_stmt_list;
                each_in_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                each_in_stmt_list.length = 0;
                each_in_stmt_list.capacity = 4;

                defer_scope_t each_in_defer_scope;
                defer_scope_init(&each_in_defer_scope, defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &each_in_stmt_list, &each_in_defer_scope, stmts_mode)){
                    ast_free_statements_fully(each_in_stmt_list.statements, each_in_stmt_list.length);
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    ast_expr_free_fully(list_expr);
                    free(it_name);
                    defer_scope_free(&each_in_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&each_in_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--; else (*i)++;

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
                stmt->statements = each_in_stmt_list.statements;
                stmt->statements_length = each_in_stmt_list.length;
                stmt->statements_capacity = each_in_stmt_list.capacity;
                stmt->is_static = is_static;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_REPEAT: {
                source = sources[(*i)++];
                ast_expr_t *limit = NULL;
                trait_t stmts_mode;
                maybe_null_weak_cstr_t label = NULL;

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data; *i += 2;
                }

                bool is_static = tokens[*i].id == TOKEN_STATIC;
                if(is_static) *i += 1;
                
                if(parse_expr(ctx, &limit)) return FAILURE;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after 'repeat' expression");
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                ast_expr_list_t repeat_stmt_list;
                repeat_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                repeat_stmt_list.length = 0;
                repeat_stmt_list.capacity = 4;

                defer_scope_t repeat_defer_scope;
                defer_scope_init(&repeat_defer_scope, defer_scope, label, BREAKABLE | CONTINUABLE);

                if(parse_stmts(ctx, &repeat_stmt_list, &repeat_defer_scope, stmts_mode)){
                    ast_free_statements_fully(repeat_stmt_list.statements, repeat_stmt_list.length);
                    ast_expr_free_fully(limit);
                    defer_scope_free(&repeat_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&repeat_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--; else (*i)++;

                ast_expr_repeat_t *stmt = malloc(sizeof(ast_expr_repeat_t));
                stmt->id = EXPR_REPEAT;
                stmt->source = source;
                stmt->label = label;
                stmt->limit = limit;
                stmt->statements = repeat_stmt_list.statements;
                stmt->statements_length = repeat_stmt_list.length;
                stmt->statements_capacity = repeat_stmt_list.capacity;
                stmt->is_static = is_static;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_DEFER: {
                defer_scope_t defer_defer_scope;
                defer_scope_init(&defer_defer_scope, defer_scope, NULL, TRAIT_NONE);

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
                ast_expr_unary_t *stmt = malloc(sizeof(ast_expr_unary_t));
                stmt->id = EXPR_DELETE;
                stmt->source = sources[(*i)++];

                if(parse_primary_expr(ctx, &stmt->value) != 0){
                    free(stmt);
                    return FAILURE;
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

                    defer_scope_rewind(defer_scope, stmt_list, BREAKABLE, stmt->label);
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    ast_expr_break_t *stmt = malloc(sizeof(ast_expr_break_t));
                    stmt->id = EXPR_BREAK;
                    stmt->source = sources[*i - 1];

                    defer_scope_rewind(defer_scope, stmt_list, BREAKABLE, NULL);
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

                    defer_scope_rewind(defer_scope, stmt_list, CONTINUABLE, stmt->label);
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                } else {
                    ast_expr_continue_t *stmt = malloc(sizeof(ast_expr_continue_t));
                    stmt->id = EXPR_CONTINUE;
                    stmt->source = sources[*i - 1];

                    defer_scope_rewind(defer_scope, stmt_list, CONTINUABLE, NULL);
                    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                }
            }
            break;
        case TOKEN_FALLTHROUGH: {
                ast_expr_fallthrough_t *stmt = malloc(sizeof(ast_expr_fallthrough_t));
                stmt->id = EXPR_FALLTHROUGH;
                stmt->source = sources[(*i)++];

                defer_scope_rewind(defer_scope, stmt_list, FALLTHROUGHABLE, NULL);
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_META:
            if(parse_meta(ctx)) return FAILURE;
            break;
        case TOKEN_EXHAUSTIVE:
            if(ctx->tokenlist->tokens[++(*i)].id != TOKEN_SWITCH){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected 'switch' keyword after 'exhaustive' keyword");
                return FAILURE;
            }
            
            if(parse_switch(ctx, stmt_list, defer_scope, true)) return FAILURE;
            break;
        case TOKEN_SWITCH:
            if(parse_switch(ctx, stmt_list, defer_scope, false)) return FAILURE;
            break;
        case TOKEN_VA_START: case TOKEN_VA_END: {
                tokenid_t tokenid = tokens[*i].id;
                source = sources[(*i)++];

                ast_expr_t *va_list_value;
                if(parse_expr(ctx, &va_list_value)) return FAILURE;

                ast_expr_unary_t *stmt = malloc(sizeof(ast_expr_unary_t));
                stmt->id = tokenid == TOKEN_VA_START ? EXPR_VA_START : EXPR_VA_END;
                stmt->source = source;
                stmt->value = va_list_value;

                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_VA_COPY: {
                // va_copy(dest, src)
                //    ^

                source = sources[(*i)++];

                // Eat '('
                if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after va_copy keyword")) return FAILURE;

                ast_expr_t *va_list_destination;
                if(parse_expr(ctx, &va_list_destination)) return FAILURE;

                // Eat ','
                if(parse_eat(ctx, TOKEN_NEXT, "Expected ',' after first parameter to va_arg")){
                    ast_expr_free_fully(va_list_destination);
                    return FAILURE;
                }
                
                ast_expr_t *va_list_source;
                if(parse_expr(ctx, &va_list_source)){
                    ast_expr_free_fully(va_list_destination);
                    ast_expr_free_fully(va_list_source);
                    return FAILURE;
                }

                // Eat ')'
                if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after va_arg parameters")){
                    ast_expr_free_fully(va_list_destination);
                    ast_expr_free_fully(va_list_source);
                    return FAILURE;
                }

                ast_expr_va_copy_t *stmt = malloc(sizeof(ast_expr_va_copy_t));
                stmt->id = EXPR_VA_COPY;
                stmt->source = source;
                stmt->dest_value = va_list_destination;
                stmt->src_value = va_list_source;

                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_FOR: {
                // for(stmt; condition; stmt) {...}    for stmt; condition; stmt {...}
                //  ^                                   ^

                ast_expr_t *condition = NULL;
                ast_expr_list_t before, statements;
                weak_cstr_t label = NULL;
                source = sources[*i];
                
                memset(&before, 0, sizeof(ast_expr_list_t));
                memset(&statements, 0, sizeof(ast_expr_list_t));

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data; *i += 2;
                }

                defer_scope_t for_defer_scope;
                defer_scope_init(&for_defer_scope, defer_scope, label, BREAKABLE | CONTINUABLE);

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
                    if(parse_stmts(ctx, &for_defer_scope.list, &for_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_NO_JOINING | PARSE_STMTS_PARENT_DEFER_SCOPE)){
                        if(condition) ast_expr_free(condition);
                        ast_exprs_free_fully(before.statements, before.length);
                        defer_scope_free(&for_defer_scope);
                        return FAILURE;
                    }

                    // TODO: Reverse statements given in 'after' statements for 'defer'
                    // NOTE: This isn't a problem right now, since there can only be one statements anyway
                }

                // Eat ')' if it exists
                if(tokens[*i].id == TOKEN_CLOSE) (*i)++;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
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
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }
                
                // Parse statements
                if(parse_stmts(ctx, &statements, &for_defer_scope, stmts_mode)){
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }

                defer_scope_free(&for_defer_scope);

                if(stmts_mode & PARSE_STMTS_SINGLE) (*i)--; else (*i)++;

                ast_expr_for_t *stmt = malloc(sizeof(ast_expr_for_t));
                stmt->id = EXPR_FOR;
                stmt->source = source;
                stmt->label = label;
                stmt->before = before;
                stmt->condition = condition;
                stmt->statements = statements;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at beginning of statement");
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_TERMINATE_JOIN && !(mode & PARSE_STMTS_NO_JOINING)){
            // Bypass single statement flag by "joining" 2+ statements
            (*i)++; continue;
        }

        // Continue over newline token
        // TODO: INVESTIGATE: Investigate whether TOKEN_META #else/#elif should be having (*i)++ here
        if(tokens[*i].id == TOKEN_NEWLINE || (tokens[*i].id == TOKEN_META && (strcmp(tokens[*i].data, "else") == 0 || strcmp(tokens[*i].data, "elif") == 0))){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_ELSE && tokens[*i].id != TOKEN_TERMINATE_JOIN && tokens[*i].id != TOKEN_CLOSE && tokens[*i].id != TOKEN_BEGIN && tokens[*i].id != TOKEN_NEXT){
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at end of statement");
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

    return SUCCESS; // '}' was reached
}

errorcode_t parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, bool is_tentative){
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

    // DANGEROUS: Using is_tentative to determine location of word
    length_t name_index = *i - 2 - (int) is_tentative;

    stmt->source = sources[name_index];
    stmt->name = (char*) tokens[name_index].data;
    tokens[name_index].data = NULL;
    stmt->arity = 0;
    stmt->args = NULL;
    stmt->is_tentative = is_tentative;

    if(parse_relocate_name(ctx, stmt->source, &stmt->name)){
        ctx->ignore_newlines_in_expr_depth--;
        free(stmt->name);
        free(stmt);
        return FAILURE;
    }

    // Ignore newline termination within children expressions
    ctx->ignore_newlines_in_expr_depth++;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected function argument") || parse_expr(ctx, &arg_expr)){
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt->name);
            free(stmt);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &stmt->args, sizeof(ast_expr_t*), stmt->arity, &args_capacity, 1, 4);
        stmt->args[stmt->arity++] = arg_expr;

        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")){
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt->name);
            free(stmt);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
            ctx->ignore_newlines_in_expr_depth--;
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt->name);
            free(stmt);
            return FAILURE;
        }
    }

    ctx->ignore_newlines_in_expr_depth--;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    (*i)++;
    return SUCCESS;
}

errorcode_t parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // Expects from 'ctx': compiler, object, tokenlist, i, func

    // <variable_name> <variable_type> [ = expression ]
    //       ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    weak_cstr_t *decl_names = NULL;
    source_t *decl_sources = NULL;
    length_t length = 0;
    length_t capacity = 0;

    bool is_pod = false;
    bool is_assign_pod = false;
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
            return FAILURE;
        }

        decl_names[length] = tokens[*i].data;
        decl_sources[length++] = sources[(*i)++];

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++; continue;
        } else break;
    }

    if(tokens[*i].id == TOKEN_POD){
        is_pod = true;
        (*i)++;
    }

    // Parse the type for the variable(s)
    if(parse_type(ctx, &decl_type)){
        free(decl_names);
        free(decl_sources);
        return FAILURE;
    }

    // Parse the initial value for the variable(s) (if an initial value is given)
    if(tokens[*i].id == TOKEN_ASSIGN){
        (*i)++; // Skip over 'assign' token

        // Handle either 'undef' or an expression
        if(tokens[*i].id == TOKEN_UNDEF){
            declare_stmt_type = EXPR_DECLAREUNDEF;
            (*i)++;
        } else {
            if(tokens[*i].id == TOKEN_POD){
                is_assign_pod = true;
                (*i)++;
            }

            if(parse_expr(ctx, &decl_value)){
                ast_type_free(&decl_type);
                free(decl_names);
                free(decl_sources);
                return FAILURE;
            }
        }
    }    

    // Add each variable to the necessary data sets
    for(length_t v = 0; v != length; v++){
        ast_expr_declare_t *stmt = malloc(sizeof(ast_expr_declare_t));
        stmt->id = declare_stmt_type;
        stmt->source = decl_sources[v];
        stmt->name = decl_names[v];
        stmt->is_pod = is_pod;
        stmt->is_assign_pod = is_assign_pod;

        if(v + 1 == length){
            stmt->type = decl_type;
            stmt->value = decl_value;
        } else {
            stmt->type = ast_type_clone(&decl_type);
            stmt->value = decl_value == NULL ? NULL : ast_expr_clone(decl_value);
        }

        if(v != 0){
            expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, 1, 8);
        }

        // Append the created declare statement
        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    }

    free(decl_names);
    free(decl_sources);
    return SUCCESS;
}

errorcode_t parse_switch(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *parent_defer_scope, bool is_exhaustive){
    // switch <condition> { ... }
    //    ^

    source_t source = ctx->tokenlist->sources[is_exhaustive ? *ctx->i - 1 : *ctx->i];

    if(parse_eat(ctx, TOKEN_SWITCH, "Expected 'switch' keyword when parsing switch statement"))
        return FAILURE;

    ast_expr_t *value;
    if(parse_expr(ctx, &value)) return FAILURE;

    ast_case_t *cases = NULL;
    length_t cases_length = 0, cases_capacity = 0;

    if(parse_ignore_newlines(ctx, "Expected '{' after value given to 'switch' statement")){
        ast_expr_free_fully(value);
        return FAILURE;
    }
    
    if(parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after value given to 'switch' statement")){
        ast_expr_free_fully(value);
        return FAILURE;
    }

    ast_expr_t **default_statements = NULL;
    length_t default_statements_length = 0, default_statements_capacity = 0;

    defer_scope_t current_defer_scope;
    ast_expr_t ***list = &default_statements;
    length_t *list_length = &default_statements_length;
    length_t *list_capacity = &default_statements_capacity;

    if(parse_ignore_newlines(ctx, "Expected '}' before end of file")){
        ast_expr_free_fully(value);
        return FAILURE;
    }

    unsigned int token_id = ctx->tokenlist->tokens[*ctx->i].id;
    bool failed = false;

    defer_scope_init(&current_defer_scope, parent_defer_scope, NULL, FALLTHROUGHABLE);

    while(token_id != TOKEN_END){
        if(token_id == TOKEN_CASE){
            ast_expr_t *condition;
            source_t case_source = ctx->tokenlist->sources[*ctx->i];
            failed = failed || parse_eat(ctx, TOKEN_CASE, "Expected 'case' keyword for switch case") || parse_expr(ctx, &condition);

            // Skip over ',' if present
            if(!failed && ctx->tokenlist->tokens[*ctx->i].id == TOKEN_NEXT) (*ctx->i)++;
            failed = failed || parse_ignore_newlines(ctx, "Expected '}' before end of file");

            if(!failed){
                defer_scope_fulfill_into(&current_defer_scope, list, list_length, list_capacity);
                defer_scope_free(&current_defer_scope);
                defer_scope_init(&current_defer_scope, parent_defer_scope, NULL, FALLTHROUGHABLE);

                expand((void**) &cases, sizeof(ast_case_t), cases_length, &cases_capacity, 1, 4);
                ast_case_t *expr_case = &cases[cases_length++];
                expr_case->condition = condition;
                expr_case->source = case_source;
                expr_case->statements = NULL;
                expr_case->statements_length = 0;
                expr_case->statements_capacity = 0;
                list = &expr_case->statements;
                list_length = &expr_case->statements_length;
                list_capacity = &expr_case->statements_capacity;
            }
        } else if(token_id == TOKEN_DEFAULT){
            defer_scope_fulfill_into(&current_defer_scope, list, list_length, list_capacity);
            defer_scope_free(&current_defer_scope);
            defer_scope_init(&current_defer_scope, parent_defer_scope, NULL, TRAIT_NONE);
            list = &default_statements;
            list_length = &default_statements_length;
            list_capacity = &default_statements_capacity;

            failed = failed || parse_eat(ctx, TOKEN_DEFAULT, "Expected 'default' keyword for switch default case")
                            || parse_ignore_newlines(ctx, "Expected '}' before end of file");
            
            // Disable exhaustive checking if default case is specified
            is_exhaustive = false;
        } else {
            ast_expr_list_t stmts_pass_list;
            stmts_pass_list.statements = *list;
            stmts_pass_list.length = *list_length;
            stmts_pass_list.capacity = *list_capacity;

            failed = failed || parse_stmts(ctx, &stmts_pass_list, &current_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_PARENT_DEFER_SCOPE)
                            || parse_ignore_newlines(ctx, "Expected '}' before end of file");

            // CLEANUP: Eww, but it works
            *list = stmts_pass_list.statements;
            *list_length = stmts_pass_list.length;
            *list_capacity = stmts_pass_list.capacity;
        }

        if(failed){
            ast_expr_free_fully(value);
            for(length_t c = 0; c != cases_length; c++){
                ast_case_t *expr_case = &cases[c];
                ast_expr_free_fully(expr_case->condition);
                ast_free_statements_fully(expr_case->statements, expr_case->statements_length);
            }
            free(cases);
            ast_free_statements_fully(default_statements, default_statements_length);
            defer_scope_free(&current_defer_scope);
            return FAILURE;
        }

        token_id = ctx->tokenlist->tokens[*ctx->i].id;
    }

    // Skip over '}'
    (*ctx->i)++;

    defer_scope_fulfill_into(&current_defer_scope, list, list_length, list_capacity);
    defer_scope_free(&current_defer_scope);

    ast_expr_switch_t *switch_expr = malloc(sizeof(ast_expr_switch_t));
    switch_expr->id = EXPR_SWITCH;
    switch_expr->source = source;
    switch_expr->value = value;
    switch_expr->cases = cases;
    switch_expr->cases_length = cases_length;
    switch_expr->cases_capacity = cases_capacity;
    switch_expr->default_statements = default_statements;
    switch_expr->default_statements_length = default_statements_length;
    switch_expr->default_statements_capacity = default_statements_capacity;
    switch_expr->is_exhaustive = is_exhaustive;

    // Append the created switch statement
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) switch_expr;
    return SUCCESS;
}

errorcode_t parse_assign(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // NOTE: Assumes 'stmt_list' has enough space for another statement
    // NOTE: expand() should've already been used on stmt_list to make room

    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    length_t *i = ctx->i;
    source_t source = sources[*i];

    ast_expr_t *mutable_expression;
    if(parse_expr(ctx, &mutable_expression)) return FAILURE;
    
    // For some expressions, bypass and treat as statement
    if(mutable_expression->id == EXPR_CALL_METHOD
            || mutable_expression->id == EXPR_POSTINCREMENT
            || mutable_expression->id == EXPR_POSTDECREMENT
            || mutable_expression->id == EXPR_PREINCREMENT
            || mutable_expression->id == EXPR_PREDECREMENT
            || mutable_expression->id == EXPR_TOGGLE){
        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) mutable_expression;
        return SUCCESS;
    }

    // NOTE: This is not the only place which assignment operators are handled
    unsigned int id = tokens[(*i)++].id;

    switch(id){
    case TOKEN_ASSIGN: case TOKEN_ADD_ASSIGN:
    case TOKEN_SUBTRACT_ASSIGN: case TOKEN_MULTIPLY_ASSIGN:
    case TOKEN_DIVIDE_ASSIGN: case TOKEN_MODULUS_ASSIGN:
    case TOKEN_BIT_AND_ASSIGN: case TOKEN_BIT_OR_ASSIGN: case TOKEN_BIT_XOR_ASSIGN:
    case TOKEN_BIT_LS_ASSIGN: case TOKEN_BIT_RS_ASSIGN:
    case TOKEN_BIT_LGC_LS_ASSIGN: case TOKEN_BIT_LGC_RS_ASSIGN:
        break;
    default:
        compiler_panic(ctx->compiler, sources[(*i) - 1], "Expected assignment operator after expression");
        ast_expr_free_fully(mutable_expression);
        return FAILURE;
    }

    bool is_pod = false;
    if(tokens[*i].id == TOKEN_POD){
        is_pod = true;
        (*i)++;
    }

    if(!expr_is_mutable(mutable_expression)){
        compiler_panic(ctx->compiler, sources[*i], "Can't modify expression because it is immutable");
        ast_expr_free_fully(mutable_expression);
        return FAILURE;
    }

    ast_expr_t *value_expression;
    if(parse_expr(ctx, &value_expression)){
        ast_expr_free_fully(mutable_expression);
        return FAILURE;
    }

    unsigned int stmt_id;
    switch(id){
    case TOKEN_ASSIGN:            stmt_id = EXPR_ASSIGN;          break;
    case TOKEN_ADD_ASSIGN:        stmt_id = EXPR_ADD_ASSIGN;      break;
    case TOKEN_SUBTRACT_ASSIGN:   stmt_id = EXPR_SUBTRACT_ASSIGN; break;
    case TOKEN_MULTIPLY_ASSIGN:   stmt_id = EXPR_MULTIPLY_ASSIGN; break;
    case TOKEN_DIVIDE_ASSIGN:     stmt_id = EXPR_DIVIDE_ASSIGN;   break;
    case TOKEN_MODULUS_ASSIGN:    stmt_id = EXPR_MODULUS_ASSIGN;  break;
    case TOKEN_BIT_AND_ASSIGN:    stmt_id = EXPR_AND_ASSIGN;      break;
    case TOKEN_BIT_OR_ASSIGN:     stmt_id = EXPR_OR_ASSIGN;       break;
    case TOKEN_BIT_XOR_ASSIGN:    stmt_id = EXPR_XOR_ASSIGN;      break;
    case TOKEN_BIT_LS_ASSIGN:     stmt_id = EXPR_LS_ASSIGN;       break;
    case TOKEN_BIT_RS_ASSIGN:     stmt_id = EXPR_RS_ASSIGN;       break;
    case TOKEN_BIT_LGC_LS_ASSIGN: stmt_id = EXPR_LGC_LS_ASSIGN;   break;
    case TOKEN_BIT_LGC_RS_ASSIGN: stmt_id = EXPR_LGC_RS_ASSIGN;   break;
    default:
        compiler_panic(ctx->compiler, sources[*i], "INTERNAL ERROR: parse_stmts() came across unknown assignment operator");
        ast_expr_free_fully(mutable_expression);
        ast_expr_free_fully(value_expression);
        return FAILURE;
    }

    ast_expr_assign_t *stmt = malloc(sizeof(ast_expr_assign_t));
    stmt->id = stmt_id;
    stmt->source = source;
    stmt->destination = mutable_expression;
    stmt->value = value_expression;
    stmt->is_pod = is_pod;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    return SUCCESS;
}

errorcode_t parse_local_constant_declaration(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, source_t source){
    // NOTE: Assumes 'stmt_list' has enough space for another statement
    // NOTE: expand() should've already been used on stmt_list to make room
    
    ast_constant_t constant;

    // Parse constant
    if(parse_constant_declaration(ctx, &constant)) return FAILURE;

    // Turn into declare constant statement
    ast_expr_declare_constant_t *stmt = malloc(sizeof(ast_expr_declare_constant_t));
    stmt->id = EXPR_DECLARE_CONSTANT;
    stmt->source = source;
    stmt->constant = constant;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    return SUCCESS;
}
