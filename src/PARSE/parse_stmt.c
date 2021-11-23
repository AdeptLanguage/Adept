
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

                ast_expr_list_t last_minute;
                last_minute.capacity = defer_scope_total(defer_scope);
                last_minute.statements = malloc(last_minute.capacity * sizeof(ast_expr_t*));
                last_minute.length = 0;

                defer_scope_fulfill(defer_scope, &last_minute);

                // Duplicate defer statements of ancestors
                defer_scope_t *traverse = defer_scope->parent;

                while(traverse){
                    for(size_t r = traverse->list.length; r != 0; r--){
                        last_minute.statements[last_minute.length++] = ast_expr_clone(traverse->list.statements[r - 1]);
                    }
                    traverse = traverse->parent;
                }

                ast_expr_create_return(&stmt_list->statements[stmt_list->length++], source, return_expression, last_minute);
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
        case TOKEN_DEFINE:
            if(parse_local_constant_declaration(ctx, stmt_list, sources[*i])) return FAILURE;
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
                case TOKEN_POLYCOUNT: /*polymorphic count*/
                case TOKEN_STRUCT: case TOKEN_PACKED: case TOKEN_UNION: /* anonymous composites */
                    (*i)--; if(parse_stmt_declare(ctx, stmt_list)) return FAILURE;
                    break;
                case TOKEN_BRACKET_OPEN: /* ambiguous case between declaration and usage */
                    (*i)--;
                    if(parse_ambiguous_open_bracket(ctx, stmt_list)) return FAILURE;
                    break;
                default: {
                        // Assume mutable expression operation statement if not one of the above
                        (*i)--;
                        if(parse_mutable_expr_operation(ctx, stmt_list)) return FAILURE;
                        break;
                    }
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

                if(parse_block_beginning(ctx, "conditional", &stmts_mode)){
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

                if(parse_block_beginning(ctx, "'each in'", &stmts_mode)){
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
                maybe_null_weak_cstr_t idx_overload_name = NULL;

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

                if(tokens[*i].id == TOKEN_USING){
                    idx_overload_name = parse_grab_word(ctx, "Expected name for 'idx' variable after 'using' keyword");

                    if(idx_overload_name == NULL){
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
                stmt->idx_overload_name = idx_overload_name;
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
                ast_expr_list_t before, after, statements;
                weak_cstr_t label = NULL;
                source = sources[*i];
                
                memset(&before, 0, sizeof(ast_expr_list_t));
                memset(&after, 0, sizeof(ast_expr_list_t));
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
                    if(parse_stmts(ctx, &after, &for_defer_scope, PARSE_STMTS_SINGLE | PARSE_STMTS_NO_JOINING | PARSE_STMTS_PARENT_DEFER_SCOPE)){
                        if(condition) ast_expr_free(condition);
                        ast_exprs_free_fully(before.statements, before.length);
                        defer_scope_free(&for_defer_scope);
                        return FAILURE;
                    }
                }

                // Eat ')' if it exists
                if(tokens[*i].id == TOKEN_CLOSE) (*i)++;

                if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
                    if(condition) ast_expr_free(condition);
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
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    ast_exprs_free_fully(after.statements, after.length);
                    defer_scope_free(&for_defer_scope);
                    return FAILURE;
                }
                
                // Parse statements
                if(parse_stmts(ctx, &statements, &for_defer_scope, stmts_mode)){
                    if(condition) ast_expr_free(condition);
                    ast_exprs_free_fully(before.statements, before.length);
                    ast_exprs_free_fully(after.statements, after.length);
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
                stmt->after = after;
                stmt->condition = condition;
                stmt->statements = statements;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_LLVM_ASM:
            if(parse_llvm_asm(ctx, stmt_list)) return FAILURE;
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
            stmt_list->statements[stmt_list->length++] = out_expr;
        }
    }

    return errorcode;
}

errorcode_t parse_stmt_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // Expects from 'ctx': compiler, object, tokenlist, i, func

    // <variable_name> <variable_type> [ = expression ]
    //       ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *token_sources = ctx->tokenlist->sources;

    weak_cstr_t *names = NULL;
    source_t *source_list = NULL;
    length_t length = 0;
    length_t capacity = 0;
    trait_t traits = TRAIT_NONE;

    switch(tokens[*i].id){
    case TOKEN_CONST:
        traits |= AST_EXPR_DECLARATION_CONST;
        (*i)++;
        break;
    case TOKEN_STATIC:
        traits |= AST_EXPR_DECLARATION_STATIC;
        (*i)++;
        break;
    }

    // Collect all variable names & sources into arrays
    while(true){
        coexpand((void**) &names, sizeof(const char*), (void**) &source_list,
            sizeof(source_t), length, &capacity, 1, 4);

        if(tokens[*i].id != TOKEN_WORD){
            compiler_panic(ctx->compiler, token_sources[*i], "Expected variable name");
            free(names);
            free(source_list);
            return FAILURE;
        }

        names[length] = tokens[*i].data;
        source_list[length++] = token_sources[(*i)++];

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++; continue;
        } else break;
    }

    if(tokens[*i].id == TOKEN_POD){
        traits |= AST_EXPR_DECLARATION_POD;
        (*i)++;
    }

    // Parse the type for the variable(s)
    ast_type_t type;
    if(parse_type(ctx, &type)){
        free(names);
        free(source_list);
        return FAILURE;
    }

    if(parse_stmt_mid_declare(ctx, stmt_list, type, names, source_list, length, traits)){
        ast_type_free(&type);
        free(names);
        free(source_list);
        return FAILURE;
    }

    free(names);
    free(source_list);
    return SUCCESS;
}

errorcode_t parse_stmt_mid_declare(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_type_t master_type, weak_cstr_t *names, source_t *source_list, length_t length, trait_t traits){
    // NOTE: 'length' is used for the length of both 'names' and 'sources'

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    unsigned int declare_stmt_type = EXPR_DECLARE;
    ast_expr_t *initial_value = NULL;

    // Parse the initial value for the variable(s) (if an initial value is given)
    if(tokens[*i].id == TOKEN_ASSIGN){
        (*i)++; // Skip over 'assign' token

        // Handle either 'undef' or an expression
        if(tokens[*i].id == TOKEN_UNDEF){
            declare_stmt_type = EXPR_DECLAREUNDEF;
            (*i)++;
        } else {
            if(tokens[*i].id == TOKEN_POD){
                traits |= AST_EXPR_DECLARATION_ASSIGN_POD;
                (*i)++;
            }

            if(parse_expr(ctx, &initial_value)) return FAILURE;
        }
    }    

    // Add each variable to the necessary data sets
    for(length_t v = 0; v != length; v++){
        ast_type_t type;
        ast_expr_t *value;

        if(v + 1 == length){
            type = master_type;
            value = initial_value;
        } else {
            type = ast_type_clone(&master_type);
            value = initial_value == NULL ? NULL : ast_expr_clone(initial_value);
        }

        if(v != 0){
            expand((void**) &stmt_list->statements, sizeof(ast_expr_t*), stmt_list->length, &stmt_list->capacity, 1, 8);
        }

        // Create the declare statement
        ast_expr_create_declaration(&stmt_list->statements[stmt_list->length++], declare_stmt_type, source_list[v], names[v], type, traits, value);
    }

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

errorcode_t parse_onetime_conditional(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, defer_scope_t *defer_scope){
    token_t *tokens = ctx->tokenlist->tokens;
    length_t *i = ctx->i;

    unsigned int conditional_type = tokens[*i].id;
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    ast_expr_t *conditional;
    trait_t stmts_mode;

    if(parse_expr(ctx, &conditional)) return FAILURE;

    if(parse_ignore_newlines(ctx, "Expected '{' or ',' after conditional expression")){
        ast_expr_free_fully(conditional);
        return FAILURE;
    }

    if(parse_block_beginning(ctx, "conditional", &stmts_mode)){
        ast_expr_free_fully(conditional);
        return FAILURE;
    }

    ast_expr_list_t if_stmt_list;
    ast_expr_list_init(&if_stmt_list, 4);

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
        ast_expr_list_init(&else_stmt_list, 4);

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

    return SUCCESS;
}

errorcode_t parse_ambiguous_open_bracket(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // This function is used to disambiguate between the two following syntaxes:
    // variable[value] ... 
    // vairable [value] Type

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

    ast_expr_list_t expr_list;
    ast_expr_list_init(&expr_list, 4);

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
        ast_expr_t *mutable_expr;
        ast_expr_create_variable(&mutable_expr, word, source);

        for(length_t i = 0; i != expr_list.length; i++){
            ast_expr_t *index_expr = expr_list.statements[i];
            ast_expr_create_access(&mutable_expr, mutable_expr, index_expr, expr_source_list[i]);
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
    source_t source = ctx->tokenlist->sources[*ctx->i];
    if(parse_expr(ctx, &mutable_expr)) return FAILURE;
    return parse_mid_mutable_expr_operation(ctx, stmt_list, mutable_expr, source);
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
    if(mutable_expr->id == EXPR_CALL_METHOD
            || mutable_expr->id == EXPR_POSTINCREMENT
            || mutable_expr->id == EXPR_POSTDECREMENT
            || mutable_expr->id == EXPR_PREINCREMENT
            || mutable_expr->id == EXPR_PREDECREMENT
            || mutable_expr->id == EXPR_TOGGLE){
        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) mutable_expr;
        return SUCCESS;
    }

    // NOTE: This is not the only place which assignment operators are handled
    unsigned int id = tokens[(*i)++].id;

    // Otherwise, it must be some type of assignment
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
        ast_expr_free_fully(mutable_expr);
        return FAILURE;
    }

    bool is_pod = false;
    if(tokens[*i].id == TOKEN_POD){
        is_pod = true;
        (*i)++;
    }

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
        ast_expr_free_fully(mutable_expr);
        ast_expr_free_fully(value_expression);
        return FAILURE;
    }

    ast_expr_create_assignment(&stmt_list->statements[stmt_list->length++], stmt_id, source, mutable_expr, value_expression, is_pod);
    return SUCCESS;
}

errorcode_t parse_llvm_asm(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
    // llvm_asm dialect { ... } "contraints" (values)
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
    } else if(strcmp(maybe_dialect, "intel") == 0){
        is_intel = TROOLEAN_TRUE;
    } else if(strcmp(maybe_dialect, "att") == 0){
        is_intel = TROOLEAN_FALSE;
    } else {
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Expected either intel or att for LLVM inline assembly dialect");
        return FAILURE;
    }

    bool has_side_effects = false;
    bool is_stack_align = false;

    maybe_null_weak_cstr_t additional_info;
    
    while((additional_info = parse_eat_word(ctx, NULL))){
        if(strcmp(additional_info, "side_effects")){
            has_side_effects = true;
        } else if(strcmp(additional_info, "stack_align")){
            is_stack_align = true;
        } else {
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Unrecognized assembly trait '%s', valid traits are: 'side_effects', 'stack_align'", additional_info);
            return FAILURE;
        }
    }

    strong_cstr_t assembly = NULL;
    length_t assembly_length = 0;
    length_t assembly_capacity = 0;

    if(parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after llvm_asm dialect")) return FAILURE;

    while(tokens[*i].id != TOKEN_END){
        switch(tokens[*i].id){
        case TOKEN_STRING: {
                token_string_data_t *string_data = (token_string_data_t*) tokens[*i].data;
                expand((void**) &assembly, sizeof(char), assembly_length, &assembly_capacity, string_data->length + 2, 512);
                memcpy(&assembly[assembly_length], string_data->array, string_data->length);
                assembly_length += string_data->length;
                assembly[assembly_length++] = '\n';
                assembly[assembly_length] = '\0';
            }
            break;
        case TOKEN_CSTRING: {
                char *string = (char*) tokens[*i].data;
                length_t length = strlen(string);
                expand((void**) &assembly, sizeof(char), assembly_length, &assembly_capacity, length + 2, 512);
                memcpy(&assembly[assembly_length], string, length);
                assembly_length += length;
                assembly[assembly_length++] = '\n';
                assembly[assembly_length] = '\0';
            }
            break;
        case TOKEN_NEXT: case TOKEN_NEWLINE:
            break;
        default:
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Expected string or ',' while inside { ... } for inline LLVM assembly");
            free(assembly);
            return FAILURE;
        }
        
        if(++(*i) == ctx->tokenlist->length){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Expected '}' for inline LLVM assembly before end-of-file");
            return FAILURE;
        }
    }

    maybe_null_weak_cstr_t constraints = parse_grab_string(ctx, "Expected constraints string after '}' for llvm_asm");
    if(constraints == NULL){
        free(assembly);
        return FAILURE;
    }

    // Move past constraints string
    (*i)++;

    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' for beginning of LLVM assembly arguments")){
        free(assembly);
        return FAILURE;
    }

    ast_expr_t **args = NULL;
    length_t arity = 0;
    length_t arity_capacity = 0;
    ast_expr_t *arg;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected argument to LLVM assembly") || parse_expr(ctx, &arg)){
            ast_exprs_free_fully(args, arity);
            free(assembly);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &args, sizeof(ast_expr_t*), arity, &arity_capacity, 1, 4);
        args[arity++] = arg;
        
        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after argument to LLVM assembly")){
            ast_exprs_free_fully(args, arity);
            free(assembly);
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' or ')' after after argument to LLVM assembly");
            ast_exprs_free_fully(args, arity);
            free(assembly);
            return FAILURE;
        }
    }

    // Move past closing ')'
    (*i)++;

    ast_expr_llvm_asm_t *stmt = malloc(sizeof(ast_expr_llvm_asm_t));
    stmt->id = EXPR_LLVM_ASM;
    stmt->source = source;
    stmt->assembly = assembly ? assembly : strclone("");
    stmt->constraints = constraints;
    stmt->args = args;
    stmt->arity = arity;
    stmt->has_side_effects = has_side_effects;
    stmt->is_stack_align = is_stack_align;
    stmt->is_intel = (is_intel == TROOLEAN_TRUE);
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    return SUCCESS;
}

errorcode_t parse_local_constant_declaration(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, source_t source){
    // NOTE: Assumes 'stmt_list' has enough space for another statement
    // NOTE: expand() should've already been used on stmt_list to make room
    
    ast_constant_t constant;

    // Parse constant
    if(parse_constant_definition(ctx, &constant)) return FAILURE;

    // Turn into declare constant statement
    ast_expr_declare_constant_t *stmt = malloc(sizeof(ast_expr_declare_constant_t));
    stmt->id = EXPR_DECLARE_CONSTANT;
    stmt->source = source;
    stmt->constant = constant;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    return SUCCESS;
}

errorcode_t parse_block_beginning(parse_ctx_t *ctx, weak_cstr_t block_readable_mother, unsigned int *out_stmts_mode){
    switch(ctx->tokenlist->tokens[*ctx->i].id){
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
