
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/filename.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"
#include "BRIDGE/bridge.h"

int ir_gen_func_statements(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ir_func_t *module_func){
    // ir_gens statements into basicblocks with instructions and sets in 'module_func'

    if(ast_func->statements_length == 0){
        compiler_warnf(compiler, ast_func->source, "Function '%s' is empty", ast_func->name);
    }

    ir_module_t *ir_module = &object->ir_module;
    ir_pool_t *pool = &ir_module->pool;

    // Used for constructing array of basicblocks
    ir_builder_t builder;
    builder.basicblocks = malloc(sizeof(ir_basicblock_t) * 4);
    builder.basicblocks_length = 1;
    builder.basicblocks_capacity = 4;
    builder.pool = pool;
    builder.type_map = &ir_module->type_map;
    builder.compiler = compiler;
    builder.object = object;
    builder.ast_func = ast_func;
    builder.module_func = module_func;
    builder.next_var_id = 0;

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
    builder.block_stack_length = 0;
    builder.block_stack_capacity = 0;

    module_func->var_scope = malloc(sizeof(bridge_var_scope_t));
    bridge_var_scope_init(module_func->var_scope, NULL);
    module_func->var_scope->first_var_id = 0;
    builder.var_scope = module_func->var_scope;

    while(module_func->arity != ast_func->arity){
        if(ir_gen_resolve_type(compiler, object, &ast_func->arg_types[module_func->arity], &module_func->argument_types[module_func->arity])) return 1;
        add_variable(&builder, ast_func->arg_names[module_func->arity], &ast_func->arg_types[module_func->arity], module_func->argument_types[module_func->arity], BRIDGE_VAR_UNDEF);
        module_func->arity++;
    }

    ast_expr_t **statements = ast_func->statements;
    length_t statements_length = ast_func->statements_length;

    if(ast_func->traits & AST_FUNC_MAIN){
        // Initialize all global variables
        if(ir_gen_globals_init(&builder)) return 1;
    }

    if(ir_gen_statements(&builder, statements, statements_length)){
        module_func->basicblocks = builder.basicblocks;
        module_func->basicblocks_length = builder.basicblocks_length;
        return 1;
    }

    // Append return instr for functions that return void
    if(statements_length == 0 || statements[statements_length - 1]->id != EXPR_RETURN){
        if(module_func->return_type->kind == TYPE_KIND_VOID){
            ir_instr_t *built_instr = build_instruction(&builder, sizeof(ir_instr_ret_t));
            ((ir_instr_ret_t*) built_instr)->id = INSTRUCTION_RET;
            ((ir_instr_ret_t*) built_instr)->value = NULL;
        } else {
            source_t where = (statements_length != 0) ? statements[statements_length - 1]->source : ast_func->source;
            char *return_typename = ast_type_str(&ast_func->return_type);
            compiler_panicf(compiler, where, "Must return a value of type '%s' before exiting function '%s'", return_typename, ast_func->name);
            free(return_typename);
            module_func->basicblocks = builder.basicblocks;
            module_func->basicblocks_length = builder.basicblocks_length;
            return 1;
        }
    }

    module_func->var_scope->following_var_id = builder.next_var_id;
    module_func->variable_count = builder.next_var_id;

    free(builder.block_stack_labels);
    free(builder.block_stack_break_ids);
    free(builder.block_stack_continue_ids);

    module_func->basicblocks = builder.basicblocks;
    module_func->basicblocks_length = builder.basicblocks_length;
    return 0;
}

int ir_gen_statements(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length){
    ir_instr_t *built_instr;
    ir_instr_t **instr = NULL;
    ir_value_t *expression_value = NULL;
    ast_type_t temporary_type;

    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN:
            if(((ast_expr_return_t*) statements[s])->value != NULL){
                // Return non-void value
                if(ir_gen_expression(builder, ((ast_expr_return_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &builder->ast_func->return_type, CONFORM_MODE_STANDARD)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&builder->ast_func->return_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Attempting to return type '%s' when function expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
                }

                ast_type_free(&temporary_type);
            } else {
                // Return void
                expression_value = NULL;

                if(builder->ast_func->return_type.elements_length != 1 || builder->ast_func->return_type.elements[0]->id != AST_ELEM_BASE
                        || strcmp(((ast_elem_base_t*) builder->ast_func->return_type.elements[0])->base, "void") != 0){
                    // This function expects a value to returned, not void
                    char *a_type_str = ast_type_str(&builder->ast_func->return_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Attempting to return void when function expects type '%s'", a_type_str);
                    free(a_type_str);
                    return 1;
                }
            }

            built_instr = build_instruction(builder, sizeof(ir_instr_ret_t));
            ((ir_instr_ret_t*) built_instr)->id = INSTRUCTION_RET;
            ((ir_instr_ret_t*) built_instr)->result_type = NULL;
            ((ir_instr_ret_t*) built_instr)->value = expression_value;

            if(s + 1 != statements_length){
                compiler_panicf(builder->compiler, statements[s + 1]->source, "Statements after 'return' statement in function '%s'", builder->ast_func->name);
                return 1;
            }
            return 0; // Return because no other statements can be after this one
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = ((ast_expr_call_t*) statements[s]);
                ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * call_stmt->arity);
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * call_stmt->arity);

                bool found_variable = false;
                ast_type_t *ast_var_type;
                ir_type_t *ir_var_type;
                ir_type_t *ir_return_type;
                ast_t *ast = &builder->object->ast;

                // Resolve & ir_gen function arguments
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(ir_gen_expression(builder, call_stmt->args[a], &arg_values[a], false, &arg_types[a])){
                        for(length_t t = 0; t != a; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }
                }

                bridge_var_t *var = bridge_var_scope_find_var(builder->var_scope, call_stmt->name);

                if(var){
                    ast_var_type = var->ast_type;
                    ir_var_type = ir_type_pointer_to(builder->pool, var->ir_type);

                    if(ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                        char *s = ast_type_str(ast_var_type);
                        compiler_panicf(builder->compiler, call_stmt->source, "Can't call value of non function type '%s'", s);
                        free(s);
                        return 1;
                    }

                    ast_type_t *ast_function_return_type = ((ast_elem_func_t*) ast_var_type->elements[0])->return_type;

                    if(ir_gen_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                        for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }

                    if(ast_var_type->elements_length == 1 && ast_var_type->elements[0]->id == AST_ELEM_FUNC){
                        expression_value = build_varptr(builder, ir_var_type, var->id);
                        expression_value = build_load(builder, expression_value);
                    }

                    found_variable = true;
                }

                if(!found_variable) for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(ast->globals[g].name, call_stmt->name) == 0){
                        ast_var_type = &ast->globals[g].type;

                        // We could search the ir var map, but this is easier
                        // TODO: Create easy way to get between ast and ir var maps
                        ir_var_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                        ir_var_type->kind = TYPE_KIND_POINTER;

                        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_var_type, (ir_type_t**) &ir_var_type->extra)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                            char *s = ast_type_str(ast_var_type);
                            compiler_panicf(builder->compiler, call_stmt->source, "Can't call value of non function type '%s'", s);
                            free(s);
                            return 1;
                        }

                        ast_type_t *ast_function_return_type = ((ast_elem_func_t*) ast_var_type->elements[0])->return_type;

                        if(ir_gen_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(ast_var_type->elements_length == 1 && ast_var_type->elements[0]->id == AST_ELEM_FUNC){
                            expression_value = build_gvarptr(builder, ir_var_type, g);
                            expression_value = build_load(builder, expression_value);
                            found_variable = true;
                        }
                        break;
                    }
                }

                if(found_variable){
                    // This is ok since we previously checked that (ast_var_type->elements[0]->id == AST_ELEM_FUNC)
                    ast_elem_func_t *function_elem = (ast_elem_func_t*) ast_var_type->elements[0];

                    if(function_elem->traits & AST_FUNC_VARARG){
                        if(function_elem->arity > call_stmt->arity){
                            compiler_panicf(builder->compiler, call_stmt->source, "Incorrect argument count (at least %d expected, %d given)", (int) function_elem->arity, (int) call_stmt->arity);
                            return 1;
                        }
                    } else if(function_elem->arity != call_stmt->arity){
                        compiler_panicf(builder->compiler, call_stmt->source, "Incorrect argument count (%d expected, %d given)", (int) function_elem->arity, (int) call_stmt->arity);
                        return 1;
                    }

                    for(length_t a = 0; a != function_elem->arity; a++){
                        if(!ast_types_conform(builder, &arg_values[a], &arg_types[a], &function_elem->arg_types[a], CONFORM_MODE_PRIMITIVES)){
                            char *s1 = ast_type_str(&function_elem->arg_types[a]);
                            char *s2 = ast_type_str(&arg_types[a]);
                            compiler_panicf(builder->compiler, call_stmt->args[a]->source, "Required argument type '%s' is incompatible with type '%s'", s1, s2);
                            free(s1);
                            free(s2);
                            return 1;
                        }
                    }

                    ir_basicblock_new_instructions(builder->current_block, 1);
                    built_instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_address_t));
                    ((ir_instr_call_address_t*) built_instr)->id = INSTRUCTION_CALL_ADDRESS;
                    ((ir_instr_call_address_t*) built_instr)->result_type = ir_return_type;
                    ((ir_instr_call_address_t*) built_instr)->address = expression_value;
                    ((ir_instr_call_address_t*) built_instr)->values = arg_values;
                    ((ir_instr_call_address_t*) built_instr)->values_length = call_stmt->arity;
                    builder->current_block->instructions[builder->current_block->instructions_length++] = built_instr;

                    for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                } else {
                    funcpair_t pair;
                    if(ir_gen_find_func_conforming(builder, call_stmt->name, arg_values, arg_types, call_stmt->arity, &pair)){
                        compiler_undeclared_function(builder->compiler, &builder->object->ir_module, statements[s]->source, call_stmt->name, arg_types, call_stmt->arity);
                        for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }

                    built_instr = build_instruction(builder, sizeof(ir_instr_call_t));
                    ((ir_instr_call_t*) built_instr)->id = INSTRUCTION_CALL;
                    ((ir_instr_call_t*) built_instr)->result_type = pair.ir_func->return_type;
                    ((ir_instr_call_t*) built_instr)->values = arg_values;
                    ((ir_instr_call_t*) built_instr)->values_length = call_stmt->arity;
                    ((ir_instr_call_t*) built_instr)->func_id = pair.func_id;

                    for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                }
            }
            break;
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                ast_expr_declare_t *declare_stmt = ((ast_expr_declare_t*) statements[s]);

                // Search for existing variable named that
                if(bridge_var_scope_already_in_list(builder->var_scope, declare_stmt->name)){
                    compiler_panicf(builder->compiler, statements[s]->source, "Variable '%s' already declared", declare_stmt->name);
                    return 1;
                }

                ir_type_t *ir_decl_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                if(ir_gen_resolve_type(builder->compiler, builder->object, &declare_stmt->type, &ir_decl_type)) return 1;

                if(declare_stmt->value != NULL){
                    // Regular declare statement initial assign value
                    ir_value_t *initial;
                    ir_value_t *destination = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                    ir_type_t *var_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                    var_pointer_type->kind = TYPE_KIND_POINTER;
                    var_pointer_type->extra = ir_decl_type;

                    if(ir_gen_expression(builder, declare_stmt->value, &initial, false, &temporary_type)) return 1;

                    if(!ast_types_conform(builder, &initial, &temporary_type, &declare_stmt->type, CONFORM_MODE_PRIMITIVES)){
                        char *a_type_str = ast_type_str(&temporary_type);
                        char *b_type_str = ast_type_str(&declare_stmt->type);
                        compiler_panicf(builder->compiler, declare_stmt->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
                        free(a_type_str);
                        free(b_type_str);
                        ast_type_free(&temporary_type);
                        return 1;
                    }

                    ast_type_free(&temporary_type);

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

                    build_store(builder, initial, destination);

                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, TRAIT_NONE);
                } else if(statements[s]->id == EXPR_DECLAREUNDEF){
                    // Mark the variable as undefined memory so it isn't auto-initialized later on
                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, BRIDGE_VAR_UNDEF);
                } else {
                    // Variable declaration without default value
                    add_variable(builder, declare_stmt->name, &declare_stmt->type, ir_decl_type, TRAIT_NONE);

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
                if(ir_gen_expression(builder, assign_stmt->value, &expression_value, false, &expression_value_type)) return 1;
                if(ir_gen_expression(builder, assign_stmt->destination, &destination, true, &destination_type)){
                    ast_type_free(&expression_value_type);
                    return 1;
                }

                if(!ast_types_conform(builder, &expression_value, &expression_value_type, &destination_type, CONFORM_MODE_PRIMITIVES)){
                    char *a_type_str = ast_type_str(&expression_value_type);
                    char *b_type_str = ast_type_str(&destination_type);
                    compiler_panicf(builder->compiler, assign_stmt->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&destination_type);
                    ast_type_free(&expression_value_type);
                    return 1;
                }

                ast_type_free(&destination_type);
                ast_type_free(&expression_value_type);

                ir_value_t *instr_value;

                if(assignment_type == EXPR_ASSIGN){
                    build_store(builder, expression_value, destination);
                } else {
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
                            return 1;
                        }
                        break;
                    case EXPR_SUBTRACTASSIGN:
                        if(i_vs_f_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't subtract those types");
                            return 1;
                        }
                        break;
                    case EXPR_MULTIPLYASSIGN:
                        if(i_vs_f_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_MULTIPLY, INSTRUCTION_FMULTIPLY)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't multiply those types");
                            return 1;
                        }
                        break;
                    case EXPR_DIVIDEASSIGN:
                        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_UDIVIDE, INSTRUCTION_SDIVIDE, INSTRUCTION_FDIVIDE)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't divide those types");
                            return 1;
                        }
                        break;
                    case EXPR_MODULUSASSIGN:
                        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) built_instr, INSTRUCTION_UMODULUS, INSTRUCTION_SMODULUS, INSTRUCTION_FMODULUS)){
                            compiler_panic(builder->compiler, assign_stmt->source, "Can't take the modulus of those types");
                            return 1;
                        }
                        break;
                    default:
                        compiler_panic(builder->compiler, assign_stmt->source, "INTERNAL ERROR: ir_gen_statements() got unknown assignment operator id");
                        return 1;
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
                if(ir_gen_expression(builder, ((ast_expr_if_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

                // Create static bool type for comparison with
                ast_elem_base_t bool_base;
                bool_base.id = AST_ELEM_BASE;
                bool_base.source.index = 0;
                bool_base.source.object_index = builder->object->index;
                bool_base.base = "bool";
                ast_elem_t *bool_type_elem = (ast_elem_t*) &bool_base;
                ast_type_t bool_type;
                bool_type.elements = &bool_type_elem;
                bool_type.elements_length = 1;
                bool_type.source.index = 0;
                bool_type.source.object_index = builder->object->index;

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &bool_type, CONFORM_MODE_PRIMITIVES)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&bool_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
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
                bool terminated = if_stmts_length == 0 ? false : EXPR_IS_TERMINATION(if_stmts[if_stmts_length - 1]->id);

                open_var_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, if_stmts, if_stmts_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, end_basicblock_id);

                close_var_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                unsigned int conditional_type = statements[s]->id;
                if(ir_gen_expression(builder, ((ast_expr_ifelse_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

                // Create static bool type for comparison with
                ast_elem_base_t bool_base;
                bool_base.id = AST_ELEM_BASE;
                bool_base.source.index = 0;
                bool_base.source.object_index = builder->object->index;
                bool_base.base = "bool";
                ast_elem_t *bool_type_elem = (ast_elem_t*) &bool_base;
                ast_type_t bool_type;
                bool_type.elements = &bool_type_elem;
                bool_type.elements_length = 1;
                bool_type.source.index = 0;
                bool_type.source.object_index = builder->object->index;

                if(!ast_types_identical(&temporary_type, &bool_type)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&bool_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
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
                bool terminated = if_stmts_length == 0 ? false : EXPR_IS_TERMINATION(if_stmts[if_stmts_length - 1]->id);

                open_var_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);

                if(ir_gen_statements(builder, if_stmts, if_stmts_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, end_basicblock_id);

                ast_expr_t **else_stmts = ((ast_expr_ifelse_t*) statements[s])->else_statements;
                length_t else_stmts_length = ((ast_expr_ifelse_t*) statements[s])->else_statements_length;
                terminated = else_stmts_length == 0 ? false : EXPR_IS_TERMINATION(else_stmts[else_stmts_length - 1]->id);

                close_var_scope(builder);
                open_var_scope(builder);
                build_using_basicblock(builder, else_basicblock_id);
                if(ir_gen_statements(builder, else_stmts, else_stmts_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, end_basicblock_id);

                build_using_basicblock(builder, end_basicblock_id);
                close_var_scope(builder);
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
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = test_basicblock_id;

                build_break(builder, test_basicblock_id);
                build_using_basicblock(builder, test_basicblock_id);

                unsigned int conditional_type = statements[s]->id;
                if(ir_gen_expression(builder, ((ast_expr_while_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

                // Create static bool type for comparison with
                ast_elem_base_t bool_base;
                bool_base.id = AST_ELEM_BASE;
                bool_base.source.index = 0;
                bool_base.source.object_index = builder->object->index;
                bool_base.base = "bool";
                ast_elem_t *bool_type_elem = (ast_elem_t*) &bool_base;
                ast_type_t bool_type;
                bool_type.elements = &bool_type_elem;
                bool_type.elements_length = 1;
                bool_type.source.index = 0;
                bool_type.source.object_index = builder->object->index;

                if(!ast_types_conform(builder, &expression_value, &temporary_type, &bool_type, CONFORM_MODE_PRIMITIVES)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&bool_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when conditional expects type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
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
                bool terminated = while_stmts_length == 0 ? false : EXPR_IS_TERMINATION(while_stmts[while_stmts_length - 1]->id);

                open_var_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, while_stmts, while_stmts_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, test_basicblock_id);

                if(((ast_expr_while_t*) statements[s])->label != NULL) builder->block_stack_length--;
                close_var_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
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
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = new_basicblock_id;

                build_break(builder, new_basicblock_id);
                build_using_basicblock(builder, new_basicblock_id);

                ast_expr_t **loop_stmts = ((ast_expr_whilecontinue_t*) statements[s])->statements;
                length_t loop_stmts_length = ((ast_expr_whilecontinue_t*) statements[s])->statements_length;
                bool terminated = loop_stmts_length == 0 ? false : EXPR_IS_TERMINATION(loop_stmts[loop_stmts_length - 1]->id);

                open_var_scope(builder);
                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, loop_stmts, loop_stmts_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated){
                    if(statements[s]->id == EXPR_WHILECONTINUE){
                        // 'while continue'
                        build_break(builder, end_basicblock_id);
                    } else {
                        // 'until break'
                        build_break(builder, new_basicblock_id);
                    }
                }

                if(((ast_expr_whilecontinue_t*) statements[s])->label != NULL) builder->block_stack_length--;
                close_var_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (call_stmt->arity + 1));
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * (call_stmt->arity + 1));

                // Resolve & ir_gen function arguments
                if(ir_gen_expression(builder, call_stmt->value, &arg_values[0], true, &arg_types[0])){
                    free(arg_types);
                    return 1;
                }

                ast_elem_t **type_elems = arg_types[0].elements;
                length_t type_elems_length = arg_types[0].elements_length;

                if(type_elems_length == 1 && type_elems[0]->id == AST_ELEM_BASE){
                    ast_type_prepend_ptr(&arg_types[0]);
                } else if(type_elems_length == 2 && type_elems[0]->id == AST_ELEM_POINTER
                        && type_elems[1]->id == AST_ELEM_BASE){
                    // Load the value that's being called on
                    arg_values[0] = build_load(builder, arg_values[0]);
                } else {
                    char *s = ast_type_str(&arg_types[0]);
                    compiler_panicf(builder->compiler, call_stmt->source, "Can't call methods on type '%s'", s);
                    ast_type_free(&arg_types[0]);
                    free(arg_types);
                    free(s);
                    return 1;
                }

                const char *struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;

                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(ir_gen_expression(builder, call_stmt->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
                        for(length_t t = 0; t != a + 1; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }
                }

                // Find function that fits given name and arguments
                funcpair_t pair;
                if(ir_gen_find_method_conforming(builder, struct_name, call_stmt->name,
                        arg_values, arg_types, call_stmt->arity + 1, &pair)){
                    compiler_panicf(builder->compiler, call_stmt->source, "Undeclared method '%s'", call_stmt->name);
                    for(length_t t = 0; t != call_stmt->arity + 1; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                    return 1;
                }

                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_call_t* instruction = (ir_instr_call_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                instruction->id = INSTRUCTION_CALL;
                instruction->result_type = pair.ir_func->return_type;
                instruction->values = arg_values;
                instruction->values_length = call_stmt->arity + 1;
                instruction->func_id = pair.func_id;
                builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;

                for(length_t t = 0; t != call_stmt->arity + 1; t++) ast_type_free(&arg_types[t]);
                free(arg_types);
            }
            break;
        case EXPR_DELETE: {
                ast_expr_delete_t *delete_expr = (ast_expr_delete_t*) statements[s];
                if(ir_gen_expression(builder, delete_expr->value, &expression_value, false, &temporary_type)) return 1;

                if(temporary_type.elements_length == 0 || ( temporary_type.elements[0]->id != AST_ELEM_POINTER &&
                    !(temporary_type.elements[0]->id == AST_ELEM_BASE && strcmp(((ast_elem_base_t*) temporary_type.elements[0])->base, "ptr") == 0)
                )){
                    char *t = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, delete_expr->source, "Can't pass non-pointer type '%s' to delete", t);
                    ast_type_free(&temporary_type);
                    free(t);
                    return 1;
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
                    return 1;
                }

                build_break(builder, builder->break_block_id);
            }
            break;
        case EXPR_CONTINUE: {
                if(builder->continue_block_id == 0){
                    compiler_panicf(builder->compiler, statements[s]->source, "Nowhere to continue to");
                    return 1;
                }

                build_break(builder, builder->continue_block_id);
            }
            break;
        case EXPR_BREAK_TO: {
                char *target_label = ((ast_expr_break_to_t*) statements[s])->label;
                length_t target_block_id = 0;

                for(length_t i = builder->block_stack_length - 1; i != -1 /* overflow occurs */; i--){
                    if(strcmp(target_label, builder->block_stack_labels[i]) == 0){
                        target_block_id = builder->block_stack_break_ids[i];
                        break;
                    }
                }

                if(target_block_id == 0){
                    compiler_panicf(builder->compiler, ((ast_expr_break_to_t*) statements[s])->label_source, "Undeclared label '%s'", target_label);
                    return 1;
                }

                build_break(builder, target_block_id);
            }
            break;
        case EXPR_CONTINUE_TO: {
                char *target_label = ((ast_expr_continue_to_t*) statements[s])->label;
                length_t target_block_id = 0;

                for(length_t i = builder->block_stack_length - 1; i != -1 /* overflow occurs */; i--){
                    if(strcmp(target_label, builder->block_stack_labels[i]) == 0){
                        target_block_id = builder->block_stack_continue_ids[i];
                        break;
                    }
                }

                if(target_block_id == 0){
                    compiler_panicf(builder->compiler, ((ast_expr_continue_to_t*) statements[s])->label_source, "Undeclared label '%s'", target_label);
                    return 1;
                }

                build_break(builder, target_block_id);
            }
            break;
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
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = inc_basicblock_id;

                ast_type_t *idx_ast_type = ast_get_usize(&builder->object->ast);

                ir_type_t *idx_ir_type = ir_builder_usize(builder);
                ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);
                ir_type_t *ir_bool_type = ir_builder_usize(builder);

                open_var_scope(builder);

                // Create 'idx' variable
                length_t idx_var_id = builder->next_var_id;
                add_variable(builder, "idx", idx_ast_type, idx_ir_type, TRAIT_NONE);
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
                ir_value_t *array_length;

                if(ir_gen_expression(builder, each_in->length, &array_length, false, &temporary_type)){
                    return 1;
                }

                if(!ast_types_conform(builder, &array_length, &temporary_type, idx_ast_type, CONFORM_MODE_STANDARD)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, each_in->length->source, "Received type '%s' when array length should be 'usize'", a_type_str);
                    free(a_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
                }

                ast_type_free(&temporary_type);

                // Generate (idx == length)
                ir_value_t *idx_value = build_load(builder, idx_ptr);

                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_EQUALS;
                ((ir_instr_math_t*) built_instr)->a = idx_value;
                ((ir_instr_math_t*) built_instr)->b = array_length;
                ((ir_instr_math_t*) built_instr)->result_type = ir_bool_type;
                ir_value_t *is_done_value = build_value_from_prev_instruction(builder);

                // Generate conditional break
                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = is_done_value;
                ((ir_instr_cond_break_t*) built_instr)->true_block_id = end_basicblock_id;
                ((ir_instr_cond_break_t*) built_instr)->false_block_id = new_basicblock_id;

                build_using_basicblock(builder, new_basicblock_id);

                // Generate new_block statements to update 'it' variable
                ir_value_t *array;

                if(ir_gen_expression(builder, each_in->low_array, &array, false, &temporary_type)){
                    ast_type_free(&temporary_type);
                    close_var_scope(builder);
                    return 1;
                }

                if(temporary_type.elements_length == 0 || temporary_type.elements[0]->id != AST_ELEM_POINTER){
                    compiler_panic(builder->compiler, each_in->low_array->source,
                        "Low-level array type for 'each in' statement must be a pointer");
                    ast_type_free(&temporary_type);
                    close_var_scope(builder);
                    return 1;
                }

                // Modify ast_type_t to remove a pointer element from the front
                // DANGEROUS: Manually deleting ast_elem_pointer_t
                free(temporary_type.elements[0]);
                memmove(temporary_type.elements, &temporary_type.elements[1], sizeof(ast_elem_t*) * (temporary_type.elements_length - 1));
                temporary_type.elements_length--; // Reduce length accordingly

                if(!ast_types_identical(&temporary_type, each_in->it_type)){
                    compiler_panic(builder->compiler, each_in->it_type->source,
                        "Element type doesn't match given array's element type");
                    ast_type_free(&temporary_type);
                    close_var_scope(builder);

                    char *s1 = ast_type_str(&temporary_type);
                    char *s2 = ast_type_str(each_in->it_type);
                    printf("(given element type : '%s', array element type : '%s')\n", s1, s2);
                    free(s1);
                    free(s2);

                    return 1;
                }

                ast_type_free(&temporary_type);

                length_t it_var_id = builder->next_var_id;
                char *it_name = each_in->it_name ? each_in->it_name : "it";

                add_variable(builder, it_name, each_in->it_type, array->type, BRIDGE_VAR_REFERENCE);
                
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
                bool terminated = each_in->statements_length == 0
                    ? false
                    : EXPR_IS_TERMINATION(each_in->statements[each_in->statements_length - 1]->id);

                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, each_in->statements, each_in->statements_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, inc_basicblock_id);

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

                close_var_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                if(each_in->label != NULL) builder->block_stack_length--;

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
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
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = inc_basicblock_id;

                ast_type_t *idx_ast_type = ast_get_usize(&builder->object->ast);

                ir_type_t *idx_ir_type = ir_builder_usize(builder);
                ir_type_t *idx_ir_type_ptr = ir_builder_usize_ptr(builder);
                ir_type_t *ir_bool_type = ir_builder_bool(builder);
                
                open_var_scope(builder);

                // Create 'idx' variable
                length_t idx_var_id = builder->next_var_id;
                add_variable(builder, "idx", idx_ast_type, idx_ir_type, TRAIT_NONE);
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
                    return 1;
                }

                if(!ast_types_conform(builder, &limit, &temporary_type, idx_ast_type, CONFORM_MODE_PRIMITIVES)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    compiler_panicf(builder->compiler, statements[s]->source, "Received type '%s' when array length should be 'usize'", a_type_str);
                    free(a_type_str);
                    ast_type_free(&temporary_type);
                    return 1;
                }

                ast_type_free(&temporary_type);

                // Generate (idx == length)
                ir_value_t *idx_value = build_load(builder, idx_ptr);

                built_instr = build_instruction(builder, sizeof(ir_instr_math_t));
                ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_EQUALS;
                ((ir_instr_math_t*) built_instr)->a = idx_value;
                ((ir_instr_math_t*) built_instr)->b = limit;
                ((ir_instr_math_t*) built_instr)->result_type = ir_bool_type;
                ir_value_t *is_done_value = build_value_from_prev_instruction(builder);

                // Generate conditional break
                built_instr = build_instruction(builder, sizeof(ir_instr_cond_break_t));
                ((ir_instr_cond_break_t*) built_instr)->id = INSTRUCTION_CONDBREAK;
                ((ir_instr_cond_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_cond_break_t*) built_instr)->value = is_done_value;
                ((ir_instr_cond_break_t*) built_instr)->true_block_id = end_basicblock_id;
                ((ir_instr_cond_break_t*) built_instr)->false_block_id = new_basicblock_id;

                build_using_basicblock(builder, new_basicblock_id);

                // Generate new_block user-defined statements
                bool terminated = repeat->statements_length == 0
                    ? false
                    : EXPR_IS_TERMINATION(repeat->statements[repeat->statements_length - 1]->id);

                build_using_basicblock(builder, new_basicblock_id);
                if(ir_gen_statements(builder, repeat->statements, repeat->statements_length)){
                    close_var_scope(builder);
                    return 1;
                }

                if(!terminated) build_break(builder, inc_basicblock_id);

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

                close_var_scope(builder);
                build_using_basicblock(builder, end_basicblock_id);

                if(repeat->label != NULL) builder->block_stack_length--;

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
            }
            break;
        default:
            compiler_panic(builder->compiler, statements[s]->source, "INTERNAL ERROR: Unimplemented statement in ir_gen_statements()");
            return 1;
        }
    }

    return 0;
}
