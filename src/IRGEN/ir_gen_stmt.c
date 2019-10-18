
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/filename.h"
#include "UTIL/builtin_type.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"
#include "BRIDGE/bridge.h"

errorcode_t ir_gen_func_statements(compiler_t *compiler, object_t *object, length_t ast_func_id, length_t ir_func_id, ir_jobs_t *jobs){
    // ir_gens statements into basicblocks with instructions and sets in 'module_func'

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    // NOTE: These may be invalidated during statement generation
    ast_func_t *ast_func = &object->ast.funcs[ast_func_id];
    ir_func_t *module_func = &object->ir_module.funcs[ir_func_id];

    if(ast_func->statements_length == 0 && !(ast_func->traits & AST_FUNC_GENERATED) && compiler->traits & COMPILER_FUSSY){
        compiler_warnf(compiler, ast_func->source, "Function '%s' is empty", ast_func->name);
    }

    // Used for constructing array of basicblocks
    ir_builder_t builder;
    builder.basicblocks = malloc(sizeof(ir_basicblock_t) * 4);
    builder.basicblocks_length = 1;
    builder.basicblocks_capacity = 4;
    builder.pool = pool;
    builder.type_map = &ir_module->type_map;
    builder.compiler = compiler;
    builder.object = object;
    builder.ast_func_id = ast_func_id;
    builder.ir_func_id = ir_func_id;
    builder.next_var_id = 0;
    builder.has_string_struct = TROOLEAN_UNKNOWN;

    ir_basicblock_t *entry_block = &builder.basicblocks[0];
    entry_block->instructions = malloc(sizeof(ir_instr_t*) * 16);
    entry_block->instructions_length = 0;
    entry_block->instructions_capacity = 16;
    entry_block->traits = TRAIT_NONE;

    // Set current block
    builder.current_block = entry_block;
    builder.current_block_id = 0;

    // Zero indicates no block to continue/break to since block 0 would make no sense
    builder.break_block_id = 0;
    builder.continue_block_id = 0;

    // Block stack, used for breaking and continuing by label
    // NOTE: Unlabeled blocks won't go in this array
    builder.block_stack_labels = NULL;
    builder.block_stack_break_ids = NULL;
    builder.block_stack_continue_ids = NULL;
    builder.block_stack_scopes = NULL;
    builder.block_stack_length = 0;
    builder.block_stack_capacity = 0;

    module_func->scope = malloc(sizeof(bridge_scope_t));
    bridge_scope_init(module_func->scope, NULL);
    module_func->scope->first_var_id = 0;
    builder.scope = module_func->scope;
    builder.jobs = jobs;

    builder.static_bool_base.id = AST_ELEM_BASE;
    builder.static_bool_base.source = NULL_SOURCE;
    builder.static_bool_base.source.object_index = builder.object->index;
    builder.static_bool_base.base = "bool";
    builder.static_bool_elems = (ast_elem_t*) &builder.static_bool_base;
    builder.static_bool.elements = &builder.static_bool_elems;
    builder.static_bool.elements_length = 1;
    builder.static_bool.source = NULL_SOURCE;
    builder.static_bool.source.object_index = builder.object->index;

    while(module_func->arity != ast_func->arity){
        if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])){
            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;

            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }
        
        trait_t arg_traits = BRIDGE_VAR_UNDEF;
        if(ast_func->arg_type_traits[module_func->arity] & AST_FUNC_ARG_TYPE_TRAIT_POD) arg_traits |= BRIDGE_VAR_POD;
        add_variable(&builder, ast_func->arg_names[module_func->arity], &ast_func->arg_types[module_func->arity], module_func->argument_types[module_func->arity], arg_traits);
        module_func->arity++;
    }

    ast_expr_t **statements = ast_func->statements;
    length_t statements_length = ast_func->statements_length;

    if(ast_func->traits & AST_FUNC_MAIN){
        // Initialize all global variables
        if(ir_gen_globals_init(&builder)){
            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;

            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }
    }

    bool terminated;
    if(ir_gen_statements(&builder, statements, statements_length, &terminated)){
        // Make sure to update 'module_func' because ir_module.funcs may have been moved
        module_func = &object->ir_module.funcs[ir_func_id];
        module_func->basicblocks = builder.basicblocks;
        module_func->basicblocks_length = builder.basicblocks_length;

        free(builder.block_stack_labels);
        free(builder.block_stack_break_ids);
        free(builder.block_stack_continue_ids);
        free(builder.block_stack_scopes);
        return FAILURE;
    }

    // Append return instr for functions that return void
    if(!terminated){
        handle_deference_for_variables(&builder, &builder.scope->list);

        if(ast_func->traits & AST_FUNC_MAIN){
            handle_deference_for_globals(&builder);
        }

        // Make sure to update references that may have been invalidated
        ast_func = &object->ast.funcs[ast_func_id];

        if(ast_func->traits & AST_FUNC_DEFER && handle_children_deference(&builder)){
            // Failed to auto-generate __defer__() calls to children of parent type
            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }

        // Ensure latest version of module_func reference
        ast_func = &object->ast.funcs[ast_func_id];
        module_func = &object->ir_module.funcs[ir_func_id];

        if(module_func->return_type->kind == TYPE_KIND_VOID){
            ir_instr_t *built_instr = build_instruction(&builder, sizeof(ir_instr_ret_t));
            ((ir_instr_ret_t*) built_instr)->id = INSTRUCTION_RET;
            ((ir_instr_ret_t*) built_instr)->value = NULL;
        } else if(ast_func->traits & AST_FUNC_MAIN && module_func->return_type->kind == TYPE_KIND_S32
                    && ast_type_is_void(&ast_func->return_type)){
            // Return an int under the hood for 'func main void'
            ir_instr_t *built_instr = build_instruction(&builder, sizeof(ir_instr_ret_t));
            ((ir_instr_ret_t*) built_instr)->id = INSTRUCTION_RET;
            ((ir_instr_ret_t*) built_instr)->value = build_literal_int(builder.pool, 0);
        } else {
            source_t where = (statements_length != 0) ? statements[statements_length - 1]->source : ast_func->source;
            char *return_typename = ast_type_str(&ast_func->return_type);
            compiler_panicf(compiler, where, "Must return a value of type '%s' before exiting function '%s'", return_typename, ast_func->name);
            free(return_typename);

            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;

            free(builder.block_stack_labels);
            free(builder.block_stack_break_ids);
            free(builder.block_stack_continue_ids);
            free(builder.block_stack_scopes);
            return FAILURE;
        }
    }

    // Make sure to update references that may have been invalidated
    module_func = &object->ir_module.funcs[ir_func_id];

    module_func->scope->following_var_id = builder.next_var_id;
    module_func->variable_count = builder.next_var_id;
    module_func->basicblocks = builder.basicblocks;
    module_func->basicblocks_length = builder.basicblocks_length;

    free(builder.block_stack_labels);
    free(builder.block_stack_break_ids);
    free(builder.block_stack_continue_ids);
    free(builder.block_stack_scopes);
    return SUCCESS;
}

errorcode_t ir_gen_statements(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length, bool *out_is_terminated){
    ir_instr_t *built_instr;
    ir_instr_t **instr = NULL;
    ir_value_t *expression_value = NULL;
    ast_type_t temporary_type;

    if(out_is_terminated) *out_is_terminated = false;

    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN: {        
                // DANGEROUS: Could be invalidated by 'ir_gen_expression' call, in which case it should no longer be used
                ast_func_t *ast_func = &builder->object->ast.funcs[builder->ast_func_id];
                ast_type_t *return_type = &ast_func->return_type;
                ast_expr_return_t *return_stmt = (ast_expr_return_t*) statements[s];

                if(return_stmt->value != NULL){
                    // Return non-void value
                    if(ir_gen_expression(builder, return_stmt->value, &expression_value, false, &temporary_type)) return FAILURE;

                    if(!ast_types_conform(builder, &expression_value, &temporary_type, return_type, CONFORM_MODE_CALCULATION)){
                        char *a_type_str = ast_type_str(&temporary_type);
                        char *b_type_str = ast_type_str(return_type);
                        compiler_panicf(builder->compiler, statements[s]->source, "Attempting to return type '%s' when function expects type '%s'", a_type_str, b_type_str);
                        free(a_type_str);
                        free(b_type_str);
                        ast_type_free(&temporary_type);
                        return FAILURE;
                    }

                    ast_type_free(&temporary_type);
                } else if(ast_func->traits & AST_FUNC_MAIN && ast_type_is_void(return_type)){
                    // Return 0 if in main function and it returns void
                    expression_value = build_literal_int(builder->pool, 0);
                } else {
                    // Return void
                    expression_value = NULL;

                    if(return_type->elements_length != 1 || return_type->elements[0]->id != AST_ELEM_BASE
                            || strcmp(((ast_elem_base_t*) return_type->elements[0])->base, "void") != 0){
                        // This function expects a value to returned, not void
                        char *a_type_str = ast_type_str(return_type);
                        compiler_panicf(builder->compiler, return_stmt->source, "Attempting to return void when function expects type '%s'", a_type_str);
                        free(a_type_str);
                        return FAILURE;
                    }
                }

                // Update ast_func in case 'ast.funcs' moved
                ast_func = &builder->object->ast.funcs[builder->ast_func_id];

                // Handle deferred statements
                bool illegal_termination;
                if(ir_gen_statements(builder, return_stmt->last_minute.statements, return_stmt->last_minute.length, &illegal_termination)){
                    return FAILURE;
                }

                if(illegal_termination){
                    compiler_panicf(builder->compiler, return_stmt->source, "Cannot expand a previously deferred terminating statement");
                    return FAILURE;
                }

                // Handle __defer__ calls
                bridge_scope_t *visit_scope;
                for(visit_scope = builder->scope; visit_scope->parent != NULL; visit_scope = visit_scope->parent){
                    handle_deference_for_variables(builder, &visit_scope->list);
                }

                handle_deference_for_variables(builder, &visit_scope->list);

                if(ast_func->traits & AST_FUNC_MAIN){
                    handle_deference_for_globals(builder);
                }

                if(ast_func->traits & AST_FUNC_DEFER && handle_children_deference(builder)){
                    // Failed to auto-generate __defer__() calls to children of parent type
                    return FAILURE;
                }

                built_instr = build_instruction(builder, sizeof(ir_instr_ret_t));
                ((ir_instr_ret_t*) built_instr)->id = INSTRUCTION_RET;
                ((ir_instr_ret_t*) built_instr)->result_type = NULL;
                ((ir_instr_ret_t*) built_instr)->value = expression_value;

                if(s + 1 != statements_length){
                    compiler_warnf(builder->compiler, statements[s + 1]->source, "Statements after 'return' in function '%s'", builder->object->ast.funcs[builder->ast_func_id].name);
                }

                if(out_is_terminated) *out_is_terminated = true;
                return SUCCESS; // Return because no other statements can be after this one
            }
        case EXPR_CALL: {
                ast_expr_call_t *call_expr = (ast_expr_call_t*) statements[s];
                ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * call_expr->arity);
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * call_expr->arity);

                ast_type_t *ast_var_type;
                ir_type_t *ir_var_type;
                ast_t *ast = &builder->object->ast;
                bool hard_break = false;
                ir_value_t *var_value;

                // Resolve & ir_gen function arguments
                for(length_t a = 0; a != call_expr->arity; a++){
                    if(ir_gen_expression(builder, call_expr->args[a], &arg_values[a], false, &arg_types[a])){
                        ast_types_free_fully(arg_types, a);
                        return FAILURE;
                    }
                }

                // Check for non-global variable
                bridge_var_t *var = bridge_scope_find_var(builder->scope, call_expr->name);

                if(var){
                    ast_var_type = var->ast_type;
                    ir_var_type = ir_type_pointer_to(builder->pool, var->ir_type);

                    if(ast_var_type->elements_length != 1 || ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                        if(call_expr->is_tentative) break;
                        char *s = ast_type_str(ast_var_type);
                        compiler_panicf(builder->compiler, call_expr->source, "Can't call value of non function type '%s'", s);
                        ast_types_free_fully(arg_types, call_expr->arity);
                        free(s);
                        return FAILURE;
                    }

                    var_value = build_varptr(builder, ir_var_type, var->id);
                    var_value = build_load(builder, var_value);

                    errorcode_t error = ir_gen_call_function_value(builder, ast_var_type, ir_var_type, call_expr, arg_values, arg_types, &var_value, NULL);
                    ast_types_free_fully(arg_types, call_expr->arity);

                    if(error){
                        if(error == ALT_FAILURE){
                            break;
                        } else {
                            return FAILURE;
                        }
                    }
                    
                    break;
                }

                // If no non-global variable exists, look for function
                funcpair_t pair;
                errorcode_t error = ir_gen_find_func_conforming(builder, call_expr->name, arg_values, arg_types, call_expr->arity, &pair);

                if(error == SUCCESS){
                    // Found function that fits given name and arguments

                    if(pair.ast_func->traits & AST_FUNC_VARARG){
                        trait_t arg_type_traits[call_expr->arity];
                        memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
                        memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (call_expr->arity - pair.ast_func->arity));
                        handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity);
                    } else {
                        handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity);
                    }

                    ir_basicblock_new_instructions(builder->current_block, 1);
                    ir_instr_call_t *instruction = (ir_instr_call_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                    instruction->id = INSTRUCTION_CALL;
                    instruction->result_type = pair.ir_func->return_type;
                    instruction->values = arg_values;
                    instruction->values_length = call_expr->arity;
                    instruction->ir_func_id = pair.ir_func_id;
                    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
                    ast_types_free_fully(arg_types, call_expr->arity);
                    break;
                }


                // If no function exists, look for global
                // TODO: SPEED: Make this so it isn't slow
                for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(ast->globals[g].name, call_expr->name) == 0){
                        ast_var_type = &ast->globals[g].type;

                        // We could search the ir var map, but this is easier
                        // TODO: Create easy way to get between ast and ir var maps
                        ir_var_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                        ir_var_type->kind = TYPE_KIND_POINTER;

                        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_var_type, (ir_type_t**) &ir_var_type->extra)){
                            ast_types_free_fully(arg_types, call_expr->arity);
                            return FAILURE;
                        }

                        if(ast_var_type->elements_length != 1 || ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                            if(call_expr->is_tentative){
                                ast_types_free_fully(arg_types, call_expr->arity);
                                hard_break = true;
                                break;
                            }

                            char *s = ast_type_str(ast_var_type);
                            compiler_panicf(builder->compiler, call_expr->source, "Can't call value of non function type '%s'", s);
                            ast_types_free_fully(arg_types, call_expr->arity);
                            free(s);
                            return FAILURE;
                        }

                        var_value = build_gvarptr(builder, ir_var_type, g);
                        var_value = build_load(builder, var_value);

                        errorcode_t error = ir_gen_call_function_value(builder, ast_var_type, ir_var_type, call_expr, arg_values, arg_types, &var_value, NULL);
                        ast_types_free_fully(arg_types, call_expr->arity);

                        if(error){
                            if(error == ALT_FAILURE){
                                hard_break = true;
                                break;
                            } else {
                                return FAILURE;
                            }
                        }
                        
                        hard_break = true;
                        break;
                    }
                }

                if(hard_break) break;

                // Otherwise no function was found
                if(call_expr->is_tentative){
                    ast_types_free_fully(arg_types, call_expr->arity);
                    break;
                } else {
                    compiler_undeclared_function(builder->compiler, builder->object, call_expr->source, call_expr->name, arg_types, call_expr->arity);
                    ast_types_free_fully(arg_types, call_expr->arity);
                }

                return FAILURE;
            }
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                ast_expr_declare_t *declare_stmt = ((ast_expr_declare_t*) statements[s]);

                // Search for existing variable named that
                if(bridge_scope_var_already_in_list(builder->scope, declare_stmt->name)){
                    compiler_panicf(builder->compiler, statements[s]->source, "Variable '%s' already declared", declare_stmt->name);
                    return FAILURE;
                }

                ir_type_t *ir_decl_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                if(ir_gen_resolve_type(builder->compiler, builder->object, &declare_stmt->type, &ir_decl_type)) return FAILURE;

                if(declare_stmt->value != NULL){
                    // Regular declare statement initial assign value
                    ir_value_t *initial;
                    ir_value_t *destination = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                    ir_type_t *var_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                    var_pointer_type->kind = TYPE_KIND_POINTER;
                    var_pointer_type->extra = ir_decl_type;

                    if(ir_gen_expression(builder, declare_stmt->value, &initial, false, &temporary_type)) return FAILURE;

                    if(!ast_types_conform(builder, &initial, &temporary_type, &declare_stmt->type, CONFORM_MODE_ASSIGNING)){
                        char *a_type_str = ast_type_str(&temporary_type);
                        char *b_type_str = ast_type_str(&declare_stmt->type);
                        compiler_panicf(builder->compiler, declare_stmt->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
                        free(a_type_str);
                        free(b_type_str);
                        ast_type_free(&temporary_type);
                        return FAILURE;
                    }


                    ir_basicblock_new_instructions(builder->current_block, 2);
                    instr = &builder->current_block->instructions[builder->current_block->instructions_length++];
                    *instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
                    ((ir_instr_varptr_t*) *instr)->id = INSTRUCTION_VARPTR;
                    ((ir_instr_varptr_t*) *instr)->index = builder->next_var_id;
                    ((ir_instr_varptr_t*) *instr)->result_type = var_pointer_type;

                    destination->value_type = VALUE_TYPE_RESULT;
                    destination->type = var_pointer_type;
                    destination->extra = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
                    ((ir_value_result_t*) (destination)->extra)->block_id = builder->current_block_id;
                    ((ir_value_result_t*) (destination)->extra)->instruction_id = builder->current_block->instructions_length - 1;

                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, declare_stmt->is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

                    if(declare_stmt->is_assign_pod || !handle_assign_management(builder, initial, &temporary_type, destination, &declare_stmt->type, true)){
                        build_store(builder, initial, destination);
                    }

                    ast_type_free(&temporary_type);
                } else if(statements[s]->id == EXPR_DECLAREUNDEF && !(builder->compiler->traits & COMPILER_NO_UNDEF)){
                    // Mark the variable as undefined memory so it isn't auto-initialized later on
                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, declare_stmt->is_pod ? BRIDGE_VAR_UNDEF | BRIDGE_VAR_POD : BRIDGE_VAR_UNDEF);
                } else /* plain DECLARE or --no-undef DECLAREUNDEF */ {
                    // Variable declaration without default value
                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, declare_stmt->is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

                    // Zero initialize the variable
                    ir_basicblock_new_instructions(builder->current_block, 1);
                    instr = &builder->current_block->instructions[builder->current_block->instructions_length++];
                    *instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varzeroinit_t));
                    ((ir_instr_varzeroinit_t*) *instr)->id = INSTRUCTION_VARZEROINIT;
                    ((ir_instr_varzeroinit_t*) *instr)->result_type = NULL;
                    ((ir_instr_varzeroinit_t*) *instr)->index = builder->next_var_id - 1;
                }
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
        case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN: {
                unsigned int assignment_type = statements[s]->id;
                ast_expr_assign_t *assign_stmt = ((ast_expr_assign_t*) statements[s]);
                ir_value_t *destination;
                ast_type_t destination_type, expression_value_type;
                if(ir_gen_expression(builder, assign_stmt->value, &expression_value, false, &expression_value_type)) return FAILURE;
                if(ir_gen_expression(builder, assign_stmt->destination, &destination, true, &destination_type)){
                    ast_type_free(&expression_value_type);
                    return FAILURE;
                }

                if(!ast_types_conform(builder, &expression_value, &expression_value_type, &destination_type, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&expression_value_type);
                    char *b_type_str = ast_type_str(&destination_type);
                    compiler_panicf(builder->compiler, assign_stmt->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&destination_type);
                    ast_type_free(&expression_value_type);
                    return FAILURE;
                }

                ir_value_t *instr_value;

                if(assignment_type == EXPR_ASSIGN){
                    if(assign_stmt->is_pod || !handle_assign_management(builder, expression_value, &expression_value_type, destination, &destination_type, false)){
                        build_store(builder, expression_value, destination);
                    }

                    ast_type_free(&destination_type);
                    ast_type_free(&expression_value_type);
                } else {
                    ast_type_free(&destination_type);
                    ast_type_free(&expression_value_type);

                    instr_value = build_load(builder, destination);

                    ir_value_result_t *value_result;
                    value_result = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
                    value_result->block_id = builder->current_block_id;
                    value_result->instruction_id = builder->current_block->instructions_length;

                    built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                    ((ir_instr_math_t*) built_instr)->result_type = expression_value->type;
                    ((ir_instr_math_t*) built_instr)->a = instr_value;
                    ((ir_instr_math_t*) built_instr)->b = expression_value;

                    switch(assignment_type){
                    case EXPR_ADDASSIGN:
                        if(i_vs_f_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_ADD, INSTRUCTION_FADD)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't add those types");
                            return FAILURE;
                        }
                        break;
                    case EXPR_SUBTRACTASSIGN:
                        if(i_vs_f_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't subtract those types");
                            return FAILURE;
                        }
                        break;
                    case EXPR_MULTIPLYASSIGN:
                        if(i_vs_f_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_MULTIPLY, INSTRUCTION_FMULTIPLY)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't multiply those types");
                            return FAILURE;
                        }
                        break;
                    case EXPR_DIVIDEASSIGN:
                        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_UDIVIDE, INSTRUCTION_SDIVIDE, INSTRUCTION_FDIVIDE)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't divide those types");
                            return FAILURE;
                        }
                        break;
                    case EXPR_MODULUSASSIGN:
                        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_UMODULUS, INSTRUCTION_SMODULUS, INSTRUCTION_FMODULUS)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't take the modulus of those types");
                            return FAILURE;
                        }
                        break;
                    default:
                        compiler_panic(builder->compiler, assign_stmt->source, "INTERNAL ERROR: ir_gen_statements() got unknown assignment operator id");
                        return FAILURE;
                    }

                    instr_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                    instr_value->value_type = VALUE_TYPE_RESULT;
                    instr_value->type = expression_value->type;
                    instr_value->extra = value_result;

                    build_store(builder, instr_value, destination);
                }
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: {
                unsigned int conditional_type = statements[s]->id;
                if(ir_gen_expression(builder, ((ast_expr_if_t*) statements[s])->value, &expression_value, false, &temporary_type)) return FAILURE;

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &builder->static_bool, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&builder->static_bool);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                length_t new_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = expression_value;

                if(conditional_type == EXPR_IF){
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = new_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = end_basicblock_id;
                } else {
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = end_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = new_basicblock_id;
                }

                ast_expr_t **if_stmts = ((ast_expr_if_t*) statements[s])->statements;
                length_t if_stmts_length = ((ast_expr_if_t*) statements[s])->statements_length;
                bool terminated;

                open_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, if_stmts, if_stmts_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, end_basicblock_id);
                }

                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                unsigned int conditional_type = statements[s]->id;
                if(ir_gen_expression(builder, ((ast_expr_ifelse_t*) statements[s])->value, &expression_value, false, &temporary_type)) return FAILURE;

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &builder->static_bool, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&builder->static_bool);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                length_t new_basicblock_id = build_basicblock(builder);
                length_t else_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = expression_value;

                if(conditional_type == EXPR_IFELSE){
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = new_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = else_basicblock_id;
                } else {
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = else_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = new_basicblock_id;
                }

                ast_expr_t **if_stmts = ((ast_expr_ifelse_t*) statements[s])->statements;
                length_t if_stmts_length = ((ast_expr_ifelse_t*) statements[s])->statements_length;
                bool terminated;

                open_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);

                if(ir_gen_statements(builder, if_stmts, if_stmts_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, end_basicblock_id);
                }

                close_scope(builder);

                ast_expr_t **else_stmts = ((ast_expr_ifelse_t*) statements[s])->else_statements;
                length_t else_stmts_length = ((ast_expr_ifelse_t*) statements[s])->else_statements_length;

                open_scope(builder);
                build_using_basicblock(builder, else_basicblock_id);
                if(ir_gen_statements(builder, else_stmts, else_stmts_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, end_basicblock_id);
                }

                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);
            }
            break;
        case EXPR_WHILE: case EXPR_UNTIL: {
                length_t test_basicblock_id = build_basicblock(builder);
                length_t new_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                if(((ast_expr_while_t*) statements[s])->label != NULL){
                    prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = ((ast_expr_while_t*) statements[s])->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = test_basicblock_id;
                    builder->block_stack_scopes[builder->block_stack_length] = builder->scope;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;
                bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = test_basicblock_id;
                builder->break_continue_scope = builder->scope;

                build_break(builder, test_basicblock_id);
                build_using_basicblock(builder, test_basicblock_id);

                unsigned int conditional_type = statements[s]->id;
                if(ir_gen_expression(builder, ((ast_expr_while_t*) statements[s])->value, &expression_value, false, &temporary_type)) return FAILURE;

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

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &bool_type, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&bool_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = expression_value;

                if(conditional_type == EXPR_WHILE){
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = new_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = end_basicblock_id;
                } else {
                    ((ir_instr_cond_break_t*) built_instr)->true_block_id = end_basicblock_id;
                    ((ir_instr_cond_break_t*) built_instr)->false_block_id = new_basicblock_id;
                }

                ast_expr_t **while_stmts = ((ast_expr_while_t*) statements[s])->statements;
                length_t while_stmts_length = ((ast_expr_while_t*) statements[s])->statements_length;
                bool terminated;

                open_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, while_stmts, while_stmts_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, test_basicblock_id);
                }

                if(((ast_expr_while_t*) statements[s])->label != NULL) builder->block_stack_length--;
                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
                builder->break_continue_scope = prev_break_continue_scope;
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                length_t new_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                if(((ast_expr_whilecontinue_t*) statements[s])->label != NULL){
                    prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = ((ast_expr_whilecontinue_t*) statements[s])->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = new_basicblock_id;
                    builder->block_stack_scopes[builder->block_stack_length] = builder->scope;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;
                bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = new_basicblock_id;
                builder->break_continue_scope = builder->scope;

                build_break(builder, new_basicblock_id);
                build_using_basicblock(builder, new_basicblock_id);

                ast_expr_t **loop_stmts = ((ast_expr_whilecontinue_t*) statements[s])->statements;
                length_t loop_stmts_length = ((ast_expr_whilecontinue_t*) statements[s])->statements_length;
                bool terminated;

                open_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, loop_stmts, loop_stmts_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);

                    if(statements[s]->id == EXPR_WHILECONTINUE){
                        // 'while continue'
                        build_break(builder, end_basicblock_id);
                    } else {
                        // 'until break'
                        build_break(builder, new_basicblock_id);
                    }
                }

                if(((ast_expr_whilecontinue_t*) statements[s])->label != NULL) builder->block_stack_length--;
                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
                builder->break_continue_scope = prev_break_continue_scope;
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (call_stmt->arity + 1));
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * (call_stmt->arity + 1));

                // Generate primary argument
                if(ir_gen_expression(builder, call_stmt->value, &arg_values[0], true, &arg_types[0])){
                    free(arg_types);
                    return FAILURE;
                }

                // Validate and prepare primary argument
                ast_elem_t **type_elems = arg_types[0].elements;
                length_t type_elems_length = arg_types[0].elements_length;

                if(type_elems_length == 1 && (type_elems[0]->id == AST_ELEM_BASE || type_elems[0]->id == AST_ELEM_GENERIC_BASE)){
                    ast_type_prepend_ptr(&arg_types[0]);
                } else if(type_elems_length == 2 && type_elems[0]->id == AST_ELEM_POINTER
                        && (type_elems[1]->id == AST_ELEM_BASE || type_elems[1]->id == AST_ELEM_GENERIC_BASE)){
                    // Load the value that's being called on
                    arg_values[0] = build_load(builder, arg_values[0]);
                } else {
                    char *s = ast_type_str(&arg_types[0]);
                    compiler_panicf(builder->compiler, call_stmt->source, "Can't call methods on type '%s'", s);
                    ast_type_free(&arg_types[0]);
                    free(arg_types);
                    free(s);
                    return FAILURE;
                }

                // Generate secondary argument values
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(ir_gen_expression(builder, call_stmt->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
                        ast_types_free_fully(arg_types, a + 1);
                        return FAILURE;
                    }
                }

                // Find appropriate method
                funcpair_t pair;
                bool tentative_fell_through = false;
                switch(arg_types[0].elements[1]->id){
                case AST_ELEM_BASE: {
                        const char *struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;
                
                        // Find function that fits given name and arguments        
                        if(ir_gen_find_method_conforming(builder, struct_name, call_stmt->name,
                                arg_values, arg_types, call_stmt->arity + 1, &pair)){
                            if(call_stmt->is_tentative){
                                tentative_fell_through = true;
                                break;
                            }

                            compiler_panicf(builder->compiler, call_stmt->source, "Undeclared method '%s'", call_stmt->name);
                            ast_types_free_fully(arg_types, call_stmt->arity + 1);
                            return FAILURE;
                        }
                        
                        break;
                    }
                case AST_ELEM_GENERIC_BASE: {
                        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[1];

                        if(generic_base->name_is_polymorphic){
                            if(call_stmt->is_tentative){
                                tentative_fell_through = true;
                                break;
                            }

                            compiler_panic(builder->compiler, generic_base->source, "Can't call method on struct value with unresolved polymorphic name");
                            ast_types_free_fully(arg_types, call_stmt->arity + 1);
                            return FAILURE;
                        }

                        if(ir_gen_find_generic_base_method_conforming(builder, generic_base->name, call_stmt->name, arg_values, arg_types, call_stmt->arity + 1, &pair)){
                            if(call_stmt->is_tentative){
                                tentative_fell_through = true;
                                break;
                            }

                            compiler_panicf(builder->compiler, call_stmt->source, "Undeclared method '%s'", call_stmt->name);
                            ast_types_free_fully(arg_types, call_stmt->arity + 1);
                            return FAILURE;
                        }

                        break;
                    }
                default:
                    redprintf("INTERNAL ERROR: EXPR_CALL_METHOD in ir_gen_stmt.c received bad primary element id\n")
                    ast_types_free_fully(arg_types, call_stmt->arity + 1);
                    return FAILURE;
                }

                if(tentative_fell_through){
                    ast_types_free_fully(arg_types, call_stmt->arity + 1);
                    break;
                }

                if(pair.ast_func->traits & AST_FUNC_VARARG){
                    trait_t arg_type_traits[call_stmt->arity + 1];
                    memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
                    memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (call_stmt->arity + 1 - pair.ast_func->arity));
                    handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_stmt->arity + 1);
                } else {
                    handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_stmt->arity + 1);
                }

                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_call_t* instruction = (ir_instr_call_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                instruction->id = INSTRUCTION_CALL;
                instruction->result_type = pair.ir_func->return_type;
                instruction->values = arg_values;
                instruction->values_length = call_stmt->arity + 1;
                instruction->ir_func_id = pair.ir_func_id;
                builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
                ast_types_free_fully(arg_types, call_stmt->arity + 1);
            }
            break;
        case EXPR_DELETE: {
                ast_expr_unary_t *delete_expr = (ast_expr_unary_t*) statements[s];
                if(ir_gen_expression(builder, delete_expr->value, &expression_value, false, &temporary_type)) return FAILURE;

                if(temporary_type.elements_length == 0 || ( temporary_type.elements[0]->id != AST_ELEM_POINTER &&
                    !(temporary_type.elements[0]->id == AST_ELEM_BASE && strcmp(((ast_elem_base_t*) temporary_type.elements[0])->base, "ptr") == 0)
                )){
                    char *t = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, delete_expr->source, "Can't pass non-pointer type '%s' to delete", t);
                    ast_type_free(&temporary_type);
                    free(t);
                    return FAILURE;
                }

                built_instr = build_instruction(builder, sizeof(ir_instr_free_t));
                ((ir_instr_free_t*) built_instr)->id = INSTRUCTION_FREE;
                ((ir_instr_free_t*) built_instr)->result_type = NULL;
                ((ir_instr_free_t*) built_instr)->value = expression_value;
                ast_type_free(&temporary_type);
            }
            break;
        case EXPR_BREAK: {
                if(builder->break_block_id == 0){
                    compiler_panicf(builder->compiler, statements[s]->source, "Nowhere to break to");
                    return FAILURE;
                }

                bridge_scope_t *visit_scope;
                for(visit_scope = builder->scope; visit_scope->parent != builder->break_continue_scope; visit_scope = visit_scope->parent){
                    handle_deference_for_variables(builder, &visit_scope->list);
                }
                handle_deference_for_variables(builder, &visit_scope->list);
                build_break(builder, builder->break_block_id);
                if(out_is_terminated) *out_is_terminated = true;
            }
            return SUCCESS;
        case EXPR_CONTINUE: {
                if(builder->continue_block_id == 0){
                    compiler_panicf(builder->compiler, statements[s]->source, "Nowhere to continue to");
                    return FAILURE;
                }

                bridge_scope_t *visit_scope;
                for(visit_scope = builder->scope; visit_scope->parent != builder->break_continue_scope; visit_scope = visit_scope->parent){
                    handle_deference_for_variables(builder, &visit_scope->list);
                }
                handle_deference_for_variables(builder, &visit_scope->list);
                build_break(builder, builder->continue_block_id);
                if(out_is_terminated) *out_is_terminated = true;
            }
            return SUCCESS;
        case EXPR_BREAK_TO: {
                char *target_label = ((ast_expr_break_to_t*) statements[s])->label;
                length_t target_block_id = 0;
                bridge_scope_t *block_scope;

                for(length_t i = builder->block_stack_length - 1; i != -1 /* overflow occurs */; i--){
                    if(strcmp(target_label, builder->block_stack_labels[i]) == 0){
                        target_block_id = builder->block_stack_break_ids[i];
                        block_scope = builder->block_stack_scopes[i];
                        break;
                    }
                }

                if(target_block_id == 0){
                    compiler_panicf(builder->compiler, ((ast_expr_break_to_t*) statements[s])->label_source, "Undeclared label '%s'", target_label);
                    return FAILURE;
                }

                bridge_scope_t *visit_scope;
                for(visit_scope = builder->scope; visit_scope->parent != block_scope; visit_scope = visit_scope->parent){
                    handle_deference_for_variables(builder, &visit_scope->list);
                }
                handle_deference_for_variables(builder, &visit_scope->list);
                build_break(builder, target_block_id);
                if(out_is_terminated) *out_is_terminated = true;
            }
            return SUCCESS;
        case EXPR_CONTINUE_TO: {
                char *target_label = ((ast_expr_continue_to_t*) statements[s])->label;
                length_t target_block_id = 0;
                bridge_scope_t *block_scope;

                for(length_t i = builder->block_stack_length - 1; i != -1 /* overflow occurs */; i--){
                    if(strcmp(target_label, builder->block_stack_labels[i]) == 0){
                        target_block_id = builder->block_stack_continue_ids[i];
                        block_scope = builder->block_stack_scopes[i];
                        break;
                    }
                }

                if(target_block_id == 0){
                    compiler_panicf(builder->compiler, ((ast_expr_continue_to_t*) statements[s])->label_source, "Undeclared label '%s'", target_label);
                    return FAILURE;
                }

                bridge_scope_t *visit_scope;
                for(visit_scope = builder->scope; visit_scope->parent != block_scope; visit_scope = visit_scope->parent){
                    handle_deference_for_variables(builder, &visit_scope->list);
                }
                handle_deference_for_variables(builder, &visit_scope->list);
                build_break(builder, target_block_id);
                if(out_is_terminated) *out_is_terminated = true;
            }
            return SUCCESS;
        case EXPR_EACH_IN: {
                ast_expr_each_in_t *each_in = (ast_expr_each_in_t*) statements[s];

                length_t prep_basicblock_id = build_basicblock(builder);
                length_t new_basicblock_id  = build_basicblock(builder);
                length_t inc_basicblock_id  = build_basicblock(builder);
                length_t end_basicblock_id  = build_basicblock(builder);
                
                if(each_in->label != NULL){
                    prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = each_in->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = inc_basicblock_id;
                    builder->block_stack_scopes[builder->block_stack_length] = builder->scope;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;
                bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = inc_basicblock_id;
                builder->break_continue_scope = builder->scope;

                ast_type_t *idx_ast_type = ast_get_usize(&builder->object->ast);

                ir_type_t *idx_ir_type = ir_builder_usize(builder);
                ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);

                open_scope(builder);

                // Create 'idx' variable
                length_t idx_var_id = builder->next_var_id;
                add_variable(builder, "idx", idx_ast_type, idx_ir_type, BRIDGE_VAR_POD | BRIDGE_VAR_UNDEF);
                ir_value_t *idx_ptr = build_varptr(builder, idx_ir_type_ptr, idx_var_id);

                // Set 'idx' to inital value of zero
                ir_value_t *initial_idx = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                initial_idx->value_type = VALUE_TYPE_LITERAL;
                initial_idx->type = idx_ir_type;
                initial_idx->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
                *((unsigned long long*) initial_idx->extra) = 0;

                build_store(builder, initial_idx, idx_ptr);
                build_break(builder, prep_basicblock_id);
                build_using_basicblock(builder, prep_basicblock_id);

                // DANGEROUS: The following chunks of code assume that either 'each_in->list' isn't null or
                //        'each_in->array' and 'each_in->length' aren't null
                ir_value_t *list_precomputed = NULL;

                // NOTE: 'phantom_list_value.type' must be passed to ast_type_free if list_precomputed isn't NULL
                ast_expr_phantom_t phantom_list_value;

                if(each_in->list){
                    if(ir_gen_expression(builder, each_in->list, &list_precomputed, true, &phantom_list_value.type)){
                        return FAILURE;
                    }
                    phantom_list_value.id = EXPR_PHANTOM;
                    phantom_list_value.ir_value = list_precomputed;
                    phantom_list_value.source = each_in->list->source;
                    phantom_list_value.is_mutable = expr_is_mutable(each_in->list);
                }
                
                // Generate length
                ir_value_t *array_length;
                if(list_precomputed){
                    ast_expr_call_method_t length_call;
                    length_call.id = EXPR_CALL_METHOD;
                    length_call.source = phantom_list_value.source;
                    length_call.name = "__length__";
                    length_call.value = (ast_expr_t*) &phantom_list_value;
                    length_call.args = NULL;
                    length_call.arity = 0;
                    length_call.is_tentative = false;

                    if(ir_gen_expression(builder, (ast_expr_t*) &length_call, &array_length, false, &temporary_type)){
                        ast_type_free(&phantom_list_value.type);
                        return FAILURE;
                    }
                } else if(ir_gen_expression(builder, each_in->length, &array_length, false, &temporary_type)){
                    return FAILURE;
                }

                if(!ast_types_conform(builder, &array_length, &temporary_type, idx_ast_type, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, each_in->length->source, "Received type '%s' when array length should be 'usize'", a_type_str);
                    free(a_type_str);

                    if(list_precomputed) ast_type_free(&phantom_list_value.type);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                // Generate (idx < length)
                ir_value_t *idx_value = build_load(builder, idx_ptr);
                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_ULESSER;
                ((ir_instr_math_t*) built_instr)->result_type = ir_builder_bool(builder);
                ((ir_instr_math_t*) built_instr)->a = idx_value;
                ((ir_instr_math_t*) built_instr)->b = array_length;
                ir_value_t *whether_keep_going_value = build_value_from_prev_instruction(builder);

                // Generate conditional break
                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = whether_keep_going_value;
                ((ir_instr_cond_break_t*) built_instr)->true_block_id = new_basicblock_id;
                ((ir_instr_cond_break_t*) built_instr)->false_block_id = end_basicblock_id;

                build_using_basicblock(builder, new_basicblock_id);

                // Generate array value
                ir_value_t *array;
                if(list_precomputed){
                    ast_expr_call_method_t array_call;
                    array_call.id = EXPR_CALL_METHOD;
                    array_call.source = phantom_list_value.source;
                    array_call.name = "__array__";
                    array_call.value = (ast_expr_t*) &phantom_list_value;
                    array_call.args = NULL;
                    array_call.arity = 0;
                    array_call.is_tentative = false;

                    if(ir_gen_expression(builder, (ast_expr_t*) &array_call, &array, false, &temporary_type)){
                        ast_type_free(&phantom_list_value.type);
                        close_scope(builder);
                        return FAILURE;
                    }

                    // We don't need 'phantom_list_value' anymore, so free its type
                    ast_type_free(&phantom_list_value.type);
                } else if(ir_gen_expression(builder, each_in->low_array, &array, false, &temporary_type)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(temporary_type.elements_length == 0 || temporary_type.elements[0]->id != AST_ELEM_POINTER){
                    compiler_panic(builder->compiler, each_in->low_array->source,
                        "Low-level array type for 'each in' statement must be a pointer");
                    ast_type_free(&temporary_type);
                    close_scope(builder);
                    return FAILURE;
                }

                // Modify ast_type_t to remove a pointer element from the front
                // DANGEROUS: Manually deleting ast_elem_pointer_t
                free(temporary_type.elements[0]);
                memmove(temporary_type.elements, &temporary_type.elements[1], sizeof(ast_elem_t*) * (temporary_type.elements_length - 1));
                temporary_type.elements_length--; // Reduce length accordingly

                if(!ast_types_identical(&temporary_type, each_in->it_type)){
                    compiler_panic(builder->compiler, each_in->it_type->source,
                        "Element type doesn't match given array's element type");

                    char *s1 = ast_type_str(each_in->it_type);
                    char *s2 = ast_type_str(&temporary_type);
                    printf("(given element type : '%s', array element type : '%s')\n", s1, s2);
                    free(s1);
                    free(s2);

                    ast_type_free(&temporary_type);
                    close_scope(builder);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                length_t it_var_id = builder->next_var_id;
                char *it_name = each_in->it_name ? each_in->it_name : "it";

                // Generate new block statements to update 'it' variable
                add_variable(builder, it_name, each_in->it_type, array->type, BRIDGE_VAR_POD | BRIDGE_VAR_REFERENCE);
                
                ir_value_t *it_ptr = build_varptr(builder, array->type, it_var_id);
                ir_value_t *it_idx = build_load(builder, idx_ptr);

                built_instr = build_instruction(builder, sizeof(ir_instr_array_access_t));
                ((ir_instr_array_access_t*) built_instr)->id = INSTRUCTION_ARRAY_ACCESS;
                ((ir_instr_array_access_t*) built_instr)->result_type = array->type;
                ((ir_instr_array_access_t*) built_instr)->index = it_idx;
                ((ir_instr_array_access_t*) built_instr)->value = array;
                ir_value_t *current_it = build_value_from_prev_instruction(builder);

                build_store(builder, current_it, it_ptr);

                // Generate new_block user-defined statements
                bool terminated;
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, each_in->statements, each_in->statements_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, inc_basicblock_id);
                }

                // Generate jump inc_block
                build_using_basicblock(builder, inc_basicblock_id);

                ir_value_t *current_idx = build_load(builder, idx_ptr);
                ir_value_t *ir_one_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                ir_one_value->value_type = VALUE_TYPE_LITERAL;
                ir_type_map_find(builder->type_map, "usize", &(ir_one_value->type));
                ir_one_value->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
                *((unsigned long long*) ir_one_value->extra) = 1;

                // Increament
                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_ADD;
                ((ir_instr_math_t*) built_instr)->result_type = current_idx->type;
                ((ir_instr_math_t*) built_instr)->a = current_idx;
                ((ir_instr_math_t*) built_instr)->b = ir_one_value;
                ir_value_t *increamented = build_value_from_prev_instruction(builder);

                // Store
                build_store(builder, increamented, idx_ptr);

                // Jump Prep
                build_break(builder, prep_basicblock_id);

                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                if(each_in->label != NULL) builder->block_stack_length--;

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
                builder->break_continue_scope = prev_break_continue_scope;
            }
            break;
        case EXPR_REPEAT: {
                ast_expr_repeat_t *repeat = (ast_expr_repeat_t*) statements[s];

                length_t prep_basicblock_id = build_basicblock(builder);
                length_t new_basicblock_id  = build_basicblock(builder);
                length_t inc_basicblock_id  = build_basicblock(builder);
                length_t end_basicblock_id  = build_basicblock(builder);
                
                if(repeat->label != NULL){
                    prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = repeat->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = inc_basicblock_id;
                    builder->block_stack_scopes[builder->block_stack_length] = builder->scope;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;
                bridge_scope_t *prev_break_continue_scope = builder->break_continue_scope;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = inc_basicblock_id;
                builder->break_continue_scope = builder->scope;

                ast_type_t *idx_ast_type = ast_get_usize(&builder->object->ast);

                ir_type_t *idx_ir_type = ir_builder_usize(builder);
                ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);
                
                open_scope(builder);

                // Create 'idx' variable
                length_t idx_var_id = builder->next_var_id;
                add_variable(builder, "idx", idx_ast_type, idx_ir_type, BRIDGE_VAR_POD | BRIDGE_VAR_UNDEF);
                ir_value_t *idx_ptr = build_varptr(builder, idx_ir_type_ptr, idx_var_id);

                // Set 'idx' to inital value of zero
                ir_value_t *initial_idx = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                initial_idx->value_type = VALUE_TYPE_LITERAL;
                initial_idx->type = idx_ir_type;
                initial_idx->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
                *((unsigned long long*) initial_idx->extra) = 0;

                build_store(builder, initial_idx, idx_ptr);
                build_break(builder, prep_basicblock_id);
                build_using_basicblock(builder, prep_basicblock_id);

                // Generate length
                ir_value_t *limit;

                if(ir_gen_expression(builder, repeat->limit, &limit, false, &temporary_type)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!ast_types_conform(builder, &limit, &temporary_type, idx_ast_type, CONFORM_MODE_CALCULATION)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when array length should be 'usize'", a_type_str);
                    free(a_type_str);
                    ast_type_free(&temporary_type);
                    close_scope(builder);
                    return FAILURE;
                }

                ast_type_free(&temporary_type);

                // Generate (idx < length)
                ir_value_t *idx_value = build_load(builder, idx_ptr);
                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_ULESSER;
                ((ir_instr_math_t*) built_instr)->result_type = ir_builder_bool(builder);
                ((ir_instr_math_t*) built_instr)->a = idx_value;
                ((ir_instr_math_t*) built_instr)->b = limit;
                ir_value_t *whether_keep_going_value = build_value_from_prev_instruction(builder);

                // Generate conditional break
                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = whether_keep_going_value;
                ((ir_instr_cond_break_t*) built_instr)->true_block_id = new_basicblock_id;
                ((ir_instr_cond_break_t*) built_instr)->false_block_id = end_basicblock_id;

                build_using_basicblock(builder, new_basicblock_id);

                // Generate new_block user-defined statements
                bool terminated;
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, repeat->statements, repeat->statements_length, &terminated)){
                    close_scope(builder);
                    return FAILURE;
                }

                if(!terminated){
                    handle_deference_for_variables(builder, &builder->scope->list);
                    build_break(builder, inc_basicblock_id);
                }

                // Generate jump inc_block
                build_using_basicblock(builder, inc_basicblock_id);

                ir_value_t *current_idx = build_load(builder, idx_ptr);
                ir_value_t *ir_one_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                ir_one_value->value_type = VALUE_TYPE_LITERAL;
                ir_type_map_find(builder->type_map, "usize", &(ir_one_value->type));
                ir_one_value->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
                *((unsigned long long*) ir_one_value->extra) = 1;

                // Increament
                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_ADD;
                ((ir_instr_math_t*) built_instr)->result_type = current_idx->type;
                ((ir_instr_math_t*) built_instr)->a = current_idx;
                ((ir_instr_math_t*) built_instr)->b = ir_one_value;
                ir_value_t *increamented = build_value_from_prev_instruction(builder);

                // Store
                build_store(builder, increamented, idx_ptr);

                // Jump Prep
                build_break(builder, prep_basicblock_id);

                close_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                if(repeat->label != NULL) builder->block_stack_length--;

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
                builder->break_continue_scope = prev_break_continue_scope;
            }
            break;
        case EXPR_SWITCH: {
                ast_expr_switch_t *switch_expr = (ast_expr_switch_t*) statements[s];

                length_t default_block_id, resume_block_id;
                length_t starting_block_id = builder->current_block_id;

                ir_value_t *condition;
                ast_type_t master_ast_type;
                if(ir_gen_expression(builder, switch_expr->value, &condition, false, &master_ast_type)){
                    return FAILURE;
                }

                // STATIC ASSERT: Ensure that all IR integer type kinds are in the expected range
                #define tmp_range(v) (v >= 0x00000002 && v <= 0x00000009)
                #if(!(tmp_range(TYPE_KIND_S8) || tmp_range(TYPE_KIND_S16) || tmp_range(TYPE_KIND_S32) || tmp_range(TYPE_KIND_S64) || \
                    tmp_range(TYPE_KIND_U8) || tmp_range(TYPE_KIND_U16) || tmp_range(TYPE_KIND_U32) || tmp_range(TYPE_KIND_U64)))
                #error "EXPR_SWITCH in ir_gen_stmt.c assumes IR integer type kinds are between 0x00000002 and 0x00000009"
                #endif

                // Use the assumtion that IR integer type kinds are in the expected range to check integer-ness
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

                ir_value_t **case_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * switch_expr->cases_length);
                length_t *case_block_ids = ir_pool_alloc(builder->pool, sizeof(length_t) * switch_expr->cases_length);
                unsigned long long *uniqueness = malloc(sizeof(unsigned long long) * switch_expr->cases_length);

                // Layout basicblocks for each case
                for(length_t c = 0; c != switch_expr->cases_length; c++){
                    case_block_ids[c] = build_basicblock(builder);
                }
                
                // Create basicblocks for default case and/or resuming control flow
                if(switch_expr->default_statements_length != 0){
                    default_block_id = build_basicblock(builder);
                    resume_block_id = build_basicblock(builder);
                } else {
                    resume_block_id = build_basicblock(builder);
                    default_block_id = resume_block_id;
                }

                // Populate each case
                for(length_t c = 0; c != switch_expr->cases_length; c++){
                    ast_case_t *switch_case = &switch_expr->cases[c];

                    ast_type_t slave_ast_type;
                    if(ir_gen_expression(builder, switch_case->condition, &case_values[c], false, &slave_ast_type)){
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

                    unsigned long long uniqueness_value = ir_value_uniqueness_value(case_values[c]);
                    uniqueness[c] = uniqueness_value;

                    for(length_t u = 0; u != c; u++){
                        if(uniqueness_value == uniqueness[u]){
                            compiler_panicf(builder->compiler, switch_expr->cases[u].condition->source, "Non-unique case value");
                            compiler_panicf(builder->compiler, switch_case->condition->source, "Duplicate here");
                            ast_type_free(&master_ast_type);
                            free(uniqueness);
                            return FAILURE;
                        }
                    }
                    
                    open_scope(builder);

                    bool case_terminated;
                    build_using_basicblock(builder, case_block_ids[c]);
                    if(ir_gen_statements(builder, switch_case->statements, switch_case->statements_length, &case_terminated)){
                        ast_type_free(&master_ast_type);
                        close_scope(builder);
                        free(uniqueness);
                        return FAILURE;
                    }

                    if(!case_terminated){
                        handle_deference_for_variables(builder, &builder->scope->list);
                        build_break(builder, resume_block_id);
                    }
                    
                    close_scope(builder);
                }

                free(uniqueness);

                // Fill in statements for default block
                if(default_block_id != resume_block_id){
                    open_scope(builder);

                    bool case_terminated;
                    build_using_basicblock(builder, default_block_id);
                    if(ir_gen_statements(builder, switch_expr->default_statements, switch_expr->default_statements_length, &case_terminated)){
                        ast_type_free(&master_ast_type);
                        close_scope(builder);
                        return FAILURE;
                    }

                    if(!case_terminated){
                        handle_deference_for_variables(builder, &builder->scope->list);
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
                ((ir_instr_switch_t*) built_instr)->cases_length = switch_expr->cases_length;
                ((ir_instr_switch_t*) built_instr)->case_values = case_values;
                ((ir_instr_switch_t*) built_instr)->case_block_ids = case_block_ids;
                ((ir_instr_switch_t*) built_instr)->default_block_id = default_block_id;
                ((ir_instr_switch_t*) built_instr)->resume_block_id = resume_block_id;
                build_using_basicblock(builder, resume_block_id);
            }
            break;
        default:
            compiler_panic(builder->compiler, statements[s]->source, "INTERNAL ERROR: Unimplemented statement in ir_gen_statements()");
            return FAILURE;
        }
    }

    return SUCCESS;
}
