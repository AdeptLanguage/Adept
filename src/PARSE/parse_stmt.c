
#include "UTIL/util.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_meta.h"
#include "PARSE/parse_stmt.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"

errorcode_t parse_stmts(parse_ctx_t *ctx, ast_expr_list_t *stmt_list, ast_expr_list_t *defer_list, trait_t mode){
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

                // Unravel all remaining defer statements
                parse_unravel_defer_stmts(stmt_list, defer_list, 0);

                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_WORD: {
                source = sources[(*i)++]; // Read ahead to see what type of statement this is

                switch(tokens[*i].id){
                case TOKEN_OPEN:
                    (*i)++; if(parse_stmt_call(ctx, stmt_list)) return FAILURE;
                    break;
                case TOKEN_WORD: case TOKEN_FUNC:
                case TOKEN_STDCALL: case TOKEN_NEXT: case TOKEN_POD:
                case TOKEN_GENERIC_INT: /*fixed array*/ case TOKEN_MULTIPLY: /*pointer*/
                    (*i)--; if(parse_stmt_declare(ctx, stmt_list)) return FAILURE;
                    break;
                default: { // Assume assign statement if not one of the above
                        (*i)--;
                        ast_expr_t *mutable_expression;
                        if(parse_expr(ctx, &mutable_expression)) return FAILURE;

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
                            return FAILURE;
                        }

                        bool is_pod = false;
                        if(tokens[*i].id == TOKEN_POD){
                            is_pod = true;
                            (*i)++;
                        }

                        if(!EXPR_IS_MUTABLE(mutable_expression->id)){
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
                            return FAILURE;
                        }

                        ast_expr_assign_t *stmt = malloc(sizeof(ast_expr_assign_t));
                        stmt->id = stmt_id;
                        stmt->source = source;
                        stmt->destination = mutable_expression;
                        stmt->value = value_expression;
                        stmt->is_pod = is_pod;
                        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
                    }
                    break;
                }
            }
            break;
        case TOKEN_MULTIPLY: case TOKEN_OPEN: {
                source = sources[*i];
                ast_expr_t *mutable_expression;
                if(parse_expr(ctx, &mutable_expression)) return FAILURE;

                unsigned int id = tokens[(*i)++].id;
                if(id != TOKEN_ASSIGN && id != TOKEN_ADDASSIGN && id != TOKEN_SUBTRACTASSIGN &&
                        id != TOKEN_MULTIPLYASSIGN && id != TOKEN_DIVIDEASSIGN && id != TOKEN_MODULUSASSIGN){
                    compiler_panic(ctx->compiler, sources[(*i) - 1], "Expected assignment operator after expression");
                    ast_expr_free_fully(mutable_expression);
                    return FAILURE;
                }

                if(!EXPR_IS_MUTABLE(mutable_expression->id)){
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
                case TOKEN_ASSIGN: stmt_id = EXPR_ASSIGN; break;
                case TOKEN_ADDASSIGN: stmt_id = EXPR_ADDASSIGN; break;
                case TOKEN_SUBTRACTASSIGN: stmt_id = EXPR_SUBTRACTASSIGN; break;
                case TOKEN_MULTIPLYASSIGN: stmt_id = EXPR_MULTIPLYASSIGN; break;
                case TOKEN_DIVIDEASSIGN: stmt_id = EXPR_DIVIDEASSIGN; break;
                case TOKEN_MODULUSASSIGN: stmt_id = EXPR_MODULUSASSIGN; break;
                default:
                    compiler_panic(ctx->compiler, sources[*i], "INTERNAL ERROR: parse_stmts() came across unknown assignment operator");
                    ast_expr_free_fully(mutable_expression);
                    return FAILURE;
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

                if(parse_expr(ctx, &conditional)) return FAILURE;

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

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &if_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(if_stmt_list.statements, if_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    return FAILURE;
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
                        return FAILURE;
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

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &while_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(while_stmt_list.statements, while_stmt_list.length);
                    ast_expr_free_fully(conditional);
                    return FAILURE;
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
                maybe_null_strong_cstr_t it_name = NULL;
                ast_type_t *it_type = NULL;
                trait_t stmts_mode;
                maybe_null_weak_cstr_t label = NULL;

                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id == TOKEN_COLON){
                    label = tokens[*i].data; *i += 2;
                }
                
                if(tokens[*i].id == TOKEN_WORD && tokens[*i + 1].id != TOKEN_IN){
                    it_name = parse_take_word(ctx, "Expected name for 'it' variable");

                    if(!it_name){
                        // Should only reach this point when syntax is incorrect
                        return FAILURE;
                    }
                }

                it_type = malloc(sizeof(ast_type_t));

                if(parse_type(ctx, it_type)
                || parse_eat(ctx, TOKEN_IN, "Expected 'in' keyword")){
                    free(it_name);
                    free(it_type);
                    return FAILURE;
                }

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
                } else {
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected [<data>, <length>] after 'in' keyword in 'each in' statement");
                    ast_type_free_fully(it_type);
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
                    free(it_name);
                    return FAILURE;
                }

                ast_expr_list_t each_in_stmt_list;
                each_in_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                each_in_stmt_list.length = 0;
                each_in_stmt_list.capacity = 4;

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &each_in_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(each_in_stmt_list.statements, each_in_stmt_list.length);
                    ast_type_free_fully(it_type);
                    ast_expr_free_fully(low_array);
                    ast_expr_free_fully(length_limit);
                    free(it_name);
                    return FAILURE;
                }

                // Unravel all defer statements added in the block
                parse_unravel_defer_stmts(&each_in_stmt_list, defer_list, defer_unravel_length);

                if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;
                else if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;

                ast_expr_each_in_t *stmt = malloc(sizeof(ast_expr_each_in_t));
                stmt->id = EXPR_EACH_IN;
                stmt->source = source;
                stmt->label = label;
                stmt->it_name = it_name;
                stmt->it_type = it_type;
                stmt->length = length_limit;
                stmt->low_array = low_array;
                stmt->statements = each_in_stmt_list.statements;
                stmt->statements_length = each_in_stmt_list.length;
                stmt->statements_capacity = each_in_stmt_list.capacity;
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
                
                if(parse_expr(ctx, &limit)) return FAILURE;

                switch(tokens[(*i)++].id){
                case TOKEN_BEGIN: stmts_mode = PARSE_STMTS_STANDARD; break;
                case TOKEN_NEXT:  stmts_mode = PARSE_STMTS_SINGLE;   break;
                default:
                    compiler_panic(ctx->compiler, sources[*i - 1], "Expected '{' or ',' after 'repeat' expression");
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                ast_expr_list_t each_in_stmt_list;
                each_in_stmt_list.statements = malloc(sizeof(ast_expr_t*) * 4);
                each_in_stmt_list.length = 0;
                each_in_stmt_list.capacity = 4;

                length_t defer_unravel_length = defer_list->length;

                if(parse_stmts(ctx, &each_in_stmt_list, defer_list, stmts_mode)){
                    ast_free_statements_fully(each_in_stmt_list.statements, each_in_stmt_list.length);
                    ast_expr_free_fully(limit);
                    return FAILURE;
                }

                // Unravel all defer statements added in the block
                parse_unravel_defer_stmts(&each_in_stmt_list, defer_list, defer_unravel_length);

                if(stmts_mode == PARSE_STMTS_STANDARD) (*i)++;
                else if(stmts_mode == PARSE_STMTS_SINGLE) (*i)--;

                ast_expr_repeat_t *stmt = malloc(sizeof(ast_expr_repeat_t));
                stmt->id = EXPR_REPEAT;
                stmt->source = source;
                stmt->label = label;
                stmt->limit = limit;
                stmt->statements = each_in_stmt_list.statements;
                stmt->statements_length = each_in_stmt_list.length;
                stmt->statements_capacity = each_in_stmt_list.capacity;
                stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
            }
            break;
        case TOKEN_DEFER: {
                (*i)++; // Skip over 'defer' keyword
                if(parse_stmts(ctx, defer_list, defer_list, PARSE_STMTS_SINGLE)) return FAILURE;
                (*i)--; // Go back to newline because we used PARSE_STMTS_SINGLE
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
        case TOKEN_META:
            if(parse_meta(ctx)) return FAILURE;
            break;
        default:
            parse_panic_token(ctx, sources[*i], tokens[*i].id, "Encountered unexpected token '%s' at beginning of statement");
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_TERMINATE_JOIN){
            // Bypass single statement flag by "joining" 2+ statements
            (*i)++; continue;
        }

        // Continue over newline token
        if(tokens[*i].id != TOKEN_ELSE && tokens[(*i)++].id != TOKEN_NEWLINE){
            parse_panic_token(ctx, sources[*i - 1], tokens[*i - 1].id, "Encountered unexpected token '%s' at end of statement");
            return FAILURE;
        }

        if(mode & PARSE_STMTS_SINGLE) return SUCCESS;
    }

    return SUCCESS; // '}' was reached
}

errorcode_t parse_stmt_call(parse_ctx_t *ctx, ast_expr_list_t *stmt_list){
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
        if(parse_ignore_newlines(ctx, "Expected function argument")) return FAILURE;

        if(parse_expr(ctx, &arg_expr)){
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt);
            return FAILURE;
        }

        // Allocate room for more arguments if necessary
        expand((void**) &stmt->args, sizeof(ast_expr_t*), stmt->arity, &args_capacity, 1, 4);

        stmt->args[stmt->arity++] = arg_expr;

        if(parse_ignore_newlines(ctx, "Expected ',' or ')' after expression")) return FAILURE;

        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' or ')' after expression");
            ast_exprs_free_fully(stmt->args, stmt->arity);
            free(stmt);
            return FAILURE;
        }
    }

    (*i)++;
    stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
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

        // Append the created declare statement
        stmt_list->statements[stmt_list->length++] = (ast_expr_t*) stmt;
    }

    free(decl_names);
    free(decl_sources);
    return SUCCESS;
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
