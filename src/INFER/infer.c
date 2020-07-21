
#include "INFER/infer.h"
#include "AST/ast_expr.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "UTIL/levenshtein.h"
#include "UTIL/builtin_type.h"
#include "BRIDGE/type_table.h"

errorcode_t infer(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;

    infer_ctx_t ctx;
    ctx.compiler = compiler;
    ctx.object = object;
    ctx.ast = ast;

    ast->type_table = malloc(sizeof(type_table_t));
    type_table_init(ast->type_table);
    ctx.type_table = ast->type_table;

    // Sort aliases and constants so we can binary search on them later
    qsort(ast->aliases, ast->aliases_length, sizeof(ast_alias_t), ast_aliases_cmp);
    qsort(ast->constants, ast->constants_length, sizeof(ast_constant_t), ast_constants_cmp);
    qsort(ast->enums, ast->enums_length, sizeof(ast_enum_t), ast_enums_cmp);

    for(length_t a = 0; a != ast->aliases_length; a++){
        if(infer_type(&ctx, &ast->aliases[a].type)) return FAILURE;

        // Ensure we have the alias type info at runtime
        type_table_give(ctx.type_table, &ast->aliases[a].type, strclone(ast->aliases[a].name));
    }

    for(length_t s = 0; s != ast->structs_length; s++){
        ast_struct_t *st = &ast->structs[s];
        for(length_t f = 0; f != st->field_count; f++){
            if(infer_type(&ctx, &st->field_types[f])) return FAILURE;
        }

        type_table_give_base(ctx.type_table, st->name);
    }

    for(length_t g = 0; g != ast->globals_length; g++){
        if(infer_type(&ctx, &ast->globals[g].type)) return FAILURE;
        ast_expr_t **global_initial = &ast->globals[g].initial;
        if(*global_initial != NULL){
            unsigned int default_primitive = ast_primitive_from_ast_type(&ast->globals[g].type);
            if(infer_expr(&ctx, NULL, global_initial, default_primitive, NULL)) return FAILURE;
        }
    }

    if(infer_in_funcs(&ctx, ast->funcs, ast->funcs_length)) return FAILURE;

    ast->type_table = ctx.type_table;
    return SUCCESS;
}

errorcode_t infer_in_funcs(infer_ctx_t *ctx, ast_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        infer_var_scope_t func_scope;
        infer_var_scope_init(&func_scope, NULL);

        ast_func_t *function = &funcs[f];

        // Resolve aliases in function return type
        if(infer_type(ctx, &function->return_type)) return FAILURE;

        // Resolve aliases in function arguments
        for(length_t a = 0; a != function->arity; a++){
            if(infer_type(ctx, &function->arg_types[a])) {
                return FAILURE;
            }

            if(function->arg_defaults && function->arg_defaults[a]){
                unsigned int default_primitive = ast_primitive_from_ast_type(&funcs[f].arg_types[a]);
                if(infer_expr(ctx, function, &function->arg_defaults[a], default_primitive, &func_scope)){
                    infer_var_scope_free(&func_scope);
                    return FAILURE;
                }
            }

            if(!(function->traits & AST_FUNC_FOREIGN)){
                infer_var_scope_add_variable(&func_scope, function->arg_names[a], &function->arg_types[a]);
            }
        }

        // Infer expressions in statements
        if(infer_in_stmts(ctx, function, function->statements, function->statements_length, &func_scope)){
            infer_var_scope_free(&func_scope);
            return FAILURE;
        }
        infer_var_scope_free(&func_scope);
    }
    return SUCCESS;
}

errorcode_t infer_in_stmts(infer_ctx_t *ctx, ast_func_t *func, ast_expr_t **statements, length_t statements_length, infer_var_scope_t *scope){
    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN: {
                ast_expr_return_t *return_stmt = (ast_expr_return_t*) statements[s];
                if(return_stmt->value != NULL && infer_expr(ctx, func, &return_stmt->value, ast_primitive_from_ast_type(&func->return_type), scope)) return FAILURE;
                if(infer_in_stmts(ctx, func, return_stmt->last_minute.statements, return_stmt->last_minute.length, scope)) return FAILURE;
            }
            break;
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = (ast_expr_call_t*) statements[s];
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(ctx, func, &call_stmt->args[a], EXPR_NONE, scope)) return FAILURE;
                }
            }
            break;
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) statements[s];
                if(infer_type(ctx, &declare_stmt->type)) return FAILURE;
                if(declare_stmt->value != NULL){
                    if(infer_expr(ctx, func, &declare_stmt->value, ast_primitive_from_ast_type(&declare_stmt->type), scope)) return FAILURE;
                }

                infer_var_scope_add_variable(scope, declare_stmt->name, &declare_stmt->type);
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
        case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
        case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
        case EXPR_LS_ASSIGN: case EXPR_RS_ASSIGN:
        case EXPR_LGC_LS_ASSIGN: case EXPR_LGC_RS_ASSIGN: {
                ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) statements[s];
                if(infer_expr(ctx, func, &assign_stmt->destination, EXPR_NONE, scope)) return FAILURE;
                if(infer_expr(ctx, func, &assign_stmt->value, EXPR_NONE, scope)) return FAILURE;
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
                ast_expr_if_t *conditional = (ast_expr_if_t*) statements[s];
                if(infer_expr(ctx, func, &conditional->value, EXPR_NONE, scope)) return FAILURE;

                infer_var_scope_push(&scope);
                if(infer_in_stmts(ctx, func, conditional->statements, conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) statements[s];
                if(infer_expr(ctx, func, &complex_conditional->value, EXPR_NONE, scope)) return FAILURE;

                infer_var_scope_push(&scope);
                if(infer_in_stmts(ctx, func, complex_conditional->statements, complex_conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                
                infer_var_scope_pop(&scope);
                infer_var_scope_push(&scope);
                if(infer_in_stmts(ctx, func, complex_conditional->else_statements, complex_conditional->else_statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) statements[s];
                infer_var_scope_push(&scope);
                if(infer_in_stmts(ctx, func, conditional->statements, conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                if(infer_expr(ctx, func, &call_stmt->value, EXPR_NONE, scope)) return FAILURE;
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(ctx, func, &call_stmt->args[a], EXPR_NONE, scope)) return FAILURE;
                }
            }
            break;
        case EXPR_DELETE: {
                ast_expr_unary_t *delete_stmt = (ast_expr_unary_t*) statements[s];
                if(infer_expr(ctx, func, &delete_stmt->value, EXPR_NONE, scope)) return FAILURE;
            }
            break;
        case EXPR_EACH_IN: {
                ast_expr_each_in_t *loop = (ast_expr_each_in_t*) statements[s];
                if(infer_type(ctx, loop->it_type)) return FAILURE;
                
                if(loop->low_array && infer_expr(ctx, func, &loop->low_array, EXPR_NONE, scope)) return FAILURE;
                if(loop->length    && infer_expr(ctx, func, &loop->length, EXPR_USIZE, scope))   return FAILURE;
                if(loop->list      && infer_expr(ctx, func, &loop->list, EXPR_USIZE, scope))     return FAILURE;
 
                infer_var_scope_push(&scope);
                infer_var_scope_add_variable(scope, "idx", ast_get_usize(ctx->ast));
                infer_var_scope_add_variable(scope, loop->it_name ? loop->it_name : "it", loop->it_type);

                if(infer_in_stmts(ctx, func, loop->statements, loop->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_REPEAT: {
                ast_expr_repeat_t *loop = (ast_expr_repeat_t*) statements[s];
                if(infer_expr(ctx, func, &loop->limit, EXPR_USIZE, scope)) return FAILURE;
 
                infer_var_scope_push(&scope);
                infer_var_scope_add_variable(scope, "idx", ast_get_usize(ctx->ast));

                if(infer_in_stmts(ctx, func, loop->statements, loop->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_SWITCH: {
                ast_expr_switch_t *expr_switch = (ast_expr_switch_t*) statements[s];
                if(infer_expr(ctx, func, &expr_switch->value, EXPR_NONE, scope)) return FAILURE;

                for(length_t c = 0; c != expr_switch->cases_length; c++){
                    ast_case_t *expr_case = &expr_switch->cases[c];

                    if(infer_expr(ctx, func, &expr_case->condition, EXPR_NONE, scope)) return FAILURE;

                    infer_var_scope_push(&scope);
                    if(infer_in_stmts(ctx, func, expr_case->statements, expr_case->statements_length, scope)){
                        infer_var_scope_pop(&scope);
                        return FAILURE;
                    }
                    infer_var_scope_pop(&scope);
                }

                infer_var_scope_push(&scope);
                if(infer_in_stmts(ctx, func, expr_switch->default_statements, expr_switch->default_statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return FAILURE;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        default: break;
            // Ignore this statement, it doesn't contain any expressions that we need to worry about
        }
    }
    return SUCCESS;
}

errorcode_t infer_expr(infer_ctx_t *ctx, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type, infer_var_scope_t *scope){
    // NOTE: The 'ast_expr_t*' pointed to by 'root' may be written to
    // NOTE: 'default_assigned_type' is used as a default assigned type if generics aren't resolved within the expression
    // NOTE: if 'default_assigned_type' is EXPR_NONE, then the most suitable type for the given generics is chosen
    // NOTE: 'ast_func' can be NULL

    undetermined_expr_list_t undetermined;
    undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
    undetermined.expressions_length = 0;
    undetermined.expressions_capacity = 4;
    undetermined.solution = EXPR_NONE;

    if(infer_expr_inner(ctx, ast_func, root, &undetermined, scope)){
        free(undetermined.expressions);
        return FAILURE;
    }

    if(undetermined.solution == EXPR_NONE){
        if(default_assigned_type == EXPR_NONE){
            default_assigned_type = generics_primitive_type(undetermined.expressions, undetermined.expressions_length);
        }

        if(default_assigned_type != EXPR_NONE){
            infer_mention_expression_literal_type(ctx, default_assigned_type);
            if(undetermined_expr_list_give_solution(ctx, &undetermined, default_assigned_type)){
                free(undetermined.expressions);
                return FAILURE;
            }
        }
    }

    // <expressions inside undetermined.expressions are just references so we dont free those>
    free(undetermined.expressions);
    return SUCCESS;
}

errorcode_t infer_expr_inner(infer_ctx_t *ctx, ast_func_t *ast_func, ast_expr_t **expr, undetermined_expr_list_t *undetermined, infer_var_scope_t *scope){
    // NOTE: 'ast_func' can be NULL
    // NOTE: The 'ast_expr_t*' pointed to by 'expr' may be written to

    ast_expr_t **a, **b;

    // Expand list if needed
    expand((void**) &undetermined->expressions, sizeof(ast_expr_t*), undetermined->expressions_length,
        &undetermined->expressions_capacity, 1, 2);

    switch((*expr)->id){
    case EXPR_NONE: break;
    case EXPR_BYTE:
        type_table_give_base(ctx->type_table, "byte");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_UBYTE:
        type_table_give_base(ctx->type_table, "ubyte");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_SHORT:
        type_table_give_base(ctx->type_table, "short");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_USHORT:
        type_table_give_base(ctx->type_table, "ushort");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_INT:
        type_table_give_base(ctx->type_table, "int");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_UINT:
        type_table_give_base(ctx->type_table, "uint");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_LONG:
        type_table_give_base(ctx->type_table, "long");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_ULONG:
        type_table_give_base(ctx->type_table, "ulong");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_USIZE:
        type_table_give_base(ctx->type_table, "usize");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_FLOAT:
        type_table_give_base(ctx->type_table, "float");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_DOUBLE:
        type_table_give_base(ctx->type_table, "double");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_BOOLEAN:
        type_table_give_base(ctx->type_table, "bool");
        if(undetermined_expr_list_give_solution(ctx, undetermined, (*expr)->id)) return FAILURE;
        break;
    case EXPR_VARIABLE: {
            bool found_variable = false;
            char *variable_name = ((ast_expr_variable_t*) *expr)->name;
            ast_type_t *variable_type;

            if(ast_func == NULL || scope == NULL){
                compiler_panicf(ctx->compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                return FAILURE;
            }

            infer_var_t *infer_var = infer_var_scope_find(scope, variable_name);

            if(infer_var != NULL){
                variable_type = infer_var->type;
                found_variable = true;
            }

            ast_t *ast = ctx->ast;

            // Search in globals if local one couldn't be found
            if(!found_variable){
                for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(variable_name, ast->globals[g].name) == 0){
                        variable_type = &ast->globals[g].type;
                        found_variable = true;
                        break;
                    }
                }
            }

            // See if this is a constant
            if(!found_variable){
                length_t constant_index = find_constant(ast->constants, ast->constants_length, variable_name);

                if(constant_index != -1){
                    // Constant does exist, substitute it's value

                    // DANGEROUS: Manually freeing variable expression
                    free(*expr);

                    *expr = ast_expr_clone(ast->constants[constant_index].expression); // Clone constant expression
                    if(infer_expr_inner(ctx, ast_func, expr, undetermined, scope)) return FAILURE;

                    break; // The identifier has been resovled, so break
                }
            }

            if(!found_variable){
                compiler_panicf(ctx->compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                const char *nearest = infer_var_scope_nearest(scope, variable_name);
                if(nearest) printf("\nDid you mean '%s'?\n", nearest);
                return FAILURE;
            }

            // Return if we already know the solution primitive
            if(undetermined->solution != EXPR_NONE) return SUCCESS;

            unsigned int var_expr_primitive = ast_primitive_from_ast_type(variable_type);
            if(undetermined_expr_list_give_solution(ctx, undetermined, var_expr_primitive)) return FAILURE;
        }
        break;
    case EXPR_STR: break;
    case EXPR_CSTR: break;
    case EXPR_ADD:
    case EXPR_SUBTRACT:
    case EXPR_MULTIPLY:
    case EXPR_DIVIDE:
    case EXPR_MODULUS:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
    case EXPR_BIT_LGC_LSHIFT:
    case EXPR_BIT_LGC_RSHIFT:
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr_inner(ctx, ast_func, a, undetermined, scope)) return FAILURE;
        if(infer_expr_inner(ctx, ast_func, b, undetermined, scope)) return FAILURE;
        break;
    case EXPR_EQUALS:
    case EXPR_NOTEQUALS:
    case EXPR_GREATER:
    case EXPR_LESSER:
    case EXPR_GREATEREQ:
    case EXPR_LESSEREQ: {
            undetermined_expr_list_t local_undetermined;
            local_undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
            local_undetermined.expressions_length = 0;
            local_undetermined.expressions_capacity = 4;
            local_undetermined.solution = EXPR_NONE;

            // Group inference for two child expressions
            a = &((ast_expr_math_t*) *expr)->a;
            b = &((ast_expr_math_t*) *expr)->b;
            if(infer_expr_inner(ctx, ast_func, a, &local_undetermined, scope) || infer_expr_inner(ctx, ast_func, b, &local_undetermined, scope)){
                free(local_undetermined.expressions);
                return FAILURE;
            }

            // Resolve primitives in children if needed
            if(local_undetermined.solution == EXPR_NONE){
                unsigned int default_assigned_type = generics_primitive_type(local_undetermined.expressions, local_undetermined.expressions_length);
                if(default_assigned_type != EXPR_NONE){
                    infer_mention_expression_literal_type(ctx, default_assigned_type);
                    if(undetermined_expr_list_give_solution(ctx, &local_undetermined, default_assigned_type)){
                        free(local_undetermined.expressions);
                        return FAILURE;
                    }
                }
            }
            free(local_undetermined.expressions);
        }
        break;
    case EXPR_AND:
    case EXPR_OR:
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr(ctx, ast_func, a, EXPR_NONE, scope)) return FAILURE;
        if(infer_expr(ctx, ast_func, b, EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_CALL:
        for(length_t i = 0; i != ((ast_expr_call_t*) *expr)->arity; i++){
            if(infer_expr(ctx, ast_func, &((ast_expr_call_t*) *expr)->args[i], EXPR_NONE, scope)) return FAILURE;
        }
        break;
    case EXPR_GENERIC_INT:
    case EXPR_GENERIC_FLOAT:
        if(undetermined_expr_list_give_generic(ctx, undetermined, expr)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(infer_expr(ctx, ast_func, &((ast_expr_member_t*) *expr)->value, EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) *expr;
            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(infer_type(ctx, &func_addr->match_args[a])) return FAILURE;
            }
        }
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
        if(infer_expr(ctx, ast_func, &((ast_expr_unary_t*) *expr)->value, EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_NULL:
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        if(infer_expr(ctx, ast_func, &((ast_expr_array_access_t*) *expr)->index, EXPR_NONE, scope)) return FAILURE;
        if(infer_expr(ctx, ast_func, &((ast_expr_array_access_t*) *expr)->value, EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_CAST:
        if(infer_type(ctx, &((ast_expr_cast_t*) *expr)->to)) return FAILURE;
        if(infer_expr(ctx, ast_func, &((ast_expr_cast_t*) *expr)->from, EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_SIZEOF:
        if(infer_type(ctx, &((ast_expr_sizeof_t*) *expr)->type)) return FAILURE;
        break;
    case EXPR_CALL_METHOD:
        if(infer_expr_inner(ctx, ast_func, &((ast_expr_call_method_t*) *expr)->value, undetermined, scope)) return FAILURE;
        for(length_t i = 0; i != ((ast_expr_call_method_t*) *expr)->arity; i++){
            if(infer_expr(ctx, ast_func, &((ast_expr_call_method_t*) *expr)->args[i], EXPR_NONE, scope)) return FAILURE;
        }
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
    case EXPR_PREINCREMENT:
    case EXPR_PREDECREMENT:
    case EXPR_POSTDECREMENT:
    case EXPR_POSTINCREMENT:
    case EXPR_TOGGLE:
        if(infer_expr_inner(ctx, ast_func, &((ast_expr_unary_t*) *expr)->value, undetermined, scope)) return FAILURE;
        break;
    case EXPR_NEW:
        if(infer_type(ctx, &((ast_expr_new_t*) *expr)->type)) return FAILURE;
        if(((ast_expr_new_t*) *expr)->amount != NULL && infer_expr(ctx, ast_func, &(((ast_expr_new_t*) *expr)->amount), EXPR_NONE, scope)) return FAILURE;
        break;
    case EXPR_NEW_CSTRING:
    case EXPR_ENUM_VALUE:
        break;
    case EXPR_STATIC_ARRAY: {
            if(infer_type(ctx, &((ast_expr_static_data_t*) *expr)->type)) return FAILURE;

            unsigned int default_primitive = ast_primitive_from_ast_type(&((ast_expr_static_data_t*) *expr)->type);

            for(length_t i = 0; i != ((ast_expr_static_data_t*) *expr)->length; i++){
                if(infer_expr(ctx, ast_func, &((ast_expr_static_data_t*) *expr)->values[i], default_primitive, scope)) return FAILURE;
            }
        }
        break;
    case EXPR_STATIC_STRUCT: {
            ast_expr_static_data_t *static_data = (ast_expr_static_data_t*) *expr;
            if(infer_type(ctx, &static_data->type)) return FAILURE;

            if(static_data->type.elements_length != 1 || static_data->type.elements[0]->id != AST_ELEM_BASE){
                char *s = ast_type_str(&static_data->type);
                compiler_panicf(ctx->compiler, static_data->type.source, "Can't create struct literal for non-struct type '%s'\n", s);
                free(s);
                return FAILURE;
            }

            const char *base = ((ast_elem_base_t*) static_data->type.elements[0])->base;
            ast_struct_t *structure = ast_struct_find(ctx->ast, base);

            if(structure == NULL){
                if(typename_is_entended_builtin_type(base)){
                    compiler_panicf(ctx->compiler, static_data->type.source, "Can't create struct literal for built-in type '%s'", base);
                } else {
                    compiler_panicf(ctx->compiler, static_data->type.source, "INTERNAL ERROR: Failed to find struct '%s' that should exist", base);
                }
                return FAILURE;
            }

            if(static_data->length != structure->field_count){
                if(static_data->length > structure->field_count){
                    compiler_panicf(ctx->compiler, static_data->type.source, "Too many fields specified for struct '%s' (expected %d, got %d)", base, structure->field_count, static_data->length);
                } else {
                    compiler_panicf(ctx->compiler, static_data->type.source, "Not enough fields specified for struct '%s' (expected %d, got %d)", base, structure->field_count, static_data->length);
                }
                return FAILURE;
            }

            for(length_t i = 0; i != ((ast_expr_static_data_t*) *expr)->length; i++){
                unsigned int default_primitive = ast_primitive_from_ast_type(&structure->field_types[i]);
                if(infer_expr(ctx, ast_func, &((ast_expr_static_data_t*) *expr)->values[i], default_primitive, scope)) return FAILURE;
            }
        }
        break;
    case EXPR_TYPEINFO: {
            // Don't infer target type, but give it to the type table
            type_table_give(ctx->type_table, &(((ast_expr_typeinfo_t*) *expr)->target), NULL);
        }
        break;
    case EXPR_TERNARY: {
            if(infer_expr(ctx, ast_func, &((ast_expr_ternary_t*) *expr)->condition, EXPR_NONE, scope)) return FAILURE;

            undetermined_expr_list_t local_undetermined;
            local_undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
            local_undetermined.expressions_length = 0;
            local_undetermined.expressions_capacity = 4;
            local_undetermined.solution = EXPR_NONE;

            // Group inference for two child expressions
            a = &((ast_expr_ternary_t*) *expr)->if_true;
            b = &((ast_expr_ternary_t*) *expr)->if_false;
            if(infer_expr_inner(ctx, ast_func, a, &local_undetermined, scope) || infer_expr_inner(ctx, ast_func, b, &local_undetermined, scope)){
                free(local_undetermined.expressions);
                return FAILURE;
            }

            // Resolve primitives in children if needed
            if(local_undetermined.solution == EXPR_NONE){
                unsigned int default_assigned_type = generics_primitive_type(local_undetermined.expressions, local_undetermined.expressions_length);
                if(default_assigned_type != EXPR_NONE){
                    infer_mention_expression_literal_type(ctx, default_assigned_type);
                    if(undetermined_expr_list_give_solution(ctx, &local_undetermined, default_assigned_type)){
                        free(local_undetermined.expressions);
                        return FAILURE;
                    }
                }
            }
            free(local_undetermined.expressions);
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = (ast_expr_inline_declare_t*) *expr;
            if(infer_type(ctx, &def->type)) return FAILURE;

            if(def->value != NULL && infer_expr(ctx, ast_func, &def->value, ast_primitive_from_ast_type(&def->type), scope) == FAILURE){
                return FAILURE;
            }

            infer_var_scope_add_variable(scope, def->name, &def->type);
        }
        break;
    default:
        printf("%08X\n", (*expr)->id);
        compiler_panic(ctx->compiler, (*expr)->source, "INTERNAL ERROR: Unimplemented expression type inside infer_expr_inner");
        return FAILURE;
    }

    return SUCCESS;
}

int undetermined_expr_list_give_solution(infer_ctx_t *ctx, undetermined_expr_list_t *undetermined, unsigned int solution_primitive){
    // Ensure solution is real
    if(solution_primitive == EXPR_NONE) return SUCCESS;

    // Ensure we don't already have a solution
    if(undetermined->solution != EXPR_NONE) return SUCCESS;
    
    // Remember the solution
    undetermined->solution = solution_primitive;

    // Resolve any generics already in the list
    if(resolve_generics(ctx, undetermined->expressions, undetermined->expressions_length, solution_primitive)){
        return FAILURE;
    }

    undetermined->expressions_length = 0;
    return SUCCESS;
}

int undetermined_expr_list_give_generic(infer_ctx_t *ctx, undetermined_expr_list_t *undetermined, ast_expr_t **expr){
    if(undetermined->solution != EXPR_NONE){
        // Resolve the generic if we already know what type it should be
        if(resolve_generics(ctx, expr, 1, undetermined->solution)) return FAILURE;
    } else {
        // Otherwise, add it to the list of undetermined expressions
        undetermined->expressions[undetermined->expressions_length++] = *expr;
    }
    return SUCCESS;
}

int resolve_generics(infer_ctx_t *ctx, ast_expr_t **expressions, length_t expressions_length, unsigned int target_primitive){
    // NOTE: Some dangerous memory stuff goes on in this function
    //           This may break on certain platforms if they use unconventional type sizes that break the C standards
    // NOTE: This function attempts to convert generics expressions into the 'target_primitive'
    // NOTE: Shouldn't be called outside of resolve_undetermined_expressions

    for(length_t e = 0; e != expressions_length; e++){
        ast_expr_t *expr = expressions[e];

        switch(expr->id){
        case EXPR_GENERIC_INT:
            switch(target_primitive){
            case EXPR_BOOLEAN: {
                     // This is ok because the memory that was allocated is larger than needed
                    ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_int_t*) expr)->value != 0);
                }
                break;
            case EXPR_BYTE: case EXPR_UBYTE: {
                    char tmp = (char) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_byte_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_SHORT: case EXPR_USHORT: {
                    short tmp = (short) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_short_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_INT: case EXPR_UINT: case EXPR_LONG: case EXPR_ULONG: case EXPR_USIZE:
                break; // No changes special changes necessary
            case EXPR_FLOAT: case EXPR_DOUBLE: {
                    double tmp = (double) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_double_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            default:
                compiler_panic(ctx->compiler, expr->source, "INTERNAL ERROR: Unrecognized target primitive in resolve_generics()");
                return FAILURE; // Unknown assigned type
            }
            expr->id = target_primitive;
            break;
        case EXPR_GENERIC_FLOAT:
            switch(target_primitive){
            case EXPR_BOOLEAN: {
                     // This is ok because the memory that was allocated is larger than needed
                    ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_float_t*) expr)->value != 0);
                    compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a bool");
                }
                break;
            case EXPR_BYTE: case EXPR_UBYTE: {
                    char tmp = (char) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_byte_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                    compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a %s", target_primitive == EXPR_BYTE ? "byte" : "ubyte");
                }
                break;
            case EXPR_SHORT: case EXPR_USHORT: {
                    short tmp = (short) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_short_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                    compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a %s", target_primitive == EXPR_SHORT ? "short" : "ushort");
                }
                break;
            case EXPR_INT: case EXPR_UINT: case EXPR_LONG: case EXPR_ULONG: case EXPR_USIZE: {
                    long long tmp = (long long) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_int_t*) expr)->value = tmp; // // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_FLOAT: case EXPR_DOUBLE:
                break; // No changes special changes necessary
            default:
                compiler_panic(ctx->compiler, expr->source, "INTERNAL ERROR: Unrecognized target primitive in resolve_generics()");
                return FAILURE; // Unknown assigned type
            }
            expr->id = target_primitive;
            break;
        // Ignore any non-generic expressions
        }
    }

    return SUCCESS;
}

unsigned int generics_primitive_type(ast_expr_t **expressions, length_t expressions_length){
    // NOTE: This function determines what the primitive type should be of a list of
    //           generic expressions 'expressions' that don't have any outside influence
    //           with concrete primitives
    //       e.g. the primitve type of (10 + 8.0 + 3) is a double
    //       e.g. the primitve type of (10 + 3) is an int

    // Do best to find a common type for generic types
    char generics = 0x00;
    #define FLAG_GENERIC_INT   0x01
    #define FLAG_GENERIC_FLOAT 0x02

    for(length_t i = 0; i != expressions_length; i++){
        if(expressions[i]->id == EXPR_GENERIC_INT)   {generics |= FLAG_GENERIC_INT;   continue;}
        if(expressions[i]->id == EXPR_GENERIC_FLOAT) {generics |= FLAG_GENERIC_FLOAT; continue;}
    }

    // Resolve from generics encountered:
    unsigned int assigned_type = EXPR_NONE;

    if((generics & FLAG_GENERIC_INT) && (generics & FLAG_GENERIC_FLOAT)) assigned_type = EXPR_DOUBLE;
    else if(generics & FLAG_GENERIC_INT)   assigned_type = EXPR_INT;
    else if(generics & FLAG_GENERIC_FLOAT) assigned_type = EXPR_DOUBLE;

    #undef FLAG_GENERIC_INT
    #undef FLAG_GENERIC_FLOAT
    return assigned_type;
}

unsigned int ast_primitive_from_ast_type(ast_type_t *type){
    // NOTE: Returns EXPR_NONE when no suitable fit primitive

    if(type->elements_length != 1) return EXPR_NONE;
    if(type->elements[0]->id != AST_ELEM_BASE) return EXPR_NONE;

    char *base = ((ast_elem_base_t*) type->elements[0])->base;
    maybe_index_t builtin = typename_builtin_type(base);

    switch(builtin){
    case BUILTIN_TYPE_BOOL:       return EXPR_BOOLEAN;
    case BUILTIN_TYPE_BYTE:       return EXPR_BYTE;
    case BUILTIN_TYPE_DOUBLE:     return EXPR_DOUBLE;
    case BUILTIN_TYPE_FLOAT:      return EXPR_FLOAT;
    case BUILTIN_TYPE_INT:        return EXPR_INT;
    case BUILTIN_TYPE_LONG:       return EXPR_LONG;
    case BUILTIN_TYPE_SHORT:      return EXPR_SHORT;
    case BUILTIN_TYPE_SUCCESSFUL: return EXPR_BOOLEAN;
    case BUILTIN_TYPE_UBYTE:      return EXPR_UBYTE;
    case BUILTIN_TYPE_UINT:       return EXPR_UINT;
    case BUILTIN_TYPE_ULONG:      return EXPR_ULONG;
    case BUILTIN_TYPE_USHORT:     return EXPR_USHORT;
    case BUILTIN_TYPE_USIZE:      return EXPR_USIZE;
    case BUILTIN_TYPE_NONE:       return EXPR_NONE;
    }

    return EXPR_NONE; // Should never be reached
}

errorcode_t infer_type(infer_ctx_t *ctx, ast_type_t *type){
    // NOTE: Expands 'type' by resolving any aliases

    ast_alias_t *aliases = ctx->ast->aliases;
    length_t aliases_length = ctx->ast->aliases_length;

    ast_elem_t **new_elements = NULL;
    length_t length = 0;
    length_t capacity = 0;
    maybe_null_strong_cstr_t maybe_alias_name = NULL;

    for(length_t e = 0; e != type->elements_length; e++){
        ast_elem_t *elem = type->elements[e];

        switch(elem->id){
        case AST_ELEM_BASE: {
                int alias_index = find_alias(aliases, aliases_length, ((ast_elem_base_t*) elem)->base);
                if(alias_index != -1){
                    // NOTE: The alias target type was already resolved of any aliases,
                    //       so we don't have to scan the new elements
                    ast_type_t cloned = ast_type_clone(&aliases[alias_index].type);
                    expand((void**) &new_elements, sizeof(ast_elem_t*), length, &capacity, cloned.elements_length, 4);

                    // Move all the elements from the cloned type to this type
                    for(length_t m = 0; m != cloned.elements_length; m++){
                        new_elements[length++] = cloned.elements[m];
                    }

                    // DANGEROUS: Manually (partially) deleting ast_elem_base_t
                    maybe_alias_name = ((ast_elem_base_t*) elem)->base;
                    free(elem);

                    free(cloned.elements);
                    continue; // Don't do normal stuff that follows
                }
            }
            break;
        case AST_ELEM_FUNC:
            for(length_t a = 0; a != ((ast_elem_func_t*) elem)->arity; a++){
                if(infer_type(ctx, &((ast_elem_func_t*) elem)->arg_types[a])) return FAILURE;
            }
            if(infer_type(ctx, ((ast_elem_func_t*) elem)->return_type)) return FAILURE;
            break;
        case AST_ELEM_GENERIC_BASE:
            for(length_t a = 0; a != ((ast_elem_generic_base_t*) elem)->generics_length; a++){
                if(infer_type(ctx, &((ast_elem_generic_base_t*) elem)->generics[a])) return FAILURE;
            }
            break;
        }

        // If not an alias, continue on as usual
        expand((void**) &new_elements, sizeof(ast_elem_t*), length, &capacity, 1, 4);
        new_elements[length++] = elem;
    }

    // Overwrite changes
    free(type->elements);
    type->elements = new_elements;
    type->elements_length = length;
    type_table_give(ctx->type_table, type, maybe_alias_name);
    return SUCCESS;
}

void infer_var_scope_init(infer_var_scope_t *out_scope, infer_var_scope_t *parent){
    out_scope->parent = parent;
    out_scope->list.variables = NULL;
    out_scope->list.length = 0;
    out_scope->list.capacity = 0;
}

void infer_var_scope_free(infer_var_scope_t *scope){
    free(scope->list.variables);
}

void infer_var_scope_push(infer_var_scope_t **scope){
    infer_var_scope_t *new_scope = malloc(sizeof(infer_var_scope_t));
    infer_var_scope_init(new_scope, *scope);
    *scope = new_scope;
} 

void infer_var_scope_pop(infer_var_scope_t **scope){
    if((*scope)->parent == NULL){
        redprintf("INTERNAL ERROR: TRIED TO POP INFER VARIABLE SCOPE WITH NO PARENT\n");
        return;
    }

    infer_var_scope_t *parent = (*scope)->parent;
    infer_var_scope_free(*scope);
    free(*scope);

    *scope = parent;
}

infer_var_t* infer_var_scope_find(infer_var_scope_t *scope, const char *name){
    for(length_t i = 0; i != scope->list.length; i++){
        if(strcmp(scope->list.variables[i].name, name) == 0){
            return &scope->list.variables[i];
        }
    }

    if(scope->parent){
        return infer_var_scope_find(scope->parent, name);
    } else {
        return NULL;
    }
}

void infer_var_scope_add_variable(infer_var_scope_t *scope, weak_cstr_t name, ast_type_t *type){
    infer_var_list_t *list = &scope->list;
    expand((void**) &list->variables, sizeof(infer_var_t), list->length, &list->capacity, 1, 4);

    infer_var_t *var = &list->variables[list->length++];
    var->name = name;
    var->type = type;
}

const char* infer_var_scope_nearest(infer_var_scope_t *scope, const char *name){
    char *nearest_name = NULL;
    infer_var_scope_nearest_inner(scope, name, &nearest_name, NULL);
    return nearest_name;
}

void infer_var_scope_nearest_inner(infer_var_scope_t *scope, const char *name, char **out_nearest_name, int *out_distance){
    // NOTE: out_nearest_name must be a valid pointer
    // NOTE: out_distance may be NULL

    char *scope_name = NULL, *parent_name = NULL;
    int scope_distance = -1, parent_distance = -1;

    infer_var_list_nearest(&scope->list, name, &scope_name, &scope_distance);

    if(scope->parent == NULL){
        *out_nearest_name = scope_name;
        if(out_distance) *out_distance = scope_distance;
        return;
    }

    infer_var_list_nearest(&scope->parent->list, name, &parent_name, &parent_distance);

    if(scope_distance <= parent_distance){
        *out_nearest_name = scope_name;
        if(out_distance) *out_distance = scope_distance;
    } else {
        *out_nearest_name = parent_name;
        if(out_distance) *out_distance = parent_distance;
    }
}

void infer_var_list_nearest(infer_var_list_t *list, const char *name, char **out_nearest_name, int *out_distance){
    // NOTE: out_nearest_name must be a valid pointer
    // NOTE: out_distance may be NULL

    // Default to no nearest value
    *out_nearest_name = NULL;
    if(out_distance) *out_distance = -1;

    // Stack array to contain all of the distances
    // NOTE: This may be bad if the length of the variable list is really long
    length_t list_length = list->length;
    int distances[list_length];

    // Calculate distance for every variable name
    for(length_t i = 0; i != list_length; i++){
        distances[i] = levenshtein(name, list->variables[i].name);
    }

    // Minimum number of changes to override NULL
    length_t minimum = 3;

    // Find the name with the shortest distance
    for(length_t i = 0; i != list_length; i++){
        if(distances[i] < minimum){
            minimum = distances[i];
            *out_nearest_name = list->variables[i].name;
        }
    }

    // Output minimum distance if a name close enough was found
    if(out_distance && *out_nearest_name) *out_distance = minimum;
}

void infer_mention_expression_literal_type(infer_ctx_t *ctx, unsigned int expression_literal_id){
    // HACK: Mention types created through primative data to the type table
    // (since with the current system, we can't know what types we will need very well)
    // TODO: Change the system so the resolution of rtti is further delayed
    // - Isaac Shelton, January 20th 2019
    switch(expression_literal_id){
    case EXPR_NONE:
    case EXPR_NULL:
        // The type would've had already been mentioned to the type table,
        // so we don't have to worry about it
        break;
    case EXPR_BOOLEAN:
        type_table_give_base(ctx->type_table, "bool");
        break;
    case EXPR_BYTE:
        type_table_give_base(ctx->type_table, "byte");
        break;
    case EXPR_UBYTE:
        type_table_give_base(ctx->type_table, "ubyte");
        break;
    case EXPR_SHORT:
        type_table_give_base(ctx->type_table, "short");
        break;
    case EXPR_USHORT:
        type_table_give_base(ctx->type_table, "ushort");
        break;
    case EXPR_INT:
        type_table_give_base(ctx->type_table, "int");
        break;
    case EXPR_UINT:
        type_table_give_base(ctx->type_table, "uint");
        break;
    case EXPR_LONG:
        type_table_give_base(ctx->type_table, "long");
        break;
    case EXPR_ULONG:
        type_table_give_base(ctx->type_table, "ulong");
        break;
    case EXPR_USIZE:
        type_table_give_base(ctx->type_table, "usize");
        break;
    case EXPR_DOUBLE:
        type_table_give_base(ctx->type_table, "double");
        break;
    case EXPR_FLOAT:
        type_table_give_base(ctx->type_table, "float");
        break;
    case EXPR_CSTR: {
            // Create '*ubyte' type template for cloning
            // ------------------------------
            ast_elem_pointer_t ptr_elem;
            ptr_elem.id = AST_ELEM_POINTER;
            ptr_elem.source = NULL_SOURCE;

            ast_elem_base_t cstr_base_elem;
            cstr_base_elem.id = AST_ELEM_BASE;
            cstr_base_elem.source = NULL_SOURCE;
            cstr_base_elem.base = "ubyte";

            ast_elem_t *cstr_type_elements[] = {
                (ast_elem_t*) &ptr_elem,
                (ast_elem_t*) &cstr_base_elem
            };

            ast_type_t cstr_type;
            cstr_type.elements = cstr_type_elements;
            cstr_type.elements_length = 2;
            cstr_type.source = NULL_SOURCE;
            // ------------------------------

            type_table_give(ctx->type_table, &cstr_type, NULL);
        }
        break;
    case EXPR_STR:
        type_table_give_base(ctx->type_table, "String");
        break;
    default:
        redprintf("INTERNAL ERROR: Failed to mention type to type table for primitive from generic!\n");
        redprintf("Continuing regardless...\n");
    }
}
