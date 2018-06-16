
#include "ground.h"
#include "assemble.h"
#include "filename.h"
#include "assemble_expr.h"
#include "assemble_find.h"
#include "assemble_stmt.h"
#include "assemble_type.h"

int assemble_func_statements(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ir_func_t *module_func){
    // Assembles statements into basicblocks with instructions and sets in 'module_func'

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

    ast_expr_t **statements = ast_func->statements;
    length_t statements_length = ast_func->statements_length;

    if(ast_func->traits & AST_FUNC_MAIN){
        // Initialize all global variables
        if(assemble_globals_init(&builder)) return 1;
    }

    if(assemble_statements(&builder, statements, statements_length)){
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

    free(builder.block_stack_labels);
    free(builder.block_stack_break_ids);
    free(builder.block_stack_continue_ids);

    module_func->basicblocks = builder.basicblocks;
    module_func->basicblocks_length = builder.basicblocks_length;
    return 0;
}

int assemble_statements(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length){
    ir_instr_t *built_instr;
    ir_instr_t **instr = NULL;
    ir_value_t *expression_value = NULL;
    ast_type_t temporary_type;

    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN:
            if(((ast_expr_return_t*) statements[s])->value != NULL){
                // Return non-void value
                if(assemble_expression(builder, ((ast_expr_return_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

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
                ast_type_t *variable_type;
                ir_type_t *ir_var_type;
                ir_type_t *ir_return_type;
                ast_t *ast = &builder->object->ast;

                // Resolve & Assemble function arguments
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(assemble_expression(builder, call_stmt->args[a], &arg_values[a], false, &arg_types[a])){
                        for(length_t t = 0; t != a; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }
                }

                for(length_t a = 0; a != builder->ast_func->var_list.length; a++){
                    if(strcmp(builder->ast_func->var_list.names[a], call_stmt->name) == 0){
                        variable_type = builder->ast_func->var_list.types[a];

                        // We could search the ir var map, but this is easier
                        // TODO: Create easy way to get between ast and ir var maps
                        ir_var_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                        ir_var_type->kind = TYPE_KIND_POINTER;

                        if(assemble_resolve_type(builder->compiler, builder->object, variable_type, (ir_type_t**) &ir_var_type->extra)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(variable_type->elements[0]->id != AST_ELEM_FUNC){
                            char *s = ast_type_str(variable_type);
                            compiler_panicf(builder->compiler, call_stmt->source, "Can't call value of non function type '%s'", s);
                            free(s);
                            return 1;
                        }

                        ast_type_t *ast_function_return_type = ((ast_elem_func_t*) variable_type->elements[0])->return_type;

                        if(assemble_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(variable_type->elements_length == 1 && variable_type->elements[0]->id == AST_ELEM_FUNC){
                            ir_basicblock_new_instructions(builder->current_block, 2);
                            built_instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
                            ((ir_instr_varptr_t*) built_instr)->id = INSTRUCTION_VARPTR;
                            ((ir_instr_varptr_t*) built_instr)->result_type = ir_var_type;
                            ((ir_instr_varptr_t*) built_instr)->index = a;
                            builder->current_block->instructions[builder->current_block->instructions_length++] = built_instr;
                            expression_value = build_value_from_prev_instruction(builder);

                            built_instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_load_t));
                            ((ir_instr_load_t*) built_instr)->id = INSTRUCTION_LOAD;
                            ((ir_instr_load_t*) built_instr)->result_type = (ir_type_t*) ir_var_type->extra;
                            ((ir_instr_load_t*) built_instr)->value = expression_value;
                            builder->current_block->instructions[builder->current_block->instructions_length++] = built_instr;
                            expression_value = build_value_from_prev_instruction(builder);

                            found_variable = true;
                        }
                        break;
                    }
                }

                if(!found_variable) for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(ast->globals[g].name, call_stmt->name) == 0){
                        variable_type = &ast->globals[g].type;

                        // We could search the ir var map, but this is easier
                        // TODO: Create easy way to get between ast and ir var maps
                        ir_var_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                        ir_var_type->kind = TYPE_KIND_POINTER;

                        if(assemble_resolve_type(builder->compiler, builder->object, variable_type, (ir_type_t**) &ir_var_type->extra)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(variable_type->elements[0]->id != AST_ELEM_FUNC){
                            char *s = ast_type_str(variable_type);
                            compiler_panicf(builder->compiler, call_stmt->source, "Can't call value of non function type '%s'", s);
                            free(s);
                            return 1;
                        }

                        ast_type_t *ast_function_return_type = ((ast_elem_func_t*) variable_type->elements[0])->return_type;

                        if(assemble_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                            for(length_t t = 0; t != call_stmt->arity; t++) ast_type_free(&arg_types[t]);
                            free(arg_types);
                            return 1;
                        }

                        if(variable_type->elements_length == 1 && variable_type->elements[0]->id == AST_ELEM_FUNC){
                            ir_basicblock_new_instructions(builder->current_block, 2);
                            built_instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
                            ((ir_instr_varptr_t*) built_instr)->id = INSTRUCTION_GLOBALVARPTR;
                            ((ir_instr_varptr_t*) built_instr)->result_type = ir_var_type;
                            ((ir_instr_varptr_t*) built_instr)->index = g;
                            builder->current_block->instructions[builder->current_block->instructions_length++] = built_instr;
                            expression_value = build_value_from_prev_instruction(builder);

                            built_instr = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_load_t));
                            ((ir_instr_load_t*) built_instr)->id = INSTRUCTION_LOAD;
                            ((ir_instr_load_t*) built_instr)->result_type = (ir_type_t*) ir_var_type->extra;
                            ((ir_instr_load_t*) built_instr)->value = expression_value;
                            builder->current_block->instructions[builder->current_block->instructions_length++] = built_instr;
                            expression_value = build_value_from_prev_instruction(builder);

                            found_variable = true;
                        }
                        break;
                    }
                }

                if(found_variable){
                    // This is ok since we previously checked that (variable_type->elements[0]->id == AST_ELEM_FUNC)
                    ast_elem_func_t *function_elem = (ast_elem_func_t*) variable_type->elements[0];

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
                    if(assemble_find_func_conforming(builder, call_stmt->name, arg_values, arg_types, call_stmt->arity, &pair)){
                        compiler_panicf(builder->compiler, statements[s]->source, "Undeclared function '%s'", call_stmt->name);
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
                for(length_t v = 0; v != builder->module_func->var_map.mappings_length; v++){
                    if(strcmp(builder->module_func->var_map.mappings[v].name, declare_stmt->name) == 0){
                        compiler_panicf(builder->compiler, statements[s]->source, "Variable '%s' already declared", declare_stmt->name);
                        return 1;
                    }
                }

                // Create variable
                if(builder->module_func->var_map.mappings_length == builder->module_func->var_map.mappings_capacity){
                    builder->module_func->var_map.mappings_capacity *= 2;
                    ir_var_mapping_t *new_mappings = malloc(sizeof(ir_var_mapping_t) * builder->module_func->var_map.mappings_capacity);
                    memcpy(new_mappings, builder->module_func->var_map.mappings, sizeof(ir_var_mapping_t) * builder->module_func->var_map.mappings_length);
                    free(builder->module_func->var_map.mappings);
                    builder->module_func->var_map.mappings = new_mappings;
                }

                ir_var_mapping_t *mapping = &builder->module_func->var_map.mappings[builder->module_func->var_map.mappings_length++];
                mapping->name = declare_stmt->name;
                mapping->var.traits = TRAIT_NONE;

                if(assemble_resolve_type(builder->compiler, builder->object, &declare_stmt->type, &mapping->var.type)) return 1;

                if(declare_stmt->value != NULL){
                    // Regular declare statement initial assign value
                    ir_value_t *initial;
                    ir_value_t *destination = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                    ir_type_t *var_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                    var_pointer_type->kind = TYPE_KIND_POINTER;
                    var_pointer_type->extra = mapping->var.type;

                    if(assemble_expression(builder, declare_stmt->value, &initial, false, &temporary_type)) return 1;

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
                    ((ir_instr_varptr_t*) *instr)->index = builder->module_func->var_map.mappings_length - 1;
                    ((ir_instr_varptr_t*) *instr)->result_type = var_pointer_type;

                    destination->value_type = VALUE_TYPE_RESULT;
                    destination->type = var_pointer_type;
                    destination->extra = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
                    ((ir_value_result_t*) (destination)->extra)->block_id = builder->current_block_id;
                    ((ir_value_result_t*) (destination)->extra)->instruction_id = builder->current_block->instructions_length - 1;

                    instr = &builder->current_block->instructions[builder->current_block->instructions_length++];
                    *instr = ir_pool_alloc(builder->pool, sizeof(ir_instr_store_t));
                    ((ir_instr_store_t*) *instr)->id = INSTRUCTION_STORE;
                    ((ir_instr_store_t*) *instr)->result_type = NULL;
                    ((ir_instr_store_t*) *instr)->value = initial;
                    ((ir_instr_store_t*) *instr)->destination = destination;
                } else if(statements[s]->id == EXPR_DECLAREUNDEF){
                    // Mark the variable as undefined memory so it isn't auto-initialized later on
                    mapping->var.traits |= IR_VAR_UNDEF;
                }
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
        case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN: {
                unsigned int assignment_type = statements[s]->id;
                ast_expr_assign_t *assign_stmt = ((ast_expr_assign_t*) statements[s]);
                ir_value_t *destination;
                ast_type_t destination_type, expression_value_type;
                if(assemble_expression(builder, assign_stmt->value, &expression_value, false, &expression_value_type)) return 1;
                if(assemble_expression(builder, assign_stmt->destination, &destination, true, &destination_type)){
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
                ir_value_result_t *value_result;

                if(assignment_type == EXPR_ASSIGN){
                    built_instr = build_instruction(builder, sizeof(ir_instr_store_t));
                    ((ir_instr_store_t*) built_instr)->id = INSTRUCTION_STORE;
                    ((ir_instr_store_t*) built_instr)->result_type = NULL;
                    ((ir_instr_store_t*) built_instr)->value = expression_value;
                    ((ir_instr_store_t*) built_instr)->destination = destination;
                } else {
                    value_result = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
                    value_result->block_id = builder->current_block_id;
                    value_result->instruction_id = builder->current_block->instructions_length;

                    built_instr = build_instruction(builder, sizeof(ir_instr_load_t));
                    ((ir_instr_load_t*) built_instr)->id = INSTRUCTION_LOAD;
                    ((ir_instr_load_t*) built_instr)->result_type = expression_value->type;
                    ((ir_instr_load_t*) built_instr)->value = destination;

                    instr_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
                    instr_value->value_type = VALUE_TYPE_RESULT;
                    instr_value->type = expression_value->type;
                    instr_value->extra = value_result;

                    value_result = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t)); // Reused
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
                        compiler_panic(builder->compiler, assign_stmt->source, "INTERNAL ERROR: assemble_statements() got unknown assignment operator id");
                        return 1;
                    }

                    instr_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t)); // Reused
                    instr_value->value_type = VALUE_TYPE_RESULT;
                    instr_value->type = expression_value->type;
                    instr_value->extra = value_result;

                    built_instr = build_instruction(builder, sizeof(ir_instr_store_t));
                    ((ir_instr_store_t*) built_instr)->id = INSTRUCTION_STORE;
                    ((ir_instr_store_t*) built_instr)->result_type = NULL;
                    ((ir_instr_store_t*) built_instr)->value = instr_value; // Reused
                    ((ir_instr_store_t*) built_instr)->destination = destination;
                }
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: {
                unsigned int conditional_type = statements[s]->id;
                if(assemble_expression(builder, ((ast_expr_if_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

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

                build_using_basicblock(builder, new_basicblock_id);
                if(assemble_statements(builder, if_stmts, if_stmts_length)) return 1;

                if(!terminated){
                    built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                    ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                    ((ir_instr_break_t*) built_instr)->result_type = NULL;
                    ((ir_instr_break_t*) built_instr)->block_id = end_basicblock_id;
                }

                build_using_basicblock(builder, end_basicblock_id);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                unsigned int conditional_type = statements[s]->id;
                if(assemble_expression(builder, ((ast_expr_ifelse_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

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

                build_using_basicblock(builder, new_basicblock_id);
                if(assemble_statements(builder, if_stmts, if_stmts_length)) return 1;

                if(!terminated){
                    built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                    ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                    ((ir_instr_break_t*) built_instr)->result_type = NULL;
                    ((ir_instr_break_t*) built_instr)->block_id = end_basicblock_id;
                }

                ast_expr_t **else_stmts = ((ast_expr_ifelse_t*) statements[s])->else_statements;
                length_t else_stmts_length = ((ast_expr_ifelse_t*) statements[s])->else_statements_length;
                terminated = else_stmts_length == 0 ? false : EXPR_IS_TERMINATION(else_stmts[else_stmts_length - 1]->id);

                build_using_basicblock(builder, else_basicblock_id);
                if(assemble_statements(builder, else_stmts, else_stmts_length)) return 1;

                if(!terminated){
                    built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                    ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                    ((ir_instr_break_t*) built_instr)->result_type = NULL;
                    ((ir_instr_break_t*) built_instr)->block_id = end_basicblock_id;
                }

                build_using_basicblock(builder, end_basicblock_id);
            }
            break;
        case EXPR_WHILE: case EXPR_UNTIL: {
                length_t test_basicblock_id = build_basicblock(builder);
                length_t new_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                if(((ast_expr_while_t*) statements[s])->label != NULL){
                    ir_builder_prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = ((ast_expr_while_t*) statements[s])->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = test_basicblock_id;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = test_basicblock_id;

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = test_basicblock_id;
                build_using_basicblock(builder, test_basicblock_id);

                unsigned int conditional_type = statements[s]->id;
                if(assemble_expression(builder, ((ast_expr_while_t*) statements[s])->value, &expression_value, false, &temporary_type)) return 1;

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

                build_using_basicblock(builder, new_basicblock_id);
                if(assemble_statements(builder, while_stmts, while_stmts_length)) return 1;

                if(!terminated){
                    built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                    ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                    ((ir_instr_break_t*) built_instr)->result_type = NULL;
                    ((ir_instr_break_t*) built_instr)->block_id = test_basicblock_id;
                }

                if(((ast_expr_while_t*) statements[s])->label != NULL) builder->block_stack_length--;
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                length_t new_basicblock_id = build_basicblock(builder);
                length_t end_basicblock_id = build_basicblock(builder);

                if(((ast_expr_whilecontinue_t*) statements[s])->label != NULL){
                    ir_builder_prepare_for_new_label(builder);
                    builder->block_stack_labels[builder->block_stack_length] = ((ast_expr_whilecontinue_t*) statements[s])->label;
                    builder->block_stack_break_ids[builder->block_stack_length] = end_basicblock_id;
                    builder->block_stack_continue_ids[builder->block_stack_length] = new_basicblock_id;
                    builder->block_stack_length++;
                }

                length_t prev_break_block_id = builder->break_block_id;
                length_t prev_continue_block_id = builder->continue_block_id;

                builder->break_block_id = end_basicblock_id;
                builder->continue_block_id = new_basicblock_id;

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = new_basicblock_id;

                ast_expr_t **loop_stmts = ((ast_expr_whilecontinue_t*) statements[s])->statements;
                length_t loop_stmts_length = ((ast_expr_whilecontinue_t*) statements[s])->statements_length;
                bool terminated = loop_stmts_length == 0 ? false : EXPR_IS_TERMINATION(loop_stmts[loop_stmts_length - 1]->id);

                build_using_basicblock(builder, new_basicblock_id);
                if(assemble_statements(builder, loop_stmts, loop_stmts_length)) return 1;

                if(!terminated){
                    built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                    ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                    ((ir_instr_break_t*) built_instr)->result_type = NULL;

                    if(statements[s]->id == EXPR_WHILECONTINUE){
                        // 'while continue'
                        ((ir_instr_break_t*) built_instr)->block_id = end_basicblock_id;
                    } else {
                        // 'until break'
                        // LLVM doesn't like when we branch to the same block, so we have to create an intermediate
                        //length_t intermediate_basicblock_id = build_basicblock(builder);
                        //((ir_instr_break_t*) built_instr)->block_id = intermediate_basicblock_id;
                        //build_using_basicblock(builder, intermediate_basicblock_id);
                        //built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                        //((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                        //((ir_instr_break_t*) built_instr)->result_type = NULL;
                        ((ir_instr_break_t*) built_instr)->block_id =  new_basicblock_id;
                    }
                }

                if(((ast_expr_whilecontinue_t*) statements[s])->label != NULL) builder->block_stack_length--;
                build_using_basicblock(builder, end_basicblock_id);

                builder->break_block_id = prev_break_block_id;
                builder->continue_block_id = prev_continue_block_id;
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_expr = (ast_expr_call_method_t*) statements[s];
                ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (call_expr->arity + 1));
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * (call_expr->arity + 1));

                // Resolve & Assemble function arguments
                if(assemble_expression(builder, call_expr->value, &arg_values[0], true, &arg_types[0])){
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
                    built_instr = build_instruction(builder, sizeof(ir_instr_load_t));
                    ((ir_instr_load_t*) built_instr)->id = INSTRUCTION_LOAD;
                    ((ir_instr_load_t*) built_instr)->result_type = (ir_type_t*) arg_values[0]->type->extra;
                    ((ir_instr_load_t*) built_instr)->value = arg_values[0];
                    arg_values[0] = build_value_from_prev_instruction(builder);
                } else {
                    char *s = ast_type_str(&arg_types[0]);
                    compiler_panicf(builder->compiler, call_expr->source, "Can't call methods on type '%s'", s);
                    free(s);
                    ast_type_free(&arg_types[0]);
                    free(arg_types);
                }

                const char *struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;

                for(length_t a = 0; a != call_expr->arity; a++){
                    if(assemble_expression(builder, call_expr->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
                        for(length_t t = 0; t != a + 1; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return 1;
                    }
                }

                // Find function that fits given name and arguments
                funcpair_t pair;
                if(assemble_find_method_conforming(builder, struct_name, call_expr->name,
                        arg_values, arg_types, call_expr->arity + 1, &pair)){
                    compiler_panicf(builder->compiler, call_expr->source, "Undeclared method '%s'", call_expr->name);
                    for(length_t t = 0; t != call_expr->arity + 1; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                    return 1;
                }

                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_call_t* instruction = (ir_instr_call_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                instruction->id = INSTRUCTION_CALL;
                instruction->result_type = pair.ir_func->return_type;
                instruction->values = arg_values;
                instruction->values_length = call_expr->arity + 1;
                instruction->func_id = pair.func_id;
                builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;

                for(length_t t = 0; t != call_expr->arity + 1; t++) ast_type_free(&arg_types[t]);
                free(arg_types);
            }
            break;
        case EXPR_DELETE: {
                ast_expr_delete_t *delete_expr = (ast_expr_delete_t*) statements[s];
                if(assemble_expression(builder, delete_expr->value, &expression_value, false, &temporary_type)) return 1;

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

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = builder->break_block_id;
            }
            break;
        case EXPR_CONTINUE: {
                if(builder->continue_block_id == 0){
                    compiler_panicf(builder->compiler, statements[s]->source, "Nowhere to continue to");
                    return 1;
                }

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = builder->continue_block_id;
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

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = target_block_id;
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

                built_instr = build_instruction(builder, sizeof(ir_instr_break_t));
                ((ir_instr_break_t*) built_instr)->id = INSTRUCTION_BREAK;
                ((ir_instr_break_t*) built_instr)->result_type = NULL;
                ((ir_instr_break_t*) built_instr)->block_id = target_block_id;
            }
            break;
        default:
            compiler_panic(builder->compiler, statements[s]->source, "INTERNAL ERROR: Unimplemented statement in assemble_statements()");
            return 1;
        }
    }

    return 0;
}
