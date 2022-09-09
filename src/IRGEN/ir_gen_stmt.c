
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_identical.h"
#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "BRIDGE/bridge.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_module.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/builtin_type.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"

errorcode_t ir_gen_stmts(ir_builder_t *builder, ast_expr_list_t *stmt_list, bool *out_is_terminated){
    ir_instr_t *built_instr;
    ir_value_t *expression_value = NULL;
    ast_type_t temporary_type;

    // Default value of whether the statements were terminated is 'false'
    if(out_is_terminated) *out_is_terminated = false;

    for(length_t s = 0; s != stmt_list->length; s++){
        ast_expr_t *stmt = stmt_list->statements[s];

        switch(stmt->id){
        case EXPR_RETURN:
            // TODO: CLEANUP: Refactor this code
            // Generate instructions in order to return
            if(ir_gen_stmt_return(builder, (ast_expr_return_t*) stmt, out_is_terminated)) return FAILURE;

            // Warn if there are statements following
            if(builder->compiler->traits & COMPILER_FUSSY && !(builder->compiler->ignore & COMPILER_IGNORE_EARLY_RETURN)){
                if(s + 1 != stmt_list->length){
                    const char *f_name = builder->object->ast.funcs[builder->ast_func_id].name;

                    if(compiler_warnf(builder->compiler, stmt_list->statements[s + 1]->source, "Statements after 'return' in function '%s'", f_name))
                        return FAILURE;
                }
            }

            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_CALL:
        case EXPR_CALL_METHOD:
            if(ir_gen_stmt_call_like(builder, stmt)) return FAILURE;
            break;
        case EXPR_PREINCREMENT:
        case EXPR_PREDECREMENT:
        case EXPR_POSTINCREMENT:
        case EXPR_POSTDECREMENT:
        case EXPR_TOGGLE:
            // Expression-statements will be processed elsewhere
            if(ir_gen_expr(builder, stmt, NULL, true, NULL)) return FAILURE;
            break;
        case EXPR_DECLARE:
        case EXPR_DECLAREUNDEF:
            if(ir_gen_stmt_declare(builder, (ast_expr_declare_t*) stmt)) return FAILURE;
            break;
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUBTRACT_ASSIGN:
        case EXPR_MULTIPLY_ASSIGN:
        case EXPR_DIVIDE_ASSIGN:
        case EXPR_MODULUS_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
        case EXPR_LGC_LSHIFT_ASSIGN:
        case EXPR_LGC_RSHIFT_ASSIGN:
            if(ir_gen_stmt_assignment_like(builder, (ast_expr_assign_t*) stmt)) return FAILURE;
            break;
        case EXPR_IF:
        case EXPR_UNLESS:
            if(ir_gen_stmt_simple_conditional(builder, (ast_expr_if_t*) stmt)) return FAILURE;
            break;
        case EXPR_IFELSE:
        case EXPR_UNLESSELSE:
            if(ir_gen_stmt_dual_conditional(builder, (ast_expr_ifelse_t*) stmt)) return FAILURE;
            break;
        case EXPR_WHILE:
        case EXPR_UNTIL:
            if(ir_gen_stmt_simple_loop(builder, (ast_expr_while_t*) stmt)) return FAILURE;
            break;
        case EXPR_WHILECONTINUE:
        case EXPR_UNTILBREAK:
            if(ir_gen_stmt_recurrent_loop(builder, (ast_expr_whilecontinue_t*) stmt)) return FAILURE;
            break;
        case EXPR_DELETE:
            if(ir_gen_stmt_delete(builder, (ast_expr_delete_t*) stmt)) return FAILURE;
            break;
        case EXPR_BREAK:
            if(ir_gen_stmt_break(builder, stmt, out_is_terminated)) return FAILURE;

            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_CONTINUE:
            if(ir_gen_stmt_continue(builder, stmt, out_is_terminated)) return FAILURE;

            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_FALLTHROUGH:
            if(ir_gen_stmt_fallthrough(builder, stmt, out_is_terminated)) return FAILURE;

            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_BREAK_TO:
            if(ir_gen_stmt_break_to(builder, (ast_expr_break_to_t*) stmt, out_is_terminated)) return FAILURE;

            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_CONTINUE_TO: 
            if(ir_gen_stmt_continue_to(builder, (ast_expr_continue_to_t*) stmt, out_is_terminated)) return FAILURE;
            
            return SUCCESS; // Return since no other statements can be after this one
        case EXPR_EACH_IN:
            if(ir_gen_stmt_each(builder, (ast_expr_each_in_t*) stmt)) return FAILURE;
            break;
        case EXPR_REPEAT:
            if(ir_gen_stmt_repeat(builder, (ast_expr_repeat_t*) stmt)) return FAILURE;
            break;
        case EXPR_SWITCH: {
                // TODO: CLEANUP: Refactor this code

                ast_expr_switch_t *switch_expr = (ast_expr_switch_t*) stmt;

                ir_value_t *condition;
                ast_type_t master_ast_type;
                if(ir_gen_expr(builder, switch_expr->value, &condition, false, &master_ast_type)){
                    return FAILURE;
                }

                length_t default_block_id, resume_block_id;
                length_t starting_block_id = builder->current_block_id;

                // STATIC ASSERT: Ensure that all IR integer type kinds are in the expected range
                #define tmp_range(v) (v >= 0x00000002 && v <= 0x00000009)
                #if(!(tmp_range(TYPE_KIND_S8) || tmp_range(TYPE_KIND_S16) || tmp_range(TYPE_KIND_S32) || tmp_range(TYPE_KIND_S64) || \
                    tmp_range(TYPE_KIND_U8) || tmp_range(TYPE_KIND_U16) || tmp_range(TYPE_KIND_U32) || tmp_range(TYPE_KIND_U64)))
                #error "EXPR_SWITCH in ir_gen_stmt.c assumes IR integer type kinds are between 0x00000002 and 0x00000009"
                #endif

                // Use the assumption that IR integer type kinds are in the expected range to check integer-ness
                bool integer_like = tmp_range(condition->type->kind);
                #undef tmp_range

                // Make sure value type is suitable
                if(!integer_like){
                    char *typename = ast_type_str(&master_ast_type);
                    compiler_panicf(builder->compiler, switch_expr->source, "Cannot perform switch on type '%s'", typename);
                    ast_type_free(&master_ast_type);
                    free(typename);
                    return FAILURE;
                }

                ir_value_t **case_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * switch_expr->cases.length);
                length_t *case_block_ids = ir_pool_alloc(builder->pool, sizeof(length_t) * switch_expr->cases.length);
                unsigned long long *uniqueness = malloc(sizeof(unsigned long long) * switch_expr->cases.length);

                // Layout basicblocks for each case
                for(length_t c = 0; c != switch_expr->cases.length; c++){
                    case_block_ids[c] = build_basicblock(builder);
                }
                
                // Create basicblocks for default case and/or resuming control flow
                if(switch_expr->or_default.length != 0){
                    default_block_id = build_basicblock(builder);
                    resume_block_id = build_basicblock(builder);
                } else {
                    resume_block_id = build_basicblock(builder);
                    default_block_id = resume_block_id;
                }

                // Populate each case
                for(length_t c = 0; c != switch_expr->cases.length; c++){
                    ast_case_t *switch_case = &switch_expr->cases.cases[c];

                    ast_type_t slave_ast_type;
                    if(ir_gen_expr(builder, switch_case->condition, &case_values[c], false, &slave_ast_type)){
                        ast_type_free(&master_ast_type);
                        free(uniqueness);
                        return FAILURE;
                    }

                    if(!ast_types_conform(builder, &case_values[c], &slave_ast_type, &master_ast_type, CONFORM_MODE_CALCULATION)){
                        char *master_typename = ast_type_str(&master_ast_type);
                        char *slave_typename = ast_type_str(&slave_ast_type);
                        compiler_panicf(builder->compiler, switch_case->source, "Case type '%s' is incompatible with expected type '%s'", slave_typename, master_typename);
                        free(master_typename);
                        free(slave_typename);
                        ast_type_free(&slave_ast_type);
                        ast_type_free(&master_ast_type);
                        free(uniqueness);
                        return FAILURE;
                    }

                    ast_type_free(&slave_ast_type);

                    unsigned int value_type = case_values[c]->value_type;
                    if(!VALUE_TYPE_IS_CONSTANT(value_type)){
                        compiler_panicf(builder->compiler, switch_case->source, "Value given to case must be constant");
                        ast_type_free(&master_ast_type);
                        free(uniqueness);
                        return FAILURE;
                    }

                    unsigned long long uniqueness_value = ir_value_uniqueness_value(builder->pool, &case_values[c]);
                    uniqueness[c] = uniqueness_value;

                    for(length_t u = 0; u != c; u++){
                        if(uniqueness_value == uniqueness[u]){
                            compiler_panicf(builder->compiler, switch_expr->cases.cases[u].condition->source, "Non-unique case value");
                            compiler_panicf(builder->compiler, switch_case->condition->source, "Duplicate here");
                            ast_type_free(&master_ast_type);
                            free(uniqueness);
                            return FAILURE;
                        }
                    }
                    
                    length_t prev_fallthrough_block_id = builder->fallthrough_block_id;
                    bridge_scope_t *prev_fallthrough_scope = builder->fallthrough_scope;

                    // For 'fallthrough' statements, go to the next case, if it doesn't exist go to the default/resume case
                    builder->fallthrough_block_id = c + 1 == switch_expr->cases.length ? default_block_id : case_block_ids[c + 1];
                    builder->fallthrough_scope = builder->scope;

                    open_scope(builder);

                    bool case_terminated;
                    build_using_basicblock(builder, case_block_ids[c]);
                    if(ir_gen_stmts(builder, &switch_case->statements, &case_terminated)){
                        ast_type_free(&master_ast_type);
                        close_scope(builder);
                        free(uniqueness);
                        return FAILURE;
                    }

                    if(!case_terminated){
                        if(handle_deference_for_variables(builder, &builder->scope->list)){
                            ast_type_free(&master_ast_type);
                            close_scope(builder);
                            free(uniqueness);
                            return FAILURE;
                        }

                        build_break(builder, resume_block_id);
                    }
                    
                    close_scope(builder);
                    builder->fallthrough_block_id = prev_fallthrough_block_id;
                    builder->fallthrough_scope = prev_fallthrough_scope;
                }

                // If the switch is an exhaustive switch, check whether the supplied values are exhaustive
                if(switch_expr->is_exhaustive && ast_type_is_base(&master_ast_type)){
                    weak_cstr_t enum_name = ((ast_elem_base_t*) master_ast_type.elements[0])->base;
                    
                    if(exhaustive_switch_check(builder, enum_name, switch_expr->source, uniqueness, switch_expr->cases.length)){
                        ast_type_free(&master_ast_type);
                        free(uniqueness);
                        return FAILURE;
                    }
                }

                free(uniqueness);

                // Fill in statements for default block
                if(default_block_id != resume_block_id){
                    open_scope(builder);

                    bool case_terminated;
                    build_using_basicblock(builder, default_block_id);
                    if(ir_gen_stmts(builder, &switch_expr->or_default, &case_terminated)){
                        ast_type_free(&master_ast_type);
                        close_scope(builder);
                        return FAILURE;
                    }

                    if(!case_terminated){
                        if(handle_deference_for_variables(builder, &builder->scope->list)){
                            ast_type_free(&master_ast_type);
                            close_scope(builder);
                            return FAILURE;
                        }

                        build_break(builder, resume_block_id);
                    }
                    
                    close_scope(builder);
                }

                ast_type_free(&master_ast_type);

                build_using_basicblock(builder, starting_block_id);
                built_instr = build_instruction(builder, sizeof(ir_instr_switch_t));
                ((ir_instr_switch_t*) built_instr)->id = INSTRUCTION_SWITCH;
                ((ir_instr_switch_t*) built_instr)->result_type = condition->type;
                ((ir_instr_switch_t*) built_instr)->condition = condition;
                ((ir_instr_switch_t*) built_instr)->cases_length = switch_expr->cases.length;
                ((ir_instr_switch_t*) built_instr)->case_values = case_values;
                ((ir_instr_switch_t*) built_instr)->case_block_ids = case_block_ids;
                ((ir_instr_switch_t*) built_instr)->default_block_id = default_block_id;
                ((ir_instr_switch_t*) built_instr)->resume_block_id = resume_block_id;
                build_using_basicblock(builder, resume_block_id);
            }
            break;
        case EXPR_VA_START: case EXPR_VA_END: {
                ast_expr_unary_t *va_expr = (ast_expr_unary_t*) stmt;
                if(ir_gen_expr(builder, va_expr->value, &expression_value, true, &temporary_type)) return FAILURE;
                
                if(!ast_type_is_base_of(&temporary_type, "va_list")){
                    char *t = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, va_expr->source, "Can't pass non-'va_list' type '%s' to va_%s", t, stmt->id == EXPR_VA_START ? "start" : "end");
                    ast_type_free(&temporary_type);
                    free(t);
                    return FAILURE;
                }

                if(!expr_is_mutable(va_expr->value)){
                    compiler_panicf(builder->compiler, va_expr->source, "Value passed to va_%s must be mutable", stmt->id == EXPR_VA_START ? "start" : "end");
                    return FAILURE;
                }

                // Cast from *va_list to *s8
                expression_value = build_bitcast(builder, expression_value, builder->ptr_type);

                built_instr = build_instruction(builder, sizeof(ir_instr_unary_t));
                ((ir_instr_unary_t*) built_instr)->id = stmt->id == EXPR_VA_START ? INSTRUCTION_VA_START : INSTRUCTION_VA_END;
                ((ir_instr_unary_t*) built_instr)->result_type = NULL;
                ((ir_instr_unary_t*) built_instr)->value = expression_value;

                ast_type_free(&temporary_type);
            }
            break;
        case EXPR_VA_COPY: {
                ast_expr_va_copy_t *va_copy_expr = (ast_expr_va_copy_t*) stmt;

                ir_value_t *dest_value;
                if(ir_gen_expr(builder, va_copy_expr->dest_value, &dest_value, true, &temporary_type)) return FAILURE;
                
                if(!ast_type_is_base_of(&temporary_type, "va_list")){
                    char *t = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, va_copy_expr->dest_value->source, "Can't pass non-'va_list' type '%s' to va_copy", t);
                    ast_type_free(&temporary_type);
                    free(t);
                    return FAILURE;
                }

                if(!expr_is_mutable(va_copy_expr->dest_value)){
                    compiler_panicf(builder->compiler, va_copy_expr->dest_value->source, "Value passed to va_copy must be mutable");
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                ir_value_t *src_value;
                if(ir_gen_expr(builder, va_copy_expr->src_value, &src_value, true, &temporary_type)) return FAILURE;
                
                if(!ast_type_is_base_of(&temporary_type, "va_list")){
                    char *t = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, va_copy_expr->src_value->source, "Can't pass non-'va_list' type '%s' to va_copy", t);
                    ast_type_free(&temporary_type);
                    free(t);
                    return FAILURE;
                }

                if(!expr_is_mutable(va_copy_expr->dest_value)){
                    compiler_panicf(builder->compiler, va_copy_expr->src_value->source, "Value passed to va_copy must be mutable");
                    return FAILURE;
                }

                // Cast from *va_list to *s8
                dest_value = build_bitcast(builder, dest_value, builder->ptr_type);
                src_value   = build_bitcast(builder, src_value, builder->ptr_type);

                built_instr = build_instruction(builder, sizeof(ir_instr_va_copy_t));
                ((ir_instr_va_copy_t*) built_instr)->id = INSTRUCTION_VA_COPY;
                ((ir_instr_va_copy_t*) built_instr)->result_type = NULL;
                ((ir_instr_va_copy_t*) built_instr)->dest_value = dest_value;
                ((ir_instr_va_copy_t*) built_instr)->src_value = src_value;

                ast_type_free(&temporary_type);
            }
            break;
        case EXPR_FOR: {
                ast_expr_for_t *for_loop = (ast_expr_for_t*) stmt;
                length_t prep_basicblock_id = build_basicblock(builder);
                length_t adv_basicblock_id = build_basicblock(builder);
                length_t new_basicblock_id  = build_basicblock(builder);
                length_t end_basicblock_id  = build_basicblock(builder);

                if(for_loop->label != NULL){
                    push_loop_label(builder, for_loop->label, end_basicblock_id, adv_basicblock_id);
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;
                bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;
                
                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = adv_basicblock_id;
                builder->break_continue_scope = builder->scope;

                open_scope(builder);

                // Do 'before' statements
                bool terminated;
                if(ir_gen_stmts(builder, &for_loop->before, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                // Don't allow 'return'/'continue'/'break' in 'before' statements
                if(terminated){
                    compiler_panic(builder->compiler, for_loop->before.statements[0]->source, "The 'before' statements of a 'for' loop cannot contain a terminator");
                    close_scope(builder);
                    return FAILURE;
                }

                // Use prep block to calculate condition
                build_break(builder, prep_basicblock_id);
                build_using_basicblock(builder, prep_basicblock_id);

                // Generate condition
                ir_value_t *condition_value;

                if(for_loop->condition){
                    if(ir_gen_expr(builder, for_loop->condition, &condition_value, false, &temporary_type)) return FAILURE;
                } else {
                    temporary_type = ast_type_make_base(strclone("bool"));
                    condition_value = build_bool(builder->pool, true);
                }

                // Create static bool type for comparison with
                ast_elem_base_t bool_base;
                bool_base.id = AST_ELEM_BASE;
                bool_base.source = NULL_SOURCE;
                bool_base.source.object_index = builder->object->index;
                bool_base.base = "bool";
                ast_elem_t *bool_type_elem = (ast_elem_t*) &bool_base;
                ast_type_t bool_type;
                bool_type.elements = &bool_type_elem;
                bool_type.elements_length = 1;
                bool_type.source = NULL_SOURCE;
                bool_type.source.object_index = builder->object->index;

                if(!ast_types_conform(builder, &condition_value, &temporary_type, &bool_type, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&bool_type);
                    compiler_panicf(builder->compiler, stmt->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }

                // Dispose of temporary AST type of condition
                ast_type_free(&temporary_type);

                // Generate conditional break
                build_cond_break(builder, condition_value, new_basicblock_id, end_basicblock_id);
                build_using_basicblock(builder, new_basicblock_id);

                // Generate new_block user-defined statements
                if(ir_gen_stmts(builder, &for_loop->statements, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    if(handle_deference_for_variables(builder, &builder->scope->list)){
                        close_scope(builder);
                        return FAILURE;
                    }

                    build_break(builder, adv_basicblock_id);
                }

                build_using_basicblock(builder, adv_basicblock_id);

                // Do 'after' statements
                if(ir_gen_stmts(builder, &for_loop->after, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                // Don't allow 'return'/'continue'/'break' in 'after' statements
                if(terminated){
                    compiler_panic(builder->compiler, for_loop->after.statements[0]->source, "The 'after' statements of a 'for' loop cannot contain a terminator");
                    close_scope(builder);
                    return FAILURE;
                }

                build_break(builder, prep_basicblock_id);

                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                if(for_loop->label != NULL) pop_loop_label(builder);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
                builder->break_continue_scope = prev_break_continue_scope;
            }
            break;
        case EXPR_DECLARE_CONSTANT:
            // This statement was handled during the inference stage
            break;
        case EXPR_LLVM_ASM: {
                ast_expr_llvm_asm_t *asm_expr = (ast_expr_llvm_asm_t*) stmt;
                ir_value_t **args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * asm_expr->arity);

                for(length_t i = 0; i != asm_expr->arity; i++){
                    if(ir_gen_expr(builder, asm_expr->args[i], &args[i], false, NULL)){
                        return FAILURE;
                    }
                }

                build_llvm_asm(builder, asm_expr->is_intel, asm_expr->assembly,
                    asm_expr->constraints, args, asm_expr->arity,
                    asm_expr->has_side_effects, asm_expr->is_stack_align);
            }
            break;
        case EXPR_CONDITIONLESS_BLOCK: {
                ast_expr_conditionless_block_t *block_expr = (ast_expr_conditionless_block_t*) stmt;
                open_scope(builder);

                errorcode_t errorcode = (
                    ir_gen_stmts(builder, &block_expr->statements, out_is_terminated)
                    || (!*out_is_terminated && handle_deference_for_variables(builder, &builder->scope->list))
                );

                close_scope(builder);
                if(errorcode) return errorcode;

                if(*out_is_terminated){
                    return SUCCESS;
                }
            }
            break;
        default:
            compiler_panic(builder->compiler, stmt->source, "INTERNAL ERROR: Unimplemented statement in ir_gen_stmts()");
            return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t ir_gen_stmt_return(ir_builder_t *builder, ast_expr_return_t *stmt, bool *out_is_terminated){
    ast_t *ast = &builder->object->ast;

    ast_type_t return_type;
    bool is_in_main_function;
    bool is_in_pass_function;
    bool is_in_defer_function;
    bool is_in_winmain_function;
    bool autogen_enabled;
    
    // 'ast_func' is only guaranteed to be valid within this scope.
    // Afterwards, it may or may not be shifted around in memory.
    // Because of this, no pointers should be taken relative to 'ast_func'.
    {
        ast_func_t *ast_func = &ast->funcs[builder->ast_func_id];

        return_type = ast_func->return_type;
        is_in_main_function  = ast_func->traits & AST_FUNC_MAIN;
        is_in_pass_function  = ast_func->traits & AST_FUNC_PASS;
        is_in_defer_function = ast_func->traits & AST_FUNC_DEFER;
        is_in_winmain_function  = ast_func->traits & AST_FUNC_WINMAIN;
        autogen_enabled      = ast_func->traits & AST_FUNC_AUTOGEN;
    }

    ir_value_t *ir_value_to_be_returned = NULL;
    bool returns_void = ast_type_is_void(&return_type);

    if(stmt->value != NULL){
        if(returns_void){
            compiler_panicf(builder->compiler, stmt->source, "Can't return a value from function that returns void");
            return FAILURE;
        }

        ast_type_t value_return_type;

        // Generate instructions to calculate return value
        if(ir_gen_expr(builder, stmt->value, &ir_value_to_be_returned, false, &value_return_type)) return FAILURE;

        // Conform return value to expected return type
        if(!ast_types_conform(builder, &ir_value_to_be_returned, &value_return_type, &return_type, CONFORM_MODE_CALCULATION)){
            char *a_type_str = ast_type_str(&value_return_type);
            char *b_type_str = ast_type_str(&return_type);
            compiler_panicf(builder->compiler, stmt->source, "Attempting to return type '%s' when function expects type '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&value_return_type);
            return FAILURE;
        }

        ast_type_free(&value_return_type);
    } else if(is_in_main_function && returns_void){
        // Return 0 if in main function and it returns void
        ir_value_to_be_returned = build_literal_int(builder->pool, 0);
    } else if(!returns_void){
        // This function expects a value to returned, not void
        char *a_type_str = ast_type_str(&return_type);
        compiler_panicf(builder->compiler, stmt->source, "Attempting to return void when function expects type '%s'", a_type_str);
        free(a_type_str);
        return FAILURE;
    } else {
        // Return void
        ir_value_to_be_returned = NULL;
    }

    // Handle deferred statements, making sure to prohibit terminating statements
    {
        bool illegally_terminated;

        if(ir_gen_stmts(builder, &stmt->last_minute, &illegally_terminated))
            return FAILURE;

        if(illegally_terminated){
            compiler_panicf(builder->compiler, stmt->source, "Cannot expand a previously deferred terminating statement");
            return FAILURE;
        }
    }

    // Make '__defer__()' calls for variables running out of scope
    if(ir_gen_variable_deference(builder, NULL)) return FAILURE;

    // Make '__defer__()' calls for global variables and (anonymous) static variables running out of scope
    if(is_in_main_function || is_in_winmain_function){
        build_main_deinitialization(builder);
    }

    // If auto-generation is enabled and this function is eligible,
    // then attempt to do so
    if(autogen_enabled){
        if(
            (is_in_pass_function  && handle_children_pass_root(builder, true)) ||
            (is_in_defer_function && handle_children_deference(builder))
        ) return FAILURE;
    }

    // Return the determined value
    build_return(builder, ir_value_to_be_returned);
    
    // The 'return' statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;

    return SUCCESS;
}

errorcode_t ir_gen_stmt_call_like(ir_builder_t *builder, ast_expr_t *call_like_stmt){
    ast_type_t dropped_type;
    ir_value_t *dropped_value;

    // Handle call-like statements as expressions
    if(ir_gen_expr(builder, call_like_stmt, &dropped_value, true, &dropped_type)) return FAILURE;

    weak_cstr_t base_name = NULL;

    if(ast_type_is_base(&dropped_type)){
        base_name = ((ast_elem_base_t*) dropped_type.elements[0])->base;
    } else if(ast_type_is_generic_base(&dropped_type)){
        base_name = ((ast_elem_generic_base_t*) dropped_type.elements[0])->name;
    }
    
    // Handle dropped values from call expressions
    if(base_name && !typename_is_extended_builtin_type(base_name)){
        // Temporarily allocate space on the stack to store the dropped value
        ir_value_t *stack_pointer = build_stack_save(builder);
        ir_value_t *temporary_mutable = build_alloc(builder, dropped_value->type);
        build_store(builder, dropped_value, temporary_mutable, call_like_stmt->source);

        // Properly clean up the dropped value
        if(handle_single_deference(builder, &dropped_type, temporary_mutable) == ALT_FAILURE){
            ast_type_free(&dropped_type);
            return FAILURE;
        }

        build_stack_restore(builder, stack_pointer);
    }

    ast_type_free(&dropped_type);
    return SUCCESS;
}

errorcode_t ir_gen_stmt_declare(ir_builder_t *builder, ast_expr_declare_t *stmt){
    // Don't allow multiple variables with the same name in the same scope
    if(bridge_scope_var_already_in_list(builder->scope, stmt->name)){
        compiler_panicf(builder->compiler, stmt->source, "Variable '%s' already declared", stmt->name);
        return FAILURE;
    }

    ir_type_t *ir_type;
    trait_t traits = TRAIT_NONE;
    bool is_undef = stmt->id == EXPR_DECLAREUNDEF && !(builder->compiler->traits & COMPILER_NO_UNDEF);

    if(stmt->traits & AST_EXPR_DECLARATION_POD)    traits |= BRIDGE_VAR_POD;
    if(stmt->traits & AST_EXPR_DECLARATION_STATIC) traits |= BRIDGE_VAR_STATIC;
    if(is_undef)                                   traits |= BRIDGE_VAR_UNDEF;

    // Resolve AST type to IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &stmt->type, &ir_type)) return FAILURE;

    // Add the variable
    bridge_var_t *bridge_variable = add_variable(builder, stmt->name, &stmt->type, ir_type, traits);

    ir_value_t *variable = stmt->inputs.has
        ? build_varptr(builder, ir_type_make_pointer_to(builder->pool, ir_type), bridge_variable)
        : NULL;

    // Initialize variable if applicable
    if(!is_undef && ir_gen_stmt_declare_try_init(builder, stmt, ir_type)) return FAILURE;

    // Construct variable if applicable
    if(stmt->inputs.has){
        if(ir_gen_do_construct(builder, variable, &stmt->type, &stmt->inputs.value, stmt->source)){
            return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t ir_gen_do_construct(
    ir_builder_t *builder,
    ir_value_t *mutable_value,
    const ast_type_t *struct_ast_type,
    ast_expr_list_t *inputs,
    source_t source
){
    object_t *object = builder->object;
    weak_cstr_t struct_name = ast_type_struct_name(struct_ast_type);

    ir_pool_snapshot_t pool_snapshot;
    instructions_snapshot_t instructions_snapshot;

    // Take snapshot of construction state,
    // so that if this call ends up to be a no-op,
    // we can reset back as if nothing happened
    ir_pool_snapshot_capture(builder->pool, &pool_snapshot);
    instructions_snapshot_capture(builder, &instructions_snapshot);

    ir_value_t **raw_arg_values;
    ast_type_t *raw_arg_types;
    length_t raw_arity = inputs->length;

    if(ir_gen_arguments(builder, inputs->expressions, raw_arity, &raw_arg_values, &raw_arg_types)){
        return FAILURE;
    }

    length_t arity = raw_arity + 1;
    ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * arity);
    ast_type_t *arg_types = malloc(sizeof(ast_type_t) * arity);
    memcpy(&arg_values[1], raw_arg_values, sizeof(ir_value_t*) * raw_arity);
    memcpy(&arg_types[1], raw_arg_types, sizeof(ast_type_t) * raw_arity);
    free(raw_arg_types);

    // Insert subject value
    arg_values[0] = mutable_value;
    arg_types[0] = ast_type_clone(struct_ast_type);
    ast_type_prepend_ptr(&arg_types[0]);

    optional_func_pair_t result;
    errorcode_t search_errorcode = ir_gen_find_method_conforming(builder, struct_name, "__constructor__", &arg_values, &arg_types, &arity, NULL, source, &result);

    if(search_errorcode || !result.has){
        if(search_errorcode != ALT_FAILURE){
            compiler_panicf(builder->compiler, source, "Constructor with arguments does not exist");
        }
        goto failure;
    }

    func_pair_t pair = result.value;

    if(handle_pass_management(builder, arg_values, arg_types, object->ast.funcs[pair.ast_func_id].arg_type_traits, arity)){
        goto failure;
    }

    ir_type_t *ir_return_type = object->ir_module.funcs.funcs[pair.ir_func_id].return_type;
    build_call_ignore_result(builder, pair.ir_func_id, ir_return_type, arg_values, arity);

    ast_types_free(arg_types, arity);
    free(arg_types);
    return SUCCESS;

failure:
    ast_types_free(arg_types, arity);
    free(arg_types);
    ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
    instructions_snapshot_restore(builder, &instructions_snapshot);
    return FAILURE;
}

errorcode_t ir_gen_stmt_declare_try_init(ir_builder_t *primary_builder, ast_expr_declare_t *stmt, ir_type_t *ir_type){
    bool is_static = stmt->traits & AST_EXPR_DECLARATION_STATIC;

    // Determine which builder to use to build initialization instructions
    ir_builder_t *working_builder = is_static ? primary_builder->object->ir_module.init_builder : primary_builder;

    // If we'll instructions in a different location, allocate
    // a scope for the secondary builder.
    if(working_builder != primary_builder){
        // Using a different location will require that our program has an entry point
        if(!primary_builder->object->ir_module.common.has_main){
            compiler_panicf(primary_builder->compiler, stmt->source, "Cannot use static variables without a main function");
            return FAILURE;
        }

        working_builder->scope = malloc(sizeof(bridge_scope_t));
        bridge_scope_init(working_builder->scope, NULL);
    }

    ir_type_t *ir_type_ptr = ir_type_make_pointer_to(primary_builder->pool, ir_type);

    // Get pointer to where the variable is on the stack
    ir_value_t *destination;

    if(is_static){
        destination = build_svarptr(working_builder, ir_type_ptr, primary_builder->object->ir_module.static_variables.length - 1);
    } else {
        destination = build_lvarptr(working_builder, ir_type_ptr, primary_builder->next_var_id - 1);
    }

    // Perform initialization

    if(stmt->value == NULL){
        // No initial value - Zero Initialize
        build_zeroinit(working_builder, destination);
    } else {
        // Initial value - Assign Accordingly

        ir_value_t *initial;
        ast_type_t initial_ast_type;
        bool is_assign_pod = stmt->traits & AST_EXPR_DECLARATION_ASSIGN_POD;

        // Generate instructions to get initial value
        if(ir_gen_expr(working_builder, stmt->value, &initial, false, &initial_ast_type)) goto failure;

        errorcode_t errorcode = ir_gen_assign(working_builder, initial, &initial_ast_type, destination, &stmt->type, is_assign_pod, stmt->source);
        ast_type_free(&initial_ast_type);
        if(errorcode != SUCCESS) goto failure;
    }

    // Destroy allocated scope if we used a secondary builder
    if(working_builder != primary_builder){
        bridge_scope_free(working_builder->scope);
        free(working_builder->scope);
    }

    return SUCCESS;

failure:
    if(working_builder != primary_builder){
        bridge_scope_free(working_builder->scope);
        free(working_builder->scope);
    }

    return FAILURE;
}

errorcode_t ir_gen_stmt_assignment_like(ir_builder_t *builder, ast_expr_assign_t *stmt){
    unsigned int assignment_kind = stmt->id;

    // Generate instructions to get "other value"
    ir_value_t *other_value;
    ast_type_t other_value_type;
    if(ir_gen_expr(builder, stmt->value, &other_value, false, &other_value_type)) return FAILURE;
    
    // Generate instructions to get "destination value"
    ir_value_t *destination;
    ast_type_t destination_type;
    if(ir_gen_expr(builder, stmt->destination, &destination, true, &destination_type)){
        ast_type_free(&other_value_type);
        return FAILURE;
    }

    // Regular Assignment
    if(assignment_kind == EXPR_ASSIGN){
        errorcode_t errorcode = ir_gen_assign(builder, other_value, &other_value_type, destination, &destination_type, stmt->is_pod, stmt->source);
        ast_type_free(&destination_type);
        ast_type_free(&other_value_type);
        return errorcode;
    } else {
        // We only have to manually conform other type if doing POD operations,
        // and since as of now, non-POD assignment arithmetic isn't supported,
        // all non-regular assignments will be POD

        if(!ast_types_conform(builder, &other_value, &other_value_type, &destination_type, CONFORM_MODE_CALCULATION)){
            strong_cstr_t a_type_str = ast_type_str(&other_value_type);
            strong_cstr_t b_type_str = ast_type_str(&destination_type);
            compiler_panicf(builder->compiler, stmt->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&destination_type);
            ast_type_free(&other_value_type);
            return FAILURE;
        }
    }

    // Otherwise, Handle Non-Regular Assignment
    ast_type_free(&destination_type);
    ast_type_free(&other_value_type);

    ir_value_t *previous_value = build_load(builder, destination, stmt->source);

    ir_value_result_t *value_result;
    value_result = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
    value_result->block_id = builder->current_block_id;
    value_result->instruction_id = builder->current_block->instructions.length;

    ir_instr_math_t *math_instr = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    math_instr->result_type = other_value->type;
    math_instr->a = previous_value;
    math_instr->b = other_value;

    switch(assignment_kind){
    case EXPR_ADD_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_ADD, INSTRUCTION_FADD)){
            compiler_panic(builder->compiler, stmt->source, "Can't add those types");
            return FAILURE;
        }
        break;
    case EXPR_SUBTRACT_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT)){
            compiler_panic(builder->compiler, stmt->source, "Can't subtract those types");
            return FAILURE;
        }
        break;
    case EXPR_MULTIPLY_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_MULTIPLY, INSTRUCTION_FMULTIPLY)){
            compiler_panic(builder->compiler, stmt->source, "Can't multiply those types");
            return FAILURE;
        }
        break;
    case EXPR_DIVIDE_ASSIGN:
        if(u_vs_s_vs_float_instruction(math_instr, INSTRUCTION_UDIVIDE, INSTRUCTION_SDIVIDE, INSTRUCTION_FDIVIDE)){
            compiler_panic(builder->compiler, stmt->source, "Can't divide those types");
            return FAILURE;
        }
        break;
    case EXPR_MODULUS_ASSIGN:
        if(u_vs_s_vs_float_instruction(math_instr, INSTRUCTION_UMODULUS, INSTRUCTION_SMODULUS, INSTRUCTION_FMODULUS)){
            compiler_panic(builder->compiler, stmt->source, "Can't take the modulus of those types");
            return FAILURE;
        }
        break;
    case EXPR_AND_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_AND, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'and' on those types");
            return FAILURE;
        }
        break;
    case EXPR_OR_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_OR, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'or' on those types");
            return FAILURE;
        }
        break;
    case EXPR_XOR_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_XOR, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'xor' on those types");
            return FAILURE;
        }
        break;
    case EXPR_LSHIFT_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'left shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_RSHIFT_ASSIGN:
        if(u_vs_s_vs_float_instruction(math_instr, INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_BIT_RSHIFT, INSTRUCTION_FMODULUS)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'right shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_LGC_LSHIFT_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'logical left shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_LGC_RSHIFT_ASSIGN:
        if(i_vs_f_instruction(math_instr, INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_NONE)){
            compiler_panic(builder->compiler, stmt->source, "Can't perform bitwise 'logical right shift' on those types");
            return FAILURE;
        }
        break;
    default:
        compiler_panic(builder->compiler, stmt->source, "INTERNAL ERROR: ir_gen_stmts() got unknown assignment operator id");
        return FAILURE;
    }

    // Store result of math instruction into variable
    build_store(builder, build_value_from_prev_instruction(builder), destination, stmt->source);
    return SUCCESS;
}

errorcode_t ir_gen_stmt_simple_conditional(ir_builder_t *builder, ast_expr_if_t *stmt){
    const char *bad_condition_format = "Received type '%s' when conditional expects type '%s'";

    // Open new scope
    open_scope(builder);

    // Generate instructions to get condition value
    ir_value_t *condition = ir_gen_conforming_expr(builder, stmt->value, &builder->static_bool, CONFORM_MODE_CALCULATION, stmt->source, bad_condition_format);

    if(condition == NULL){
        close_scope(builder);
        return FAILURE;
    }

    length_t new_basicblock_id = build_basicblock(builder); // Create block for when the condition is true
    length_t end_basicblock_id = build_basicblock(builder); // Create block for when the condition is false

    if(stmt->id == EXPR_IF){
        build_cond_break(builder, condition, new_basicblock_id, end_basicblock_id);
    } else {
        build_cond_break(builder, condition, end_basicblock_id, new_basicblock_id);
    }

    // Prepare for block statements
    build_using_basicblock(builder, new_basicblock_id);

    // Generate IR instructions for the statements within the conditional block
    // and terminate the conditional block if it wasn't already terminated
    bool terminated;
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)
    || ir_gen_stmts_auto_terminate(builder, terminated, end_basicblock_id)){
        close_scope(builder);
        return FAILURE;
    }

    // Close block scope and continue building at the continuation point
    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);
    return SUCCESS;
}

errorcode_t ir_gen_stmt_dual_conditional(ir_builder_t *builder, ast_expr_ifelse_t *stmt){
    const char *bad_condition_error_format = "Received type '%s' when conditional expects type '%s'";

    // Create over-arching scope for entire conditional
    open_scope(builder);

    // Generate instructions to get condition value
    ir_value_t *condition = ir_gen_conforming_expr(builder, stmt->value, &builder->static_bool, CONFORM_MODE_CALCULATION, stmt->source, bad_condition_error_format);

    if(condition == NULL){
        close_scope(builder);
        return FAILURE;
    }

    length_t new_basicblock_id  = build_basicblock(builder); // Create block for when the condition is true
    length_t else_basicblock_id = build_basicblock(builder); // Create block for when the condition is false
    length_t end_basicblock_id  = build_basicblock(builder); // Create block for the continuation point

    if(stmt->id == EXPR_IFELSE){
        build_cond_break(builder, condition, new_basicblock_id, else_basicblock_id);
    } else {
        build_cond_break(builder, condition, else_basicblock_id, new_basicblock_id);
    }

    // Open primary block scope and prepare for block statements
    open_scope(builder);
    build_using_basicblock(builder, new_basicblock_id);

    // Generate IR instructions for the statements within the primary conditional block
    // and terminate the primary conditional block if it wasn't already terminated
    bool terminated;
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)
    || ir_gen_stmts_auto_terminate(builder, terminated, end_basicblock_id)){
        close_scope(builder);
        close_scope(builder);
        return FAILURE;
    }

    // Close primary block scope
    close_scope(builder);

    // Open secondary block scope and prepare for block statements
    open_scope(builder);
    build_using_basicblock(builder, else_basicblock_id);

    // Generate IR instructions for the statements within the secondary conditional block
    // and terminate the secondary conditional block if it wasn't already terminated
    if(ir_gen_stmts(builder, &stmt->else_statements, &terminated)
    || ir_gen_stmts_auto_terminate(builder, terminated, end_basicblock_id)){
        close_scope(builder);
        close_scope(builder);
        return FAILURE;
    }

    // Close secondary block scope and continue building at the continuation point
    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);

    // Close over-arching scope
    close_scope(builder);
    return SUCCESS;
}

errorcode_t ir_gen_stmt_simple_loop(ir_builder_t *builder, ast_expr_while_t *stmt){
    const char *bad_condition_error_format = "Received type '%s' when conditional expects type '%s'";

    length_t test_basicblock_id = build_basicblock(builder);
    length_t new_basicblock_id = build_basicblock(builder);
    length_t end_basicblock_id = build_basicblock(builder);

    // Add loop label
    if(stmt->label != NULL) push_loop_label(builder, stmt->label, end_basicblock_id, test_basicblock_id);

    // Remember previous jump points and scope level
    length_t prev_break_block_id              = builder->break_block_id;
    length_t prev_continue_block_id           = builder->continue_block_id;
    bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

    // Set the working jump points and scope level
    builder->break_block_id = end_basicblock_id;
    builder->continue_block_id = test_basicblock_id;
    builder->break_continue_scope = builder->scope;

    // Prepare to generate the condition value
    build_break(builder, test_basicblock_id);
    build_using_basicblock(builder, test_basicblock_id);

    // Open scope
    open_scope(builder);

    // Generate IR instructions to get the condition value
    ir_value_t *condition = ir_gen_conforming_expr(builder, stmt->value, &builder->static_bool, CONFORM_MODE_CALCULATION, stmt->source, bad_condition_error_format);

    if(condition == NULL){
        close_scope(builder);
        return FAILURE;
    }

    // Continue/exit depending on condition and conditional kind
    if(stmt->id == EXPR_WHILE){
        build_cond_break(builder, condition, new_basicblock_id, end_basicblock_id);
    } else {
        build_cond_break(builder, condition, end_basicblock_id, new_basicblock_id);
    }

    // Prepare for block statements
    build_using_basicblock(builder, new_basicblock_id);

    // Generate IR instructions for the statements within the conditional block
    // and terminate the conditional block if it wasn't already terminated
    bool terminated;
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)
    || ir_gen_stmts_auto_terminate(builder, terminated, test_basicblock_id)){
        close_scope(builder);
        return FAILURE;
    }

    // Remove loop label
    if(stmt->label != NULL) pop_loop_label(builder);

    // Close block scope and continue building at the continuation point
    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);

    // Restore previous jump points and scope level
    builder->break_block_id = prev_break_block_id;
    builder->continue_block_id = prev_continue_block_id;
    builder->break_continue_scope = prev_break_continue_scope;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_recurrent_loop(ir_builder_t *builder, ast_expr_whilecontinue_t *stmt){
    length_t new_basicblock_id = build_basicblock(builder);
    length_t end_basicblock_id = build_basicblock(builder);

    // Add loop label
    if(stmt->label != NULL) push_loop_label(builder, stmt->label, end_basicblock_id, new_basicblock_id);

    // Remember previous jump points and scope level
    length_t prev_break_block_id = builder->break_block_id;
    length_t prev_continue_block_id = builder->continue_block_id;
    bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

    // Set the working jump points and scope level
    builder->break_block_id = end_basicblock_id;
    builder->continue_block_id = new_basicblock_id;
    builder->break_continue_scope = builder->scope;

    // Enter block unconditionally
    build_break(builder, new_basicblock_id);
    build_using_basicblock(builder, new_basicblock_id);

    // Open block scope and prepare for block statements
    open_scope(builder);
    build_using_basicblock(builder, new_basicblock_id);

    // Generate IR instructions for the statements within the block
    // and terminate the block if it wasn't already terminated
    bool terminated;
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)
    || ir_gen_stmts_auto_terminate(builder, terminated, stmt->id == EXPR_WHILECONTINUE ? end_basicblock_id : new_basicblock_id)){
        close_scope(builder);
        return FAILURE;
    }

    // Remove loop label
    if(stmt->label != NULL) pop_loop_label(builder);

    // Close block scope and continue building at the continuation point
    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);

    // Restore previous jump points and scope level
    builder->break_block_id = prev_break_block_id;
    builder->continue_block_id = prev_continue_block_id;
    builder->break_continue_scope = prev_break_continue_scope;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_delete(ir_builder_t *builder, ast_expr_unary_t *stmt){
    ir_value_t *pointer_value;
    ast_type_t pointer_type;

    // Generate IR instructions to get the pointer value
    if(ir_gen_expr(builder, stmt->value, &pointer_value, false, &pointer_type)) return FAILURE;

    // Ensure that the value we got is a pointer type
    if(!ast_type_is_pointer(&pointer_type) && !ast_type_is_base_of(&pointer_type, "ptr")){
        char *human_readable_type = ast_type_str(&pointer_type);
        compiler_panicf(builder->compiler, stmt->source, "Can't pass non-pointer type '%s' to delete", human_readable_type);
        ast_type_free(&pointer_type);
        free(human_readable_type);
        return FAILURE;
    }

    ast_type_free(&pointer_type);

    // Build 'free' instruction
    ir_instr_free_t *built_instr = (ir_instr_free_t*) build_instruction(builder, sizeof(ir_instr_free_t));
    built_instr->id = INSTRUCTION_FREE;
    built_instr->result_type = NULL;
    built_instr->value = pointer_value;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_break(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated){
    // Ensure we have a valid point to break to
    if(builder->break_block_id == 0){
        compiler_panicf(builder->compiler, stmt->source, "Nowhere to break to");
        return FAILURE;
    }

    // Make '__defer__()' calls for variables whose scope will end
    if(ir_gen_variable_deference(builder, builder->break_continue_scope)) return FAILURE;

    // Break to the targeted block
    build_break(builder, builder->break_block_id);

    // This statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_continue(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated){
    // Ensure we have a valid point to continue to
    if(builder->continue_block_id == 0){
        compiler_panicf(builder->compiler, stmt->source, "Nowhere to continue to");
        return FAILURE;
    }

    // Make '__defer__()' calls for variables whose scope will end
    if(ir_gen_variable_deference(builder, builder->break_continue_scope)) return FAILURE;

    // Continue to the targeted block
    build_break(builder, builder->continue_block_id);

    // This statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_fallthrough(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated){
    // Ensure we have a valid point to fallthrough to
    if(builder->fallthrough_block_id == 0){
        compiler_panicf(builder->compiler, stmt->source, "Nowhere to fall through to");
        return FAILURE;
    }

    // Make '__defer__()' calls for variables whose scope will end
    if(ir_gen_variable_deference(builder, builder->fallthrough_scope)) return FAILURE;

    // Fallthrough to the targeted block
    build_break(builder, builder->fallthrough_block_id);

    // This statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_break_to(ir_builder_t *builder, ast_expr_break_to_t *stmt, bool *out_is_terminated){
    length_t target_block_id;
    bridge_scope_t *block_scope;

    // Get info about where this loop label is a reference to
    if(!ir_builder_get_loop_label_info(builder, stmt->label, &block_scope, &target_block_id, NULL)){
        compiler_panicf(builder->compiler, stmt->label_source, "Undeclared label '%s'", stmt->label);
        return FAILURE;
    }

    // Make '__defer__()' calls for variables whose scope will end
    ir_gen_variable_deference(builder, block_scope);

    // Break to the targeted block
    build_break(builder, target_block_id);

    // This statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_continue_to(ir_builder_t *builder, ast_expr_break_to_t *stmt, bool *out_is_terminated){
    length_t target_block_id;
    bridge_scope_t *block_scope;

    // Get info about where this loop label is a reference to
    if(!ir_builder_get_loop_label_info(builder, stmt->label, &block_scope, NULL, &target_block_id)){
        compiler_panicf(builder->compiler, stmt->label_source, "Undeclared label '%s'", stmt->label);
        return FAILURE;
    }

    // Make '__defer__()' calls for variables whose scope will end
    if(ir_gen_variable_deference(builder, block_scope)) return FAILURE;

    // Break to the targeted block
    build_break(builder, target_block_id);

    // This statement is always a terminating statement
    if(out_is_terminated) *out_is_terminated = true;
    return SUCCESS;
}

errorcode_t ir_gen_stmt_each(ir_builder_t *builder, ast_expr_each_in_t *stmt){
    // CLEANUP: This function is complicated and complex, but doesn't seem
    // to want to be refactored easily. For the sake of performance and
    // avoiding repetitious code with slight differences, it will have
    // to remain like this for now. - Isaac Shelton, Jan 18 2021

    length_t initial_basicblock_id = builder->current_block_id;

    ast_type_t *idx_ast_type = &builder->object->ast.common.ast_usize_type;
    ir_type_t *idx_ir_type = ir_builder_usize(builder);
    ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);
    
    open_scope(builder);

    // Create 'idx' variable
    add_variable(builder, "idx", idx_ast_type, idx_ir_type, BRIDGE_VAR_POD | BRIDGE_VAR_UNDEF);
    ir_value_t *idx_ptr = build_lvarptr(builder, idx_ir_type_ptr, builder->next_var_id - 1);

    // Set 'idx' to initial value of zero
    build_store(builder, build_literal_usize(builder->pool, 0), idx_ptr, stmt->source);

    length_t prep_basicblock_id = (length_t) -1; // garbage value

    if(!stmt->is_static){
        prep_basicblock_id = build_basicblock(builder);
        build_using_basicblock(builder, prep_basicblock_id);
    }

    ast_expr_phantom_t *single_expr = NULL;

    // NOTE: The follow values only exist if 'single_expr' isn't null
    ir_value_t *single_value = NULL; // (alias for 'single_expr->type')
    ast_type_t single_type;

    if(stmt->list){
        if(ir_gen_expr(builder, stmt->list, &single_value, true, &single_type)) return FAILURE;

        ast_expr_create_phantom((ast_expr_t**) &single_expr, single_type, single_value, stmt->list->source, expr_is_mutable(stmt->list));
    }
    
    ir_value_t *fixed_array_value = NULL;
    ast_type_t temporary_type;

    // Generate length
    ir_value_t *array_length;
    if(single_expr){
        if(ast_type_is_fixed_array(&single_type)){
            // FIXED ARRAY

            ast_type_t remaining_type = ast_type_unwrapped_view(&single_type);
            
            // Verify that the item type matches the item type provided
            if(!ast_types_identical(&remaining_type, stmt->it_type)){
                compiler_panic(builder->compiler, stmt->it_type->source,
                    "Element type doesn't match given array's element type");

                char *s1 = ast_type_str(stmt->it_type);
                char *s2 = ast_type_str(&remaining_type);
                printf("(given element type : '%s', array element type : '%s')\n", s1, s2);
                free(s1);
                free(s2);
                goto failure;
            }

            // Verify that the value that was given is mutable
            if(!single_expr->is_mutable){
                compiler_panicf(builder->compiler, single_type.source, "Fixed array given to 'each in' statement must be mutable");
                goto failure;
            }

            ir_type_t *item_ir_type;
            if(ir_gen_resolve_type(builder->compiler, builder->object, &remaining_type, &item_ir_type))
                goto failure;

            fixed_array_value = build_bitcast(builder, single_value, ir_type_make_pointer_to(builder->pool, item_ir_type));

            // Get array length from the type signature of the fixed array
            // NOTE: Assumes element->id == AST_ELEM_FIXED_ARRAY because of earlier 'ast_type_is_fixed_array' call should've verified
            ast_elem_fixed_array_t *fixed_array_element = (ast_elem_fixed_array_t*) single_type.elements[0];
            array_length = build_literal_usize(builder->pool, fixed_array_element->length);
        } else {
            // STRUCTURE
            // Get array length by calling the __length__() method

            ast_expr_call_method_t length_call;
            ast_expr_create_call_method_in_place(&length_call, "__length__", (ast_expr_t*) single_expr, 0, NULL, false, true, NULL, single_expr->source);

            if(ir_gen_expr(builder, (ast_expr_t*) &length_call, &array_length, false, &temporary_type))
                goto failure;
        }
    } else if(ir_gen_expr(builder, stmt->length, &array_length, false, &temporary_type)){
        goto failure;
    }

    if(!fixed_array_value){
        // Ensure the given value for the array length is of type 'usize'
        if(!ast_types_conform(builder, &array_length, &temporary_type, idx_ast_type, CONFORM_MODE_CALCULATION)){
            char *a_type_str = ast_type_str(&temporary_type);
            compiler_panicf(builder->compiler, stmt->length->source, "Received type '%s' when array length should be 'usize'", a_type_str);
            free(a_type_str);

            ast_type_free(&temporary_type);
            goto failure;
        }

        ast_type_free(&temporary_type);
    }

    if(stmt->is_static){
        // Finally move ahead to prep basicblock for static each-in
        prep_basicblock_id = build_basicblock(builder);
        build_using_basicblock(builder, prep_basicblock_id);
    }

    // Generate (idx < length)
    ir_value_t *idx_value = build_load(builder, idx_ptr, stmt->source);
    ir_value_t *whether_keep_going_value = build_math(builder, INSTRUCTION_ULESSER, idx_value, array_length, ir_builder_bool(builder));

    // Generate body blocks
    length_t new_basicblock_id = build_basicblock(builder);
    length_t inc_basicblock_id = build_basicblock(builder);
    length_t end_basicblock_id = build_basicblock(builder);

    // Hook up labels
    if(stmt->label != NULL) push_loop_label(builder, stmt->label, end_basicblock_id, inc_basicblock_id);
    
    length_t prev_break_block_id = builder->break_block_id;
    length_t prev_continue_block_id = builder->continue_block_id;
    bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

    builder->break_block_id = end_basicblock_id;
    builder->continue_block_id = inc_basicblock_id;
    builder->break_continue_scope = builder->scope;

    // Generate conditional break
    build_cond_break(builder, whether_keep_going_value, new_basicblock_id, end_basicblock_id);

    if(stmt->is_static){
        // Calculate array inside initial basicblock if static
        build_using_basicblock(builder, initial_basicblock_id);
    } else {
        // Calculate array inside new basicblock otherwise
        build_using_basicblock(builder, new_basicblock_id);
    }

    // Generate array value
    ir_value_t *array;
    if(fixed_array_value){
        // FIXED ARRAY
        // We already have the value for the array (since we calculated it when doing fixed-array stuff)
        array = fixed_array_value;
    } else if(single_value){
        // STRUCTURE
        // Call the '__array__()' method to get the value for the array

        ast_expr_call_method_t array_call;
        ast_expr_create_call_method_in_place(&array_call, "__array__", (ast_expr_t*) single_expr, 0, NULL, false, true, NULL, single_expr->source);

        if(ir_gen_expr(builder, (ast_expr_t*) &array_call, &array, false, &temporary_type)){
            close_scope(builder);
            goto failure;
        }
    } else if(ir_gen_expr(builder, stmt->low_array, &array, false, &temporary_type)){
        close_scope(builder);
        goto failure;
    }

    if(!fixed_array_value){
        // Ensure the given value for the array is of a pointer type
        if(!ast_type_is_pointer(&temporary_type)){
            compiler_panic(builder->compiler, stmt->low_array->source,
                "Low-level array type for 'each in' statement must be a pointer");
            ast_type_free(&temporary_type);
            close_scope(builder);
            goto failure;
        }

        // Get the item type
        ast_type_dereference(&temporary_type);

        // Ensure the item type matches the item type provided
        if(!ast_types_identical(&temporary_type, stmt->it_type)){
            compiler_panic(builder->compiler, stmt->it_type->source,
                "Element type doesn't match given array's element type");

            char *s1 = ast_type_str(stmt->it_type);
            char *s2 = ast_type_str(&temporary_type);
            printf("(given element type : '%s', array element type : '%s')\n", s1, s2);
            free(s1);
            free(s2);

            ast_type_free(&temporary_type);
            close_scope(builder);
            goto failure;
        }

        ast_type_free(&temporary_type);
    }
    
    // Finish off initial basic block
    build_using_basicblock(builder, initial_basicblock_id);
    build_break(builder, prep_basicblock_id);

    // Update 'it' inside new basicblock
    build_using_basicblock(builder, new_basicblock_id);

    // Generate new block statements to update 'it' variable
    add_variable(builder, stmt->it_name ? stmt->it_name : "it", stmt->it_type, array->type, BRIDGE_VAR_POD | BRIDGE_VAR_REFERENCE);
    
    ir_value_t *it_ptr = build_lvarptr(builder, array->type, builder->next_var_id - 1);
    ir_value_t *it_idx = build_load(builder, idx_ptr, stmt->source);

    // Update 'it' value
    build_store(builder, build_array_access(builder, array, it_idx, stmt->source), it_ptr, stmt->source);

    // Generate new_block user-defined statements
    bool terminated;
    build_using_basicblock(builder, new_basicblock_id);
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)){
        close_scope(builder);
        goto failure;
    }

    if(!terminated){
        if(handle_deference_for_variables(builder, &builder->scope->list)){
            close_scope(builder);
            goto failure;
        }

        build_break(builder, inc_basicblock_id);
    }

    // Generate jump inc_block
    build_using_basicblock(builder, inc_basicblock_id);

    if(!stmt->is_static && stmt->list && !single_expr->is_mutable){
        // Call '__defer__' on list value and recompute the list if the each-in loop isn't static

        if(handle_single_deference(builder, &single_type, single_value) == ALT_FAILURE){
            close_scope(builder);
            goto failure;
        }
    }

    // Increment
    ir_value_t *incremented = build_math(builder, INSTRUCTION_ADD, build_load(builder, idx_ptr, stmt->source), build_literal_usize(builder->pool, 1), idx_ir_type);
    
    // Store
    build_store(builder, incremented, idx_ptr, stmt->source);

    // Jump Prep
    build_break(builder, prep_basicblock_id);

    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);

    if(stmt->list && !single_expr->is_mutable){
        // Call '__defer__' on list value and recompute the list if the each-in loop isn't static

        if(handle_single_deference(builder, &single_type, single_value) == ALT_FAILURE){
            close_scope(builder);
            goto failure;
        }
    }

    if(single_expr){
        ast_type_free(&single_expr->type);
        free(single_expr);
    }

    if(stmt->label != NULL) pop_loop_label(builder);

    builder->break_block_id = prev_break_block_id;
    builder->continue_block_id = prev_continue_block_id;
    builder->break_continue_scope = prev_break_continue_scope;
    return SUCCESS;

failure:
    if(single_expr){
        ast_type_free(&single_expr->type);
        free(single_expr);
    }

    return FAILURE;
}

errorcode_t ir_gen_stmt_repeat(ir_builder_t *builder, ast_expr_repeat_t *stmt){
    length_t prep_basicblock_id = (length_t) -1; // garbage value
    length_t new_basicblock_id  = build_basicblock(builder);
    length_t inc_basicblock_id  = build_basicblock(builder);
    length_t end_basicblock_id  = build_basicblock(builder);
    
    if(stmt->label != NULL){
        push_loop_label(builder, stmt->label, end_basicblock_id, inc_basicblock_id);
    }

    length_t prev_break_block_id = builder->break_block_id;
    length_t prev_continue_block_id = builder->continue_block_id;
    bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

    builder->break_block_id = end_basicblock_id;
    builder->continue_block_id = inc_basicblock_id;
    builder->break_continue_scope = builder->scope;

    ast_type_t *idx_ast_type = &builder->object->ast.common.ast_usize_type;
    ir_type_t *idx_ir_type = ir_builder_usize(builder);
    ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);
    
    open_scope(builder);

    weak_cstr_t idx_var_name = stmt->idx_overload_name ? stmt->idx_overload_name : "idx";

    // Create 'idx' variable
    add_variable(builder, idx_var_name, idx_ast_type, idx_ir_type, BRIDGE_VAR_POD | BRIDGE_VAR_UNDEF);
    ir_value_t *idx_ptr = build_lvarptr(builder, idx_ir_type_ptr, builder->next_var_id - 1);

    // Set 'idx' to initial value of zero
    ir_value_t *initial_idx = build_literal_usize(builder->pool, 0);

    build_store(builder, initial_idx, idx_ptr, stmt->source);

    if(!stmt->is_static){
        // Use prep block to calculate limit
        prep_basicblock_id = build_basicblock(builder);
        build_break(builder, prep_basicblock_id);
        build_using_basicblock(builder, prep_basicblock_id);
    }

    // Generate length
    const char *error_format = "Received type '%s' when array length should be '%s'";
    ir_value_t *limit = ir_gen_conforming_expr(builder, stmt->limit, idx_ast_type, CONFORM_MODE_CALCULATION, stmt->source, error_format);
    if(limit == NULL) return FAILURE;

    if(stmt->is_static){
        // Use prep block after calculating limit
        prep_basicblock_id = build_basicblock(builder);
        build_break(builder, prep_basicblock_id);
        build_using_basicblock(builder, prep_basicblock_id);
    }

    // Generate (idx < length)
    ir_value_t *idx_value = build_load(builder, idx_ptr, stmt->source);
    ir_value_t *whether_keep_going_value = build_math(builder, INSTRUCTION_ULESSER, idx_value, limit, ir_builder_bool(builder));

    // Generate conditional break
    build_cond_break(builder, whether_keep_going_value, new_basicblock_id, end_basicblock_id);
    build_using_basicblock(builder, new_basicblock_id);

    // Generate new_block user-defined statements
    bool terminated;
    build_using_basicblock(builder, new_basicblock_id);
    if(ir_gen_stmts(builder, &stmt->statements, &terminated)){
        close_scope(builder);
        return FAILURE;
    }

    if(!terminated){
        if(handle_deference_for_variables(builder, &builder->scope->list)){
            close_scope(builder);
            return FAILURE;
        }

        build_break(builder, inc_basicblock_id);
    }

    // Generate jump inc_block
    build_using_basicblock(builder, inc_basicblock_id);

    // Increment
    ir_value_t *current_idx = build_load(builder, idx_ptr, stmt->source);
    ir_value_t *ir_one_value = build_literal_usize(builder->pool, 1);
    ir_value_t *incremented = build_math(builder, INSTRUCTION_ADD, current_idx, ir_one_value, current_idx->type);

    // Store
    build_store(builder, incremented, idx_ptr, stmt->source);

    // Jump Prep
    build_break(builder, prep_basicblock_id);

    close_scope(builder);
    build_using_basicblock(builder, end_basicblock_id);

    if(stmt->label != NULL) pop_loop_label(builder);

    builder->break_block_id = prev_break_block_id;
    builder->continue_block_id = prev_continue_block_id;
    builder->break_continue_scope = prev_break_continue_scope;
    return SUCCESS;
}

errorcode_t exhaustive_switch_check(ir_builder_t *builder, weak_cstr_t enum_name, source_t switch_source, unsigned long long uniqueness_values[], length_t uniqueness_values_length){
    ast_t *ast = &builder->object->ast;
    maybe_index_t enum_index = ast_find_enum(ast->enums, ast->enums_length, enum_name);
    if(enum_index == -1) return SUCCESS;

    // Assumes regular 0..n enum
    ast_enum_t *enum_definition = &ast->enums[enum_index];
    length_t n = enum_definition->length;

    // Don't check enums that have more than 512 elements
    if(n > 512){
        if(uniqueness_values_length < n){
            compiler_panic(builder->compiler, switch_source, "Exhaustive switch with more than 512 elements is missing cases");
            return FAILURE;
        } else if(uniqueness_values_length > n){
            compiler_panic(builder->compiler, switch_source, "Exhaustive switch with more than 512 elements has extraneous cases");
            return FAILURE;
        }

        return SUCCESS;
    }
    
    bool covered[n];
    memset(covered, 0, sizeof(covered));

    for(length_t i = 0; i < uniqueness_values_length; i++){
        if(uniqueness_values[i] >= n){
            compiler_panic(builder->compiler, switch_source, "Exhaustive switch got out of bounds case value");
            return FAILURE;
        }

        covered[uniqueness_values[i]] = true;
    }

    bool is_missing_case = false;
    for(length_t i = 0; i < n; i++) if(!covered[i]){
        // If missing case
        
        if(!is_missing_case){
            is_missing_case = true;

            compiler_panic(builder->compiler, switch_source, "Not all cases covered in exhaustive switch statement");
            printf("\nMissing cases:\n");
        }
        
        printf("    case %s::%s\n", enum_name, enum_definition->kinds[i]);
    }

    return is_missing_case ? FAILURE : SUCCESS;
}


errorcode_t ir_gen_stmts_auto_terminate(ir_builder_t *builder, bool already_terminated, length_t continuation_block_id){
    if(already_terminated) return SUCCESS;

    if(handle_deference_for_variables(builder, &builder->scope->list)) return FAILURE;
    build_break(builder, continuation_block_id);
    return SUCCESS;
}

errorcode_t ir_gen_variable_deference(ir_builder_t *builder, bridge_scope_t *up_until_scope){
    // Make '__defer__()' calls for variables running out of scope

    bridge_scope_t *visit_scope = builder->scope;

    do {
        if(handle_deference_for_variables(builder, &visit_scope->list)) return FAILURE;
        visit_scope = visit_scope->parent;
    } while(visit_scope && visit_scope != up_until_scope);

    return SUCCESS;
}

errorcode_t ir_gen_assign(ir_builder_t *builder, ir_value_t *value, ast_type_t *value_ast_type, ir_value_t *destination, ast_type_t *destination_type, bool force_pod_assignment, source_t source){
    // User defined assignment
    if(!force_pod_assignment){
        errorcode_t errorcode = try_user_defined_assign(builder, value, value_ast_type, destination, destination_type, source);
        if(errorcode == ALT_FAILURE || errorcode == SUCCESS) return errorcode;
    }

    // Regular POD Assignment
    return ir_gen_assign_pod(builder, &value, value_ast_type, destination, destination_type, source);
}

errorcode_t ir_gen_assign_pod(ir_builder_t *builder, ir_value_t **value, ast_type_t *value_ast_type,
        ir_value_t *destination, ast_type_t *destination_ast_type, source_t source_on_error){
    // When doing normal assignment (which is POD), ensure the new value is of the same type

    // Conform initial value to the type of the variable
    if(!ast_types_conform(builder, value, value_ast_type, destination_ast_type, CONFORM_MODE_ASSIGNING)){
        char *a_type_str = ast_type_str(value_ast_type);
        char *b_type_str = ast_type_str(destination_ast_type);
        compiler_panicf(builder->compiler, source_on_error, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);
        return FAILURE;
    }

    build_store(builder, *value, destination, source_on_error);
    return SUCCESS;
}
