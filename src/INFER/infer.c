
#include "INFER/infer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "BRIDGE/type_table.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/levenshtein.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

errorcode_t infer(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;

    if(compiler->extract_import_order){
        for(length_t i = 0; i < compiler->objects_length; i++){
            printf("%s\n", compiler->objects[i]->full_filename);
        }
        compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        return ALT_FAILURE;
    }

    infer_ctx_t ctx = (infer_ctx_t){
        .compiler = compiler,
        .object = object,
        .ast = ast,
        .constants_recursion_depth = 0,
        .aliases_recursion_depth = 0,
        .scope = NULL,
    };

    ast->type_table = malloc(sizeof(type_table_t));
    type_table_init(ast->type_table);
    ctx.type_table = ast->type_table;

    // Sort aliases and constants so we can binary search on them later
    qsort(ast->aliases, ast->aliases_length, sizeof(ast_alias_t), ast_aliases_cmp);
    qsort(ast->constants, ast->constants_length, sizeof(ast_constant_t), ast_constants_cmp);
    qsort(ast->enums, ast->enums_length, sizeof(ast_enum_t), ast_enums_cmp);
    qsort(ast->globals, ast->globals_length, sizeof(ast_global_t), ast_globals_cmp);

    for(length_t i = 0; i != ast->composites_length; i++){
        ast_composite_t *composite = &ast->composites[i];

        if(infer_layout_skeleton(&ctx, &composite->layout.skeleton)) return FAILURE;

        type_table_give_base(ctx.type_table, composite->name);
    }

    for(length_t i = 0; i != ast->globals_length; i++){
        if(infer_type(&ctx, &ast->globals[i].type)) return FAILURE;
        ast_expr_t **global_initial = &ast->globals[i].initial;
        if(*global_initial != NULL){
            unsigned int default_primitive = ast_primitive_from_ast_type(&ast->globals[i].type);
            if(infer_expr(&ctx, NULL, global_initial, default_primitive, false)) return FAILURE;
        }
    }

    if(infer_in_funcs(&ctx, ast->funcs, ast->funcs_length)) return FAILURE;

    ast->type_table = ctx.type_table;

    // We had unused variables and have been told to treat them as errors, so exit
    if(compiler->traits & COMPILER_WARN_AS_ERROR && compiler->show_unused_variables_how_to_disable){
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t infer_layout_skeleton(infer_ctx_t *ctx, ast_layout_skeleton_t *skeleton){
    for(length_t i = 0; i < skeleton->bones_length; i++){
        ast_layout_bone_t *bone = &skeleton->bones[i];

        switch(bone->kind){
        case AST_LAYOUT_BONE_KIND_UNION:
        case AST_LAYOUT_BONE_KIND_STRUCT:
            if(infer_layout_skeleton(ctx, &bone->children)) return FAILURE;
            break;
        case AST_LAYOUT_BONE_KIND_TYPE:
            if(infer_type(ctx, &bone->type)) return FAILURE;
            break;
        default:
            die("infer_layout_skeleton() - Unrecognized bone kind %d\n", (int) bone->kind);
        }
    }

    return SUCCESS;
}

errorcode_t infer_in_funcs(infer_ctx_t *ctx, ast_func_t *funcs, length_t funcs_length){
    infer_var_scope_t indirect_func_scope_storage;
    infer_var_scope_t *previous_scope = ctx->scope;
    bool variadic_functions_are_allowed = ctx->object->ast.common.ast_variadic_array != NULL;

    for(length_t f = 0; f != funcs_length; f++){
        ast_func_t *function = &funcs[f];

        // If the function is variadic, ensure that variadic functions are allowed
        if(function->traits & AST_FUNC_VARIADIC && !variadic_functions_are_allowed){
            compiler_panic(ctx->compiler, function->source, "In order to use variadic functions, __variadic_array__ must be defined");
            printf("\nTry importing '%s/VariadicArray.adept'\n", ADEPT_VERSION_STRING);
            return FAILURE;
        }

        // Create inference variable scope for function
        ctx->scope = &indirect_func_scope_storage;
        infer_var_scope_init(ctx->scope, NULL);
        
        // Resolve aliases in function return type
        if(infer_type(ctx, &function->return_type)){
            ctx->scope = previous_scope;
            return FAILURE;
        }

        // Resolve aliases in function arguments
        for(length_t a = 0; a != function->arity; a++){
            if(infer_type(ctx, &function->arg_types[a])) {
                ctx->scope = previous_scope;
                return FAILURE;
            }

            if(function->arg_defaults && function->arg_defaults[a]){
                unsigned int default_primitive = ast_primitive_from_ast_type(&funcs[f].arg_types[a]);
                if(infer_expr(ctx, function, &function->arg_defaults[a], default_primitive, false)){
                    infer_var_scope_free(ctx->compiler, ctx->scope);
                    ctx->scope = previous_scope;
                    return FAILURE;
                }
            }

            if(!(function->traits & AST_FUNC_FOREIGN)){
                const bool force_used =
                    ctx->compiler->ignore & COMPILER_IGNORE_UNUSED
                    || function->traits & (AST_FUNC_MAIN | AST_FUNC_DISALLOW | AST_FUNC_DISPATCHER)
                    || (a == 0 && streq(function->arg_names[a], "this"));
                
                infer_var_scope_add_variable(ctx->scope, function->arg_names[a], &function->arg_types[a], function->arg_sources[a], force_used, false);
            }
        }

        if(function->traits & AST_FUNC_VARIADIC){
            // Add variadic array variable
            infer_var_scope_add_variable(ctx->scope, function->variadic_arg_name, ctx->ast->common.ast_variadic_array, function->variadic_source, false, false);
        }
        
        // Infer expressions in statements
        if(infer_in_stmts(ctx, function, &function->statements)){
            infer_var_scope_free(ctx->compiler, ctx->scope);
            ctx->scope = previous_scope;
            return FAILURE;
        }

        infer_var_scope_free(ctx->compiler, ctx->scope);
        ctx->scope = previous_scope;
    }

    return SUCCESS;
}

errorcode_t infer_in_stmts(infer_ctx_t *ctx, ast_func_t *func, ast_expr_list_t *stmt_list){
    for(length_t s = 0; s != stmt_list->length; s++){
        ast_expr_t *stmt = stmt_list->statements[s];

        switch(stmt->id){
        case EXPR_RETURN: {
                ast_expr_return_t *return_stmt = (ast_expr_return_t*) stmt;
                if(return_stmt->value != NULL && infer_expr(ctx, func, &return_stmt->value, ast_primitive_from_ast_type(&func->return_type), false)) return FAILURE;
                if(infer_in_stmts(ctx, func, &return_stmt->last_minute)) return FAILURE;
            }
            break;
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = (ast_expr_call_t*) stmt;

                // HACK: Mark a variable as used if a call is made to a function with the same name
                // SPEED: PERFORMANCE: This is probably really slow to do
                // TODO: Clean up and/or speed up this code
                if(!(ctx->compiler->ignore & COMPILER_IGNORE_UNUSED || ctx->compiler->traits & COMPILER_NO_WARN) && ctx->scope != NULL){
                    infer_var_t *func_variable = infer_var_scope_find(ctx->scope, call_stmt->name);
                    if(func_variable) func_variable->used = true;
                }
                
                for(length_t i = 0; i != call_stmt->arity; i++){
                    if(infer_expr(ctx, func, &call_stmt->args[i], EXPR_NONE, false)) return FAILURE;
                }

                if(call_stmt->gives.elements_length != 0){
                    if(infer_type(ctx, &call_stmt->gives)) return FAILURE;
                }
            }
            break;
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) stmt;

                if(infer_type(ctx, &declare_stmt->type)) return FAILURE;

                if(declare_stmt->value != NULL){
                    if(infer_expr(ctx, func, &declare_stmt->value, ast_primitive_from_ast_type(&declare_stmt->type), false)) return FAILURE;
                }

                if(declare_stmt->inputs.has){
                    for(length_t i = 0; i != declare_stmt->inputs.value.length; i++){
                        if(infer_expr(ctx, func, &declare_stmt->inputs.value.statements[i], EXPR_NONE, false)) return FAILURE;
                    }
                }

                infer_var_scope_add_variable(ctx->scope, declare_stmt->name, &declare_stmt->type, declare_stmt->source, false, declare_stmt->traits & AST_EXPR_DECLARATION_CONST);
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
        case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
        case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN: case EXPR_RSHIFT_ASSIGN:
        case EXPR_LGC_LSHIFT_ASSIGN: case EXPR_LGC_RSHIFT_ASSIGN: {
                ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) stmt;
                if(infer_expr(ctx, func, &assign_stmt->destination, EXPR_NONE, true)) return FAILURE;
                if(infer_expr(ctx, func, &assign_stmt->value, EXPR_NONE, false)) return FAILURE;
            }
            break;
        case EXPR_CONDITIONLESS_BLOCK: {
                assert(ctx->scope);

                ast_expr_conditionless_block_t *block = (ast_expr_conditionless_block_t*) stmt;

                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &block->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
                assert(ctx->scope);

                ast_expr_if_t *conditional = (ast_expr_if_t*) stmt;
                if(infer_expr(ctx, func, &conditional->value, EXPR_NONE, false)) return FAILURE;

                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &conditional->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                assert(ctx->scope);

                ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) stmt;
                if(infer_expr(ctx, func, &complex_conditional->value, EXPR_NONE, false)) return FAILURE;

                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &complex_conditional->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &complex_conditional->else_statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                assert(ctx->scope);

                ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) stmt;

                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &conditional->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) stmt;
                if(infer_expr(ctx, func, &call_stmt->value, EXPR_NONE, true)) return FAILURE;

                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(ctx, func, &call_stmt->args[a], EXPR_NONE, false)) return FAILURE;
                }

                if(call_stmt->gives.elements_length != 0){
                    if(infer_type(ctx, &call_stmt->gives)) return FAILURE;
                }
            }
            break;
        case EXPR_DELETE: case EXPR_VA_START: case EXPR_VA_END: {
                ast_expr_unary_t *unary_stmt = (ast_expr_unary_t*) stmt;
                if(infer_expr(ctx, func, &unary_stmt->value, EXPR_NONE, false)) return FAILURE;
            }
            break;
        case EXPR_EACH_IN: {
                assert(ctx->scope);

                ast_expr_each_in_t *loop = (ast_expr_each_in_t*) stmt;
                if(infer_type(ctx, loop->it_type)) return FAILURE;
                
                if(loop->low_array && infer_expr(ctx, func, &loop->low_array, EXPR_NONE, true)) return FAILURE;
                if(loop->length    && infer_expr(ctx, func, &loop->length, EXPR_USIZE, false))  return FAILURE;
                if(loop->list      && infer_expr(ctx, func, &loop->list, EXPR_USIZE, true))     return FAILURE;
 
                infer_var_scope_push(&ctx->scope);
                infer_var_scope_add_variable(ctx->scope, "idx", &ctx->ast->common.ast_usize_type, loop->source, true, false);
                infer_var_scope_add_variable(ctx->scope, loop->it_name ? loop->it_name : "it", loop->it_type, loop->source, true, false);

                if(infer_in_stmts(ctx, func, &loop->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }
                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_REPEAT: {
                assert(ctx->scope);

                ast_expr_repeat_t *loop = (ast_expr_repeat_t*) stmt;
                if(infer_expr(ctx, func, &loop->limit, EXPR_USIZE, false)) return FAILURE;
 
                infer_var_scope_push(&ctx->scope);
                infer_var_scope_add_variable(ctx->scope, loop->idx_overload_name ? loop->idx_overload_name : "idx", &ctx->ast->common.ast_usize_type, loop->source, true, false);

                if(infer_in_stmts(ctx, func, &loop->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }

                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_SWITCH: {
                assert(ctx->scope);

                ast_expr_switch_t *expr_switch = (ast_expr_switch_t*) stmt;
                if(infer_expr(ctx, func, &expr_switch->value, EXPR_NONE, false)) return FAILURE;

                for(length_t i = 0; i != expr_switch->cases.length; i++){
                    ast_case_t *expr_case = &expr_switch->cases.cases[i];

                    if(infer_expr(ctx, func, &expr_case->condition, EXPR_NONE, false)) return FAILURE;

                    infer_var_scope_push(&ctx->scope);
                    if(infer_in_stmts(ctx, func, &expr_case->statements)){
                        infer_var_scope_pop(ctx->compiler, &ctx->scope);
                        return FAILURE;
                    }
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                }

                infer_var_scope_push(&ctx->scope);
                if(infer_in_stmts(ctx, func, &expr_switch->or_default)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }

                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_VA_COPY: {
                ast_expr_va_copy_t *va_copy_stmt = (ast_expr_va_copy_t*) stmt;
                if(infer_expr(ctx, func, &va_copy_stmt->src_value, EXPR_NONE, false)) return FAILURE;
                if(infer_expr(ctx, func, &va_copy_stmt->dest_value, EXPR_NONE, true)) return FAILURE;
            }
            break;
        case EXPR_FOR: {
                assert(ctx->scope);

                ast_expr_for_t *loop = (ast_expr_for_t*) stmt;
                infer_var_scope_push(&ctx->scope);

                if(infer_in_stmts(ctx, func, &loop->before)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }

                if(infer_in_stmts(ctx, func, &loop->after)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }

                if(loop->condition && infer_expr(ctx, func, &loop->condition, EXPR_NONE, false)) return FAILURE;

                if(infer_in_stmts(ctx, func, &loop->statements)){
                    infer_var_scope_pop(ctx->compiler, &ctx->scope);
                    return FAILURE;
                }

                infer_var_scope_pop(ctx->compiler, &ctx->scope);
            }
            break;
        case EXPR_DECLARE_CONSTANT: {
                assert(ctx->scope);

                ast_expr_declare_constant_t *declare_constant_stmt = (ast_expr_declare_constant_t*) stmt;

                infer_var_scope_add_constant(ctx->scope, ast_constant_clone(&declare_constant_stmt->constant));
            }
            break;
        case EXPR_PREINCREMENT:
        case EXPR_PREDECREMENT:
        case EXPR_POSTDECREMENT:
        case EXPR_POSTINCREMENT:
        case EXPR_TOGGLE:
            if(infer_expr(ctx, func, &((ast_expr_unary_t*) stmt)->value, EXPR_NONE, true)) return FAILURE;
            break;
        case EXPR_LLVM_ASM: {
                ast_expr_llvm_asm_t *llvm_asm_stmt = (ast_expr_llvm_asm_t*) stmt;

                for(length_t a = 0; a != llvm_asm_stmt->arity; a++){
                    if(infer_expr(ctx, func, &llvm_asm_stmt->args[a], EXPR_NONE, false)) return FAILURE;
                }
            }
            break;
        default: break;
            // Ignore this statement, it doesn't contain any expressions that we need to worry about
        }
    }
    return SUCCESS;
}

errorcode_t infer_expr(infer_ctx_t *ctx, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type, bool must_be_mutable){
    // NOTE: The 'ast_expr_t*' pointed to by 'root' may be written to
    // NOTE: 'default_assigned_type' is used as a default assigned type if generics aren't resolved within the expression
    // NOTE: if 'default_assigned_type' is EXPR_NONE, then the most suitable type for the given generics is chosen
    // NOTE: 'ast_func' can be NULL

    undetermined_expr_list_t undetermined = {
        .expressions = malloc(sizeof(ast_expr_t*) * 4),
        .expressions_length = 0,
        .expressions_capacity = 4,
        .solution = EXPR_NONE,
    };

    length_t previous_constants_recursion_depth = ctx->constants_recursion_depth;
    ctx->constants_recursion_depth = 0;

    if(infer_expr_inner(ctx, ast_func, root, &undetermined, must_be_mutable)){
        ctx->constants_recursion_depth = previous_constants_recursion_depth;
        free(undetermined.expressions);
        return FAILURE;
    }

    ctx->constants_recursion_depth = previous_constants_recursion_depth;

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

errorcode_t infer_expr_inner(infer_ctx_t *ctx, ast_func_t *ast_func, ast_expr_t **expr, undetermined_expr_list_t *undetermined, bool must_be_mutable){
    // NOTE: 'ast_func' can be NULL
    // NOTE: The 'ast_expr_t*' pointed to by 'expr' may be written to

    ast_expr_t **a, **b;

    // Expand list if needed
    expand((void**) &undetermined->expressions, sizeof(ast_expr_t*), undetermined->expressions_length,
        &undetermined->expressions_capacity, 1, 2);

    // TODO: CLEANUP: Clean up this code
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
    case EXPR_POLYCOUNT:
        if(undetermined_expr_list_give_solution(ctx, undetermined, EXPR_USIZE)) return FAILURE;
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
    case EXPR_VARIABLE:
        if(infer_expr_inner_variable(ctx, ast_func, expr, undetermined, must_be_mutable)) return FAILURE;
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
        if(infer_expr_inner(ctx, ast_func, a, undetermined, false)) return FAILURE;
        if(infer_expr_inner(ctx, ast_func, b, undetermined, false)) return FAILURE;
        break;
    case EXPR_EQUALS:
    case EXPR_NOTEQUALS:
    case EXPR_GREATER:
    case EXPR_LESSER:
    case EXPR_GREATEREQ:
    case EXPR_LESSEREQ: {
            undetermined_expr_list_t local_undetermined = {
                .expressions = malloc(sizeof(ast_expr_t*) * 4),
                .expressions_length = 0,
                .expressions_capacity = 4,
                .solution = EXPR_NONE,
            };

            // Group inference for two child expressions
            a = &((ast_expr_math_t*) *expr)->a;
            b = &((ast_expr_math_t*) *expr)->b;
            if(infer_expr_inner(ctx, ast_func, a, &local_undetermined, false) || infer_expr_inner(ctx, ast_func, b, &local_undetermined, false)){
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
        if(infer_expr(ctx, ast_func, a, EXPR_NONE, false)) return FAILURE;
        if(infer_expr(ctx, ast_func, b, EXPR_NONE, false)) return FAILURE;
        break;
    case EXPR_CALL: {
            // HACK: Mark a variable as used if a call is made to a function with the same name
            // SPEED: PERFORMANCE: This is probably really slow to do
            // TODO: Clean up and/or speed up this code
            if(!(ctx->compiler->ignore & COMPILER_IGNORE_UNUSED || ctx->compiler->traits & COMPILER_NO_WARN) && ctx->scope != NULL){
                infer_var_t *variable = infer_var_scope_find(ctx->scope, ((ast_expr_call_t*) *expr)->name);

                if(variable){
                    variable->used = true;
                }
            }

            ast_expr_call_t *call_expr = (ast_expr_call_t*) *expr;
            
            for(length_t i = 0; i != call_expr->arity; i++){
                if(infer_expr(ctx, ast_func, &call_expr->args[i], EXPR_NONE, false)) return FAILURE;
            }

            if(infer_type(ctx, &call_expr->gives)) return FAILURE;
        }
        break;
    case EXPR_GENERIC_INT:
    case EXPR_GENERIC_FLOAT:
        if(undetermined_expr_list_give_generic(ctx, undetermined, expr)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(infer_expr(ctx, ast_func, &((ast_expr_member_t*) *expr)->value, EXPR_NONE, must_be_mutable)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) *expr;

            if(func_addr->match_args != NULL){
                for(length_t i = 0; i != func_addr->match_args_length; i++){
                    if(infer_type(ctx, &func_addr->match_args[i])) return FAILURE;
                }
            }
        }
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
        if(infer_expr(ctx, ast_func, &((ast_expr_unary_t*) *expr)->value, EXPR_NONE, true)) return FAILURE;
        break;
    case EXPR_NULL:
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        if(infer_expr(ctx, ast_func, &((ast_expr_array_access_t*) *expr)->index, EXPR_NONE, false)) return FAILURE;
        if(infer_expr(ctx, ast_func, &((ast_expr_array_access_t*) *expr)->value, EXPR_NONE, must_be_mutable)) return FAILURE;
        break;
    case EXPR_CAST:
        if(infer_type(ctx, &((ast_expr_cast_t*) *expr)->to)) return FAILURE;
        if(infer_expr(ctx, ast_func, &((ast_expr_cast_t*) *expr)->from, EXPR_NONE, false)) return FAILURE;
        break;
    case EXPR_SIZEOF:
        if(infer_type(ctx, &((ast_expr_sizeof_t*) *expr)->type)) return FAILURE;
        break;
    case EXPR_SIZEOF_VALUE:
        if(infer_expr(ctx, ast_func, &((ast_expr_sizeof_value_t*) *expr)->value, EXPR_NONE, false)) return FAILURE;
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_method_expr = (ast_expr_call_method_t*) *expr;

            if(infer_expr_inner(ctx, ast_func, &call_method_expr->value, undetermined, true)) return FAILURE;

            for(length_t i = 0; i != call_method_expr->arity; i++){
                if(infer_expr(ctx, ast_func, &call_method_expr->args[i], EXPR_NONE, false)) return FAILURE;
            }

            if(infer_type(ctx, &call_method_expr->gives)) return FAILURE;
        }
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
        if(infer_expr_inner(ctx, ast_func, &((ast_expr_unary_t*) *expr)->value, undetermined, false)) return FAILURE;
        break;
    case EXPR_PREINCREMENT:
    case EXPR_PREDECREMENT:
    case EXPR_POSTDECREMENT:
    case EXPR_POSTINCREMENT:
    case EXPR_TOGGLE:
        if(infer_expr_inner(ctx, ast_func, &((ast_expr_unary_t*) *expr)->value, undetermined, true)) return FAILURE;
        break;
    case EXPR_NEW:
        if(infer_type(ctx, &((ast_expr_new_t*) *expr)->type)) return FAILURE;
        if(((ast_expr_new_t*) *expr)->amount != NULL && infer_expr(ctx, ast_func, &(((ast_expr_new_t*) *expr)->amount), EXPR_NONE, false)) return FAILURE;
        
        if(((ast_expr_new_t*) *expr)->inputs.has){
            for(length_t i = 0; i != ((ast_expr_new_t*) *expr)->inputs.value.length; i++){
                if(infer_expr(ctx, ast_func, &((ast_expr_new_t*) *expr)->inputs.value.expressions[i], EXPR_NONE, false)) return FAILURE;
            }
        }
        break;
    case EXPR_NEW_CSTRING:
    case EXPR_ENUM_VALUE:
        break;
    case EXPR_STATIC_ARRAY: {
            if(infer_type(ctx, &((ast_expr_static_data_t*) *expr)->type)) return FAILURE;

            unsigned int default_primitive = ast_primitive_from_ast_type(&((ast_expr_static_data_t*) *expr)->type);

            for(length_t i = 0; i != ((ast_expr_static_data_t*) *expr)->length; i++){
                if(infer_expr(ctx, ast_func, &((ast_expr_static_data_t*) *expr)->values[i], default_primitive, false)) return FAILURE;
            }
        }
        break;
    case EXPR_STATIC_STRUCT: {
            ast_expr_static_data_t *static_data = (ast_expr_static_data_t*) *expr;
            if(infer_type(ctx, &static_data->type)) return FAILURE;

            if(static_data->type.elements_length != 1 || static_data->type.elements[0]->id != AST_ELEM_BASE){
                strong_cstr_t s = ast_type_str(&static_data->type);
                compiler_panicf(ctx->compiler, static_data->type.source, "Can't create struct literal for non-struct type '%s'\n", s);
                free(s);
                return FAILURE;
            }

            const char *base = ((ast_elem_base_t*) static_data->type.elements[0])->base;
            ast_composite_t *composite = ast_composite_find_exact(ctx->ast, base);

            if(composite == NULL){
                if(typename_is_extended_builtin_type(base)){
                    compiler_panicf(ctx->compiler, static_data->type.source, "Can't create struct literal for built-in type '%s'", base);
                } else {
                    compiler_panicf(ctx->compiler, static_data->type.source, "Struct '%s' does not exist", base);
                }
                return FAILURE;
            }

            if(composite->layout.kind != AST_LAYOUT_STRUCT || !composite->layout.field_map.is_simple){
                compiler_panicf(ctx->compiler, static_data->type.source, "Can't create struct literal for complex composite type '%s'", base);
                return FAILURE;
            }

            length_t field_count = ast_simple_field_map_get_count(&composite->layout.field_map);

            if(static_data->length != field_count){
                if(static_data->length > field_count){
                    compiler_panicf(ctx->compiler, static_data->type.source, "Too many fields specified for struct '%s' (expected %d, got %d)", base, field_count, static_data->length);
                } else {
                    compiler_panicf(ctx->compiler, static_data->type.source, "Not enough fields specified for struct '%s' (expected %d, got %d)", base, field_count, static_data->length);
                }
                return FAILURE;
            }

            for(length_t i = 0; i != ((ast_expr_static_data_t*) *expr)->length; i++){
                ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&composite->layout.skeleton, i);

                unsigned int default_primitive = ast_primitive_from_ast_type(field_type);

                if(infer_expr(ctx, ast_func, &((ast_expr_static_data_t*) *expr)->values[i], default_primitive, false)) return FAILURE;
            }
        }
        break;
    case EXPR_TYPEINFO: {
            // Don't infer target type, but give it to the type table
            type_table_give(ctx->type_table, &(((ast_expr_typeinfo_t*) *expr)->type), NULL);
        }
        break;
    case EXPR_TERNARY: {
            if(infer_expr(ctx, ast_func, &((ast_expr_ternary_t*) *expr)->condition, EXPR_NONE, false)) return FAILURE;

            undetermined_expr_list_t local_undetermined;
            local_undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
            local_undetermined.expressions_length = 0;
            local_undetermined.expressions_capacity = 4;
            local_undetermined.solution = EXPR_NONE;

            // Group inference for two child expressions
            a = &((ast_expr_ternary_t*) *expr)->if_true;
            b = &((ast_expr_ternary_t*) *expr)->if_false;
            if(infer_expr_inner(ctx, ast_func, a, &local_undetermined, false) || infer_expr_inner(ctx, ast_func, b, &local_undetermined, false)){
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
            assert(ctx->scope);

            ast_expr_inline_declare_t *def = (ast_expr_inline_declare_t*) *expr;
            if(infer_type(ctx, &def->type)) return FAILURE;

            if(def->value != NULL && infer_expr(ctx, ast_func, &def->value, ast_primitive_from_ast_type(&def->type), false) == FAILURE){
                return FAILURE;
            }

            infer_var_scope_add_variable(ctx->scope, def->name, &def->type, def->source, true, false);
        }
        break;
    case EXPR_VA_ARG: {
            ast_expr_va_arg_t *va = (ast_expr_va_arg_t*) *expr;

            if(infer_expr(ctx, ast_func, &va->va_list, EXPR_NONE, true)) return FAILURE;
            if(infer_type(ctx, &va->arg_type)) return FAILURE;
        }
        break;
    case EXPR_TYPENAMEOF: {
            ast_expr_typenameof_t *typenameof = (ast_expr_typenameof_t*) *expr;
            if(infer_type(ctx, &typenameof->type)) return FAILURE;
        }
        break;
    case EXPR_EMBED:
        // Nothing to infer
        break;
    case EXPR_ALIGNOF:
        if(infer_type(ctx, &((ast_expr_alignof_t*) *expr)->type)) return FAILURE;
        break;
    case EXPR_INITLIST: {
            ast_expr_initlist_t *initlist = (ast_expr_initlist_t*) *expr;
            
            for(length_t i = 0; i != initlist->length; i++){
                if(infer_expr(ctx, ast_func, &initlist->elements[i], EXPR_NONE, false)) return FAILURE;
            }
        }
        break;
    default:
        compiler_panicf(ctx->compiler, (*expr)->source, "INTERNAL ERROR: Unimplemented expression type 0x%08X inside infer_expr_inner", (*expr)->id);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t infer_expr_inner_variable(infer_ctx_t *ctx, ast_func_t *ast_func, ast_expr_t **expr, undetermined_expr_list_t *undetermined, bool must_be_mutable){

    // Setup for searching
    char *variable_name = ((ast_expr_variable_t*) *expr)->name;
    ast_type_t *variable_type;
    unsigned int var_expr_primitive;

    // Search in local variables scope first
    infer_var_t *local_variable = ctx->scope ? infer_var_scope_find(ctx->scope, variable_name) : NULL;
    
    if(local_variable != NULL){
        variable_type = local_variable->type;
        local_variable->used = true;

        if(local_variable->is_const && must_be_mutable){
            // Couldn't find identifier
            compiler_panicf(ctx->compiler, (*expr)->source, "Variable '%s' is constant, and cannot be used as a mutable value", variable_name);
            ctx->compiler->ignore |= COMPILER_IGNORE_UNUSED;
            return FAILURE;
        }

        goto found_variable;
    }

    // Search in global variables scope
    ast_t *ast = ctx->ast;
    maybe_index_t global_variable_index = ast_find_global(ast->globals, ast->globals_length, variable_name);

    if(global_variable_index != -1){
        variable_type = &ast->globals[global_variable_index].type;
        goto found_variable;
    }

    // Search in local constants scope
    ast_constant_t *constant = ctx->scope ? infer_var_scope_find_constant(ctx->scope, variable_name) : NULL;
    if(constant) goto found_constant;
    
    // Search in global constants scope
    maybe_index_t constant_index = ast_find_constant(ast->constants, ast->constants_length, variable_name);

    if(constant_index != -1){
        constant = &ast->constants[constant_index];
        goto found_constant;
    }

    // Couldn't find identifier
    compiler_panicf(ctx->compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
    const char *nearest = ctx->scope ? infer_var_scope_nearest(ctx->scope, variable_name) : NULL;
    if(nearest) printf("\nDid you mean '%s'?\n", nearest);
    return FAILURE;

found_constant:
    // Constant does exist, substitute it's value

    if(ctx->constants_recursion_depth++ >= 100){
        compiler_panicf(ctx->compiler, (*expr)->source, "Recursion depth of 100 exceeded, most likely a circular definition");
        return FAILURE;
    }

    // DANGEROUS: Manually freeing variable expression
    free(*expr);

    // Clone expression of named constant expression
    *expr = ast_expr_clone(constant->expression);
    if(infer_expr_inner(ctx, ast_func, (ast_expr_t**) expr, undetermined, false)) return FAILURE;

    return SUCCESS;

found_variable:
    // We don't have to infer any types if we already know the solution primitive
    if(undetermined->solution != EXPR_NONE) return SUCCESS;
    
    // Attempt to boil down undetermined primitive types
    var_expr_primitive = ast_primitive_from_ast_type(variable_type);
    if(undetermined_expr_list_give_solution(ctx, undetermined, var_expr_primitive)) return FAILURE;

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
            case EXPR_BOOLEAN:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_bool) == 1
                ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_int_t*) expr)->value != 0);
                break;
            case EXPR_BYTE:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_byte) == 1
                ((ast_expr_byte_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_UBYTE:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_ubyte) == 1
                ((ast_expr_ubyte_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_SHORT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_short) == 2
                ((ast_expr_short_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_USHORT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_ushort) == 2
                ((ast_expr_ushort_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_INT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_int) == 4
                ((ast_expr_int_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_UINT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_uint) == 4
                ((ast_expr_uint_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_LONG: case EXPR_ULONG: case EXPR_USIZE:
                // This is ok because the memory that was allocated is equal to that needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_long, adept_ulong, adept_usize) == 8
                // No changes special changes necessary, because casting int64 to int64/uint64 is just a bitcast
                break;
            case EXPR_FLOAT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_float) == 4
                ((ast_expr_float_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            case EXPR_DOUBLE:
                // This is ok because the memory that was allocated is equal to that needed
                // sizeof(adept_generic_int) == 8  -->  sizeof(adept_double) == 8
                ((ast_expr_double_t*) expr)->value = ((ast_expr_generic_int_t*) expr)->value;
                break;
            default:
                compiler_panic(ctx->compiler, expr->source, "INTERNAL ERROR: Unrecognized target primitive in resolve_generics()");
                return FAILURE; // Unknown assigned type
            }
            expr->id = target_primitive;
            break;
        case EXPR_GENERIC_FLOAT:
            switch(target_primitive){
            case EXPR_BOOLEAN:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_bool) == 1

                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a bool"))
                    return FAILURE;
                ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_float_t*) expr)->value != 0);
                break;
            case EXPR_BYTE:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_byte) == 1
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a byte"))
                    return FAILURE;
                ((ast_expr_byte_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_UBYTE:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_ubyte) == 1
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a ubyte"))
                    return FAILURE;
                ((ast_expr_ubyte_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_SHORT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_short) == 2
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a short"))
                    return FAILURE;
                ((ast_expr_short_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_USHORT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_ushort) == 2
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a ushort"))
                    return FAILURE;
                ((ast_expr_ushort_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_INT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_int) == 4
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to an int"))
                    return FAILURE;
                ((ast_expr_int_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_UINT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_uint) == 4
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a uint"))
                    return FAILURE;
                ((ast_expr_uint_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_LONG:
                // This is ok because the memory that was allocated is equal to that needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_long) == 8
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a long"))
                    return FAILURE;
                ((ast_expr_long_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_ULONG:
                // This is ok because the memory that was allocated is equal to that needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_ulong) == 8
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a ulong"))
                    return FAILURE;
                ((ast_expr_ulong_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_USIZE:
                // This is ok because the memory that was allocated is equal to that needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_usize) == 8
                if(compiler_warnf(ctx->compiler, expr->source, "Implicitly converting generic float value to a usize"))
                    return FAILURE;
                ((ast_expr_usize_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_FLOAT:
                // This is ok because the memory that was allocated is larger than needed
                // sizeof(adept_generic_float) == 8  -->  sizeof(adept_float) == 4
                ((ast_expr_float_t*) expr)->value = ((ast_expr_generic_float_t*) expr)->value;
                break;
            case EXPR_DOUBLE:
                // No changes special changes necessary, since (adept_double == adept_generic_float)
                break;
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
    //       e.g. the primitive type of (10 + 8.0 + 3) is a double
    //       e.g. the primitive type of (10 + 3) is an int

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
    else if(generics & FLAG_GENERIC_INT)   assigned_type = EXPR_LONG;
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
    length_t prev_recursion_depth = ctx->aliases_recursion_depth;
    ctx->aliases_recursion_depth = 0;
       
    errorcode_t res = infer_type_inner(ctx, type, type->source);
    ctx->aliases_recursion_depth = prev_recursion_depth;
    return res;
}

errorcode_t infer_type_inner(infer_ctx_t *ctx, ast_type_t *type, source_t original_source){
    // NOTE: Expands 'type' by resolving any aliases
    
    ast_alias_t *aliases = ctx->ast->aliases;
    length_t aliases_length = ctx->ast->aliases_length;

    ast_elem_t **new_elements = NULL;
    length_t length = 0;
    length_t capacity = 0;

    for(length_t e = 0; e < type->elements_length; e++){
        ast_elem_t *elem = type->elements[e];

        switch(elem->id){
        case AST_ELEM_BASE: {
                weak_cstr_t base = ((ast_elem_base_t*) elem)->base;

                if(streq(base, "void") && type->elements_length > 1 && e != 0 && type->elements[e - 1]->id == AST_ELEM_POINTER){
                    // Substitute '*void' with 'ptr'

                    // Create replacement element
                    ast_elem_base_t *ptr_elem = malloc(sizeof(ast_elem_base_t));
                    ptr_elem->id = AST_ELEM_BASE;
                    ptr_elem->source = type->elements[e]->source;
                    ptr_elem->base = strclone("ptr");

                    strong_cstr_t typename = ast_type_str(type);

                    // Free base type element 'void' that will disappear
                    ast_elem_free(elem);

                    // DANGEROUS: Manually freeing pointer ast_elem_pointer_t element
                    free(new_elements[length - 1]);

                    // Replace previous '*' with 'ptr'
                    new_elements[length - 1] = (ast_elem_t*) ptr_elem;

                    ast_type_t replaced_view = (ast_type_t){
                        .elements = new_elements,
                        .elements_length = length,
                        .source = original_source,
                    };

                    type_table_give(ctx->type_table, &replaced_view, typename);
                    continue;
                }

                int alias_index = ast_find_alias(aliases, aliases_length, base);
                
                if(alias_index != -1){
                    // NOTE: The alias target type was already resolved of any aliases,
                    //       so we don't have to scan the new elements
                    ast_type_t cloned = ast_type_clone(&aliases[alias_index].type);
                    expand((void**) &new_elements, sizeof(ast_elem_t*), length, &capacity, cloned.elements_length, 4);

                    {
                        if(ctx->aliases_recursion_depth++ >= 100){
                            compiler_panicf(ctx->compiler, original_source, "Recursion depth of 100 exceeded, most likely a circular definition");
                            return FAILURE;
                        }

                        if(infer_type_inner(ctx, &cloned, original_source)) return FAILURE;

                        type_table_give(ctx->type_table, &cloned, strclone(base));
                        ctx->aliases_recursion_depth--;
                    }

                    // Move all the elements from the cloned type to this type
                    for(length_t m = 0; m != cloned.elements_length; m++){
                        new_elements[length++] = cloned.elements[m];
                    }

                    ast_elem_free(elem);

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
        case AST_ELEM_GENERIC_BASE: {
                for(length_t a = 0; a != ((ast_elem_generic_base_t*) elem)->generics_length; a++){
                    if(infer_type(ctx, &((ast_elem_generic_base_t*) elem)->generics[a])) return FAILURE;
                }
            }
            break;
        case AST_ELEM_VAR_FIXED_ARRAY: {
                // Take out expression from variable fixed array and perform inference on it
                ast_expr_t **length = &(((ast_elem_var_fixed_array_t*) elem)->length);
                if(infer_expr(ctx, NULL, length, EXPR_NONE, false)) return FAILURE;

                length_t value;
                if(ast_expr_deduce_to_size(*length, &value)){
                    compiler_panic(ctx->compiler, (*length)->source, "Could not deduce fixed array size at compile time");
                    return FAILURE;
                }

                ast_expr_free_fully(*length);

                // Boil it down to a regular fixed array
                // DANGEROUS: Relying on memory layout
                // DANGEROUS: Assuming that sizeof(ast_elem_fixed_array_t) <= sizeof(ast_elem_var_fixed_array_t)
                ((ast_elem_fixed_array_t*) elem)->id = AST_ELEM_FIXED_ARRAY;
                ((ast_elem_fixed_array_t*) elem)->length = value;
                // neglect ((ast_elem_fixed_array_t*) elem)->source;
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
    return SUCCESS;
}

void infer_var_scope_init(infer_var_scope_t *out_scope, infer_var_scope_t *parent){
    out_scope->parent = parent;
    out_scope->list.variables = NULL;
    out_scope->list.length = 0;
    out_scope->list.capacity = 0;
    out_scope->constants = NULL;
    out_scope->constants_length = 0;
    out_scope->constants_capacity = 0;
}

void infer_var_scope_free(compiler_t *compiler, infer_var_scope_t *scope){
    infer_var_t *variables = scope->list.variables;
    length_t length = scope->list.length;

    // TODO: Don't print unused variable warnings if error occurred
    if(!(compiler->ignore & COMPILER_IGNORE_UNUSED || compiler->traits & COMPILER_NO_WARN)){
        for(length_t i = 0; i < length; i++){
            if(!variables[i].used){
                // Ignore whether to terminate, since we cannot terminate right now because we are freeing 'infer_var_scope_t's
                // Optional termination for this warning is handled when inference is finished.
                compiler_warnf(compiler, variables[i].source, "Variable '%s' is never used", variables[i].name);
                compiler->show_unused_variables_how_to_disable = true;
            }
        }
    }

    free(scope->list.variables);

    if(scope->constants){
        ast_free_constants(scope->constants, scope->constants_length);
        free(scope->constants);
    }
}

void infer_var_scope_push(infer_var_scope_t **scope){
    infer_var_scope_t *new_scope = malloc(sizeof(infer_var_scope_t));
    infer_var_scope_init(new_scope, *scope);
    *scope = new_scope;
} 

void infer_var_scope_pop(compiler_t *compiler, infer_var_scope_t **scope){
    if((*scope)->parent == NULL){
        internalerrorprintf("infer_var_scope_pop() - Cannot pop infer-variable-scope with no parent\n");
        return;
    }

    infer_var_scope_t *parent = (*scope)->parent;
    infer_var_scope_free(compiler, *scope);
    free(*scope);

    *scope = parent;
}

infer_var_t* infer_var_scope_find(infer_var_scope_t *scope, const char *name){
    for(length_t i = 0; i != scope->list.length; i++){
        if(streq(scope->list.variables[i].name, name)){
            return &scope->list.variables[i];
        }
    }

    return scope->parent ? infer_var_scope_find(scope->parent, name) : NULL;
}

ast_constant_t* infer_var_scope_find_constant(infer_var_scope_t *scope, const char *name){
    // If the scope has local constants, search through them
    if(scope->constants_length != 0){
        maybe_index_t index = ast_find_constant(scope->constants, scope->constants_length, name);
        if(index != -1) return &scope->constants[index];
    }

    // Otherwise try in parent scope
    return scope->parent ? infer_var_scope_find_constant(scope->parent, name) : NULL;
}

void infer_var_scope_add_variable(infer_var_scope_t *scope, weak_cstr_t name, ast_type_t *type, source_t source, bool force_used, bool is_const){
    // NOTE: Assumes name is a valid C-String

    infer_var_list_t *list = &scope->list;
    expand((void**) &list->variables, sizeof(infer_var_t), list->length, &list->capacity, 1, 4);

    infer_var_t *var = &list->variables[list->length++];
    var->name = name;
    var->type = type;
    var->source = source;
    var->used = force_used || name[0] == '_';
    var->is_const = is_const;
}

void infer_var_scope_add_constant(infer_var_scope_t *scope, ast_constant_t constant){
    expand((void**) &scope->constants, sizeof(ast_constant_t), scope->constants_length, &scope->constants_capacity, 1, 4);

    length_t insert_position = find_insert_position(scope->constants, scope->constants_length, ast_constants_cmp, &constant, sizeof(ast_constant_t));

    // Move other constants over, so that the list will be sorted
    memmove(&scope->constants[insert_position + 1], &scope->constants[insert_position], sizeof(ast_constant_t) * (scope->constants_length - insert_position));

    scope->constants[insert_position] = constant;
    scope->constants_length++;
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

    // Heap-allocated array to contain all of the distances
    length_t list_length = list->length;
    int *distances = malloc(sizeof(int) * list_length);

    // Calculate distance for every variable name
    for(length_t i = 0; i != list_length; i++){
        distances[i] = levenshtein(name, list->variables[i].name);
    }

    // Minimum number of changes to override NULL
    int minimum = 3;

    // Find the name with the shortest distance
    for(length_t i = 0; i != list_length; i++){
        if(distances[i] < minimum){
            minimum = distances[i];
            *out_nearest_name = list->variables[i].name;
        }
    }

    // Output minimum distance if a name close enough was found
    if(out_distance && *out_nearest_name) *out_distance = minimum;

    free(distances);
}

void infer_mention_expression_literal_type(infer_ctx_t *ctx, unsigned int expression_literal_id){
    // HACK: Mention types created through primitive data to the type table
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
        die("infer_mention_expression_literal_type() - Unrecognized literal expression ID\n", (int) expression_literal_id);
    }
}
