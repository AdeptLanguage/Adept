
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/filename.h"
#include "UTIL/builtin_type.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_type.h"
#include "BRIDGE/rtti.h"
#include "BRIDGE/bridge.h"

errorcode_t ir_gen_expression(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    // NOTE: Generates an ir_value_t from an ast_expr_t
    // NOTE: Will write determined ast_type_t to 'out_expr_type' (Can use NULL to ignore)

    ir_instr_t *instruction;

    #define BUILD_MATH_OP_IvF_MACRO(i, f, o, E) { \
        instruction = ir_gen_math_operands(builder, expr, ir_value, o, out_expr_type); \
        if(instruction == NULL) return FAILURE; \
        if(i_vs_f_instruction((ir_instr_math_t*) instruction, i, f)){ \
            compiler_panic(builder->compiler, ((ast_expr_math_t*) expr)->source, E); \
            ast_type_free(out_expr_type); \
            return FAILURE; \
        } \
    }

    #define BUILD_MATH_OP_UvSvF_MACRO(u, s, f, o, E) { \
        instruction = ir_gen_math_operands(builder, expr, ir_value, o, out_expr_type); \
        if(instruction == NULL) return FAILURE; \
        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) instruction, u, s, f)){ \
            compiler_panic(builder->compiler, ((ast_expr_math_t*) expr)->source, E); \
            ast_type_free(out_expr_type); \
            return FAILURE; \
        } \
    }

    #define BUILD_MATH_OP_BOOL_MACRO(i, E) { \
        instruction = ir_gen_math_operands(builder, expr, ir_value, MATH_OP_ALL_BOOL, out_expr_type); \
        if(instruction == NULL) return FAILURE; \
        instruction->id = i; \
    }

    #define BUILD_LITERAL_IR_VALUE(ast_expr_type, typename, storage_type) { \
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone(typename)); \
        *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t)); \
        (*ir_value)->value_type = VALUE_TYPE_LITERAL; \
        ir_type_map_find(builder->type_map, typename, &((*ir_value)->type)); \
        (*ir_value)->extra = ir_pool_alloc(builder->pool, sizeof(storage_type)); \
        *((storage_type*) (*ir_value)->extra) = ((ast_expr_type*) expr)->value; \
    }

    switch(expr->id){
    case EXPR_BYTE:
        BUILD_LITERAL_IR_VALUE(ast_expr_byte_t, "byte", char); break;
    case EXPR_UBYTE:
        BUILD_LITERAL_IR_VALUE(ast_expr_ubyte_t, "ubyte", unsigned char); break;
    case EXPR_SHORT:
        BUILD_LITERAL_IR_VALUE(ast_expr_short_t, "short", /*stored w/ extra precision*/ int); break;
    case EXPR_USHORT:
        BUILD_LITERAL_IR_VALUE(ast_expr_ushort_t, "ushort", /*stored w/ extra precision*/ unsigned int); break;
    case EXPR_INT:
        BUILD_LITERAL_IR_VALUE(ast_expr_int_t, "int", /*stored w/ extra precision*/ long long); break;
    case EXPR_UINT:
        BUILD_LITERAL_IR_VALUE(ast_expr_uint_t, "uint", /*stored w/ extra precision*/ unsigned long long); break;
    case EXPR_LONG:
        BUILD_LITERAL_IR_VALUE(ast_expr_long_t, "long", long long); break;
    case EXPR_ULONG:
        BUILD_LITERAL_IR_VALUE(ast_expr_ulong_t, "ulong", unsigned long long); break;
    case EXPR_FLOAT:
        BUILD_LITERAL_IR_VALUE(ast_expr_float_t, "float", /*stored w/ extra precision*/ double); break;
    case EXPR_DOUBLE:
        BUILD_LITERAL_IR_VALUE(ast_expr_double_t, "double", double); break;
    case EXPR_BOOLEAN:
        BUILD_LITERAL_IR_VALUE(ast_expr_boolean_t, "bool", bool); break;
    case EXPR_NULL:
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("ptr"));
        *ir_value = build_null_pointer(builder->pool);
        break;
    case EXPR_ADD:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ADD, INSTRUCTION_FADD, INSTRUCTION_NONE, "add", "__add__", false))
            return FAILURE;
        break;
    case EXPR_SUBTRACT:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT, INSTRUCTION_NONE, "subtract", "__subtract__", false))
            return FAILURE;
        break;
    case EXPR_MULTIPLY:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_MULTIPLY, INSTRUCTION_FMULTIPLY, INSTRUCTION_NONE, "multiply", "__multiply__", false))
            return FAILURE;
        break;
    case EXPR_DIVIDE:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UDIVIDE, INSTRUCTION_SDIVIDE, INSTRUCTION_FDIVIDE, "divide", "__divide__", false))
            return FAILURE;
        break;
    case EXPR_MODULUS:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UMODULUS, INSTRUCTION_SMODULUS, INSTRUCTION_FMODULUS, "take the modulus of", "__modulus__", false))
            return FAILURE;
        break;
    case EXPR_EQUALS:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_EQUALS, INSTRUCTION_FEQUALS, INSTRUCTION_NONE, "test equality for", "__equals__", true))
            return FAILURE;
        break;
    case EXPR_NOTEQUALS:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_NOTEQUALS, INSTRUCTION_FNOTEQUALS, INSTRUCTION_NONE, "test inequality for", "__not_equals__", true))
            return FAILURE;
        break;
    case EXPR_GREATER:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UGREATER, INSTRUCTION_SGREATER, INSTRUCTION_FGREATER, "compare", "__greater_than__", true))
            return FAILURE;
        break;
    case EXPR_LESSER:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ULESSER, INSTRUCTION_SLESSER, INSTRUCTION_FLESSER, "compare", "__less_than__", true))
            return FAILURE;
        break;
    case EXPR_GREATEREQ:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UGREATEREQ, INSTRUCTION_SGREATEREQ, INSTRUCTION_FGREATEREQ, "compare", "__greater_than_or_equal__", true))
            return FAILURE;
        break;
    case EXPR_LESSEREQ:
        if(differentiate_math_operation(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ULESSEREQ, INSTRUCTION_SLESSEREQ, INSTRUCTION_FLESSEREQ, "compare", "__less_than_or_equal__", true))
            return FAILURE;
        break;
    case EXPR_AND:
        BUILD_MATH_OP_BOOL_MACRO(INSTRUCTION_AND, "Can't use operator 'and' on those types"); break;
    case EXPR_OR:
        BUILD_MATH_OP_BOOL_MACRO(INSTRUCTION_OR, "Can't use operator 'or' on those types"); break;
    case EXPR_STR:
        if(builder->object->ir_module.common.ir_string_struct == NULL){
            compiler_panic(builder->compiler, expr->source, "Can't create string literal without String type present");
            printf("\nTry importing '2.1/String.adept'\n");
            return FAILURE;
        }
        
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("String"));
        *ir_value = build_literal_str(builder, ((ast_expr_str_t*) expr)->array, ((ast_expr_str_t*) expr)->length);
        // NOTE: build_literal_str shouldn't return NULL since we verified that 'String' type is present
        break;
    case EXPR_CSTR:
        if(out_expr_type != NULL) ast_type_make_base_ptr(out_expr_type, strclone("ubyte"));
        *ir_value = build_literal_cstr(builder, ((ast_expr_cstr_t*) expr)->value);
        break;
    case EXPR_VARIABLE: {
            char *variable_name = ((ast_expr_variable_t*) expr)->name;
            bridge_var_t *variable = bridge_var_scope_find_var(builder->var_scope, variable_name);
            length_t var_index;

            if(variable){
                if(out_expr_type != NULL) *out_expr_type = ast_type_clone(variable->ast_type);
                ir_type_t *ir_ptr_type = ir_type_pointer_to(builder->pool, variable->ir_type);

                // Variable-Pointer instruction to get pointer to stack variable
                *ir_value = build_varptr(builder, ir_ptr_type, variable->id);

                // Load if the variable is a reference
                if(variable->traits & BRIDGE_VAR_REFERENCE) *ir_value = build_load(builder, *ir_value);

                // Unless requested to leave the expression mutable, dereference it
                if(!leave_mutable) *ir_value = build_load(builder, *ir_value);
                break;
            }

            ast_t *ast = &builder->object->ast;
            ir_module_t *ir_module = &builder->object->ir_module;

            // Attempt to find global variable
            bool found_global = false;
            for(var_index = 0; var_index != ir_module->globals_length; var_index++){
                if(strcmp(variable_name, ir_module->globals[var_index].name) == 0){
                    found_global = true; break;
                }
            }

            if(found_global){
                // Assume it exists in ast's globals
                if(out_expr_type != NULL) for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(variable_name, ast->globals[g].name) == 0){
                        *out_expr_type = ast_type_clone(&ast->globals[g].type);
                        break;
                    }
                }

                ir_global_t *global = &ir_module->globals[var_index];
                ir_type_t *global_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                global_pointer_type->kind = TYPE_KIND_POINTER;
                global_pointer_type->extra = global->type;

                *ir_value = build_gvarptr(builder, global_pointer_type, var_index);

                // If not requested to leave the expression mutable, dereference it
                if(!leave_mutable){
                    *ir_value = build_load(builder, *ir_value);
                }
                break;
            }

            // No suitable variable or global found
            compiler_panicf(builder->compiler, ((ast_expr_variable_t*) expr)->source, "Undeclared variable '%s'", variable_name);
            const char *nearest = bridge_var_scope_nearest(builder->var_scope, variable_name);
            if(nearest) printf("\nDid you mean '%s'?\n", nearest);
            return FAILURE;
        }
        break;
    case EXPR_CALL: {
            ast_expr_call_t *call_expr = (ast_expr_call_t*) expr;
            ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * call_expr->arity);
            ast_type_t *arg_types = malloc(sizeof(ast_type_t) * call_expr->arity);

            bool found_variable = false;
            ast_type_t *ast_var_type;
            ir_type_t *ir_var_type;
            ir_type_t *ir_return_type;
            ast_t *ast = &builder->object->ast;
            bool hard_break = false;

            // Resolve & ir_gen function arguments
            for(length_t a = 0; a != call_expr->arity; a++){
                if(ir_gen_expression(builder, call_expr->args[a], &arg_values[a], false, &arg_types[a])){
                    for(length_t t = 0; t != a; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                    return FAILURE;
                }
            }

            bridge_var_t *var = bridge_var_scope_find_var(builder->var_scope, call_expr->name);

            if(var){
                ast_var_type = var->ast_type;
                ir_var_type = ir_type_pointer_to(builder->pool, var->ir_type);

                if(ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                    if(call_expr->is_tentative) break;
                    char *s = ast_type_str(ast_var_type);
                    compiler_panicf(builder->compiler, call_expr->source, "Can't call value of non function type '%s'", s);
                    ast_types_free_fully(arg_types, call_expr->arity);
                    free(s);
                    return FAILURE;
                }

                ast_type_t *ast_function_return_type = ((ast_elem_func_t*) ast_var_type->elements[0])->return_type;

                if(ir_gen_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                    for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);
                    return FAILURE;
                }

                if(ast_var_type->elements_length == 1 && ast_var_type->elements[0]->id == AST_ELEM_FUNC){
                    *ir_value = build_varptr(builder, ir_var_type, var->id);
                    *ir_value = build_load(builder, *ir_value);
                }

                found_variable = true;
            }

            if(!found_variable) for(length_t g = 0; g != ast->globals_length; g++){
                if(strcmp(ast->globals[g].name, call_expr->name) == 0){
                    ast_var_type = &ast->globals[g].type;

                    // We could search the ir var map, but this is easier
                    // TODO: Create easy way to get between ast and ir var maps
                    ir_var_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                    ir_var_type->kind = TYPE_KIND_POINTER;

                    if(ir_gen_resolve_type(builder->compiler, builder->object, ast_var_type, (ir_type_t**) &ir_var_type->extra)){
                        for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return FAILURE;
                    }

                    if(ast_var_type->elements[0]->id != AST_ELEM_FUNC){
                        if(call_expr->is_tentative){
                            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                            hard_break = true;
                            break;
                        }

                        char *s = ast_type_str(ast_var_type);
                        compiler_panicf(builder->compiler, call_expr->source, "Can't call value of non function type '%s'", s);
                        ast_types_free_fully(arg_types, call_expr->arity);
                        free(s);
                        return FAILURE;
                    }

                    ast_type_t *ast_function_return_type = ((ast_elem_func_t*) ast_var_type->elements[0])->return_type;

                    if(ir_gen_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
                        for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                        free(arg_types);
                        return FAILURE;
                    }

                    if(ast_var_type->elements_length == 1 && ast_var_type->elements[0]->id == AST_ELEM_FUNC){
                        *ir_value = build_gvarptr(builder, ir_var_type, g);
                        *ir_value = build_load(builder, *ir_value);
                        found_variable = true;
                    }
                    break;
                }
            }

            // Allow for hard break of globals loop
            if(hard_break) break;

            if(found_variable){
                // This is ok since we previously checked that (ast_var_type->elements[0]->id == AST_ELEM_FUNC)
                ast_elem_func_t *function_elem = (ast_elem_func_t*) ast_var_type->elements[0];

                if(function_elem->traits & AST_FUNC_VARARG){
                    if(function_elem->arity > call_expr->arity){
                        if(call_expr->is_tentative){
                            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                            break;
                        }
                        
                        compiler_panicf(builder->compiler, call_expr->source, "Incorrect argument count (at least %d expected, %d given)", (int) function_elem->arity, (int) call_expr->arity);
                        return FAILURE;
                    }
                } else if(function_elem->arity != call_expr->arity){
                    if(call_expr->is_tentative){
                        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                        break;
                    }

                    compiler_panicf(builder->compiler, call_expr->source, "Incorrect argument count (%d expected, %d given)", (int) function_elem->arity, (int) call_expr->arity);
                    return FAILURE;
                }

                for(length_t a = 0; a != function_elem->arity; a++){
                    if(!ast_types_conform(builder, &arg_values[a], &arg_types[a], &function_elem->arg_types[a], CONFORM_MODE_CALL_ARGUMENTS)){
                        if(call_expr->is_tentative){
                            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                            hard_break = true;
                            break;
                        }

                        char *s1 = ast_type_str(&function_elem->arg_types[a]);
                        char *s2 = ast_type_str(&arg_types[a]);
                        compiler_panicf(builder->compiler, call_expr->args[a]->source, "Required argument type '%s' is incompatible with type '%s'", s1, s2);
                        free(s1);
                        free(s2);
                        return FAILURE;
                    }
                }

                if(hard_break) break;
                handle_pass_management(builder, arg_values, arg_types, NULL, call_expr->arity);

                ir_basicblock_new_instructions(builder->current_block, 1);
                instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_address_t));
                ((ir_instr_call_address_t*) instruction)->id = INSTRUCTION_CALL_ADDRESS;
                ((ir_instr_call_address_t*) instruction)->result_type = ir_return_type;
                ((ir_instr_call_address_t*) instruction)->address = *ir_value;
                ((ir_instr_call_address_t*) instruction)->values = arg_values;
                ((ir_instr_call_address_t*) instruction)->values_length = call_expr->arity;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                *ir_value = build_value_from_prev_instruction(builder);

                for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                free(arg_types);

                if(out_expr_type != NULL) *out_expr_type = ast_type_clone(function_elem->return_type);
            } else {
                // Find function that fits given name and arguments
                funcpair_t pair;
                errorcode_t error = ir_gen_find_func_conforming(builder, call_expr->name, arg_values, arg_types, call_expr->arity, &pair);

                if(error){
                    if(error == FAILURE && !call_expr->is_tentative){
                        compiler_undeclared_function(builder->compiler, builder->object, expr->source, call_expr->name, arg_types, call_expr->arity);
                    }

                    for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                    free(arg_types);

                    if(call_expr->is_tentative){
                        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                        break;
                    }

                    return FAILURE;
                }

                if(pair.ast_func->traits & AST_FUNC_VARARG){
                    trait_t arg_type_traits[call_expr->arity];
                    memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
                    memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (call_expr->arity - pair.ast_func->arity));
                    handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity);
                } else {
                    handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity);
                }

                ir_basicblock_new_instructions(builder->current_block, 1);
                instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                ((ir_instr_call_t*) instruction)->id = INSTRUCTION_CALL;
                ((ir_instr_call_t*) instruction)->result_type = pair.ir_func->return_type;
                ((ir_instr_call_t*) instruction)->values = arg_values;
                ((ir_instr_call_t*) instruction)->values_length = call_expr->arity;
                ((ir_instr_call_t*) instruction)->ir_func_id = pair.ir_func_id;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                *ir_value = build_value_from_prev_instruction(builder);

                for(length_t t = 0; t != call_expr->arity; t++) ast_type_free(&arg_types[t]);
                free(arg_types);

                if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&pair.ast_func->return_type);
            }
        }
        break;
    case EXPR_MEMBER: {
            ast_expr_member_t *member_expr = (ast_expr_member_t*) expr;
            ir_value_t *struct_value;
            ast_type_t struct_value_ast_type;

            // This expression should be able to be mutable (Checked during parsing)
            if(ir_gen_expression(builder, member_expr->value, &struct_value, true, &struct_value_ast_type)) return FAILURE;

            if(struct_value->type->kind != TYPE_KIND_POINTER){
                char *given_type = ast_type_str(&struct_value_ast_type);
                compiler_panicf(builder->compiler, member_expr->source, "Can't use member operator '.' on temporary value of type '%s'", given_type);
                free(given_type);
                ast_type_free(&struct_value_ast_type);
                return FAILURE;
            }

            // Auto-derefernce '*something' types
            if(struct_value_ast_type.elements_length > 1 && struct_value_ast_type.elements[0]->id == AST_ELEM_POINTER
                && struct_value_ast_type.elements[1]->id != AST_ELEM_POINTER){
                // Modify ast_type_t to remove a pointer element from the front
                // DANGEROUS: Manually deleting ast_elem_pointer_t
                free(struct_value_ast_type.elements[0]);
                memmove(struct_value_ast_type.elements, &struct_value_ast_type.elements[1], sizeof(ast_elem_t*) * (struct_value_ast_type.elements_length - 1));
                struct_value_ast_type.elements_length--; // Reduce length accordingly

                if(expr_is_mutable(member_expr->value)){
                    struct_value = build_load(builder, struct_value);
                }
            }

            if(struct_value_ast_type.elements_length == 0){
                compiler_panicf(builder->compiler, expr->source, "INTERNAL ERROR: Member expression in ir_gen_expression received bad AST type");
                ast_type_free(&struct_value_ast_type);
                return FAILURE;

            }

            length_t field_index;
            ir_type_t *field_type;
            ast_elem_t *elem = struct_value_ast_type.elements[0];

            switch(elem->id){
            case AST_ELEM_BASE: {
                    char *struct_name = ((ast_elem_base_t*) elem)->base;
                    ast_struct_t *target = ast_struct_find(&builder->object->ast, struct_name);

                    if(target == NULL){
                        if(typename_builtin_type(struct_name) != BUILTIN_TYPE_NONE){
                            compiler_panicf(builder->compiler, expr->source, "Can't use member operator on built-in type '%s'", struct_name);
                        } else {
                            compiler_panicf(builder->compiler, ((ast_expr_member_t*) expr)->source, "INTERNAL ERROR: Failed to find struct '%s' that should exist", struct_name);
                        }
                        ast_type_free(&struct_value_ast_type);
                        return FAILURE;
                    }

                    if(!ast_struct_find_field(target, member_expr->member, &field_index)){
                        char *struct_typename = ast_type_str(&struct_value_ast_type);
                        compiler_panicf(builder->compiler, ((ast_expr_member_t*) expr)->source, "Field '%s' doesn't exist in struct '%s'", member_expr->member, struct_typename);
                        ast_type_free(&struct_value_ast_type);
                        free(struct_typename);
                        return FAILURE;
                    }

                    if(ir_gen_resolve_type(builder->compiler, builder->object, &target->field_types[field_index], &field_type)){
                        ast_type_free(&struct_value_ast_type);
                        return FAILURE;
                    }

                    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&target->field_types[field_index]);
                }
                break;
            case AST_ELEM_GENERIC_BASE: {
                    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) elem;

                    char *poly_struct_name = generic_base->name;
                    ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&builder->object->ast, poly_struct_name);

                    if(template == NULL){
                        compiler_panicf(builder->compiler, ((ast_expr_member_t*) expr)->source, "INTERNAL ERROR: Failed to find polymorphic struct '%s' that should exist", poly_struct_name);
                        ast_type_free(&struct_value_ast_type);
                        return FAILURE;
                    }

                    if(!ast_struct_find_field((ast_struct_t*) template, member_expr->member, &field_index)){
                        char *struct_typename = ast_type_str(&struct_value_ast_type);
                        compiler_panicf(builder->compiler, ((ast_expr_member_t*) expr)->source, "Field '%s' doesn't exist in struct '%s'", member_expr->member, struct_typename);
                        ast_type_free(&struct_value_ast_type);
                        free(struct_typename);
                        return FAILURE;
                    }

                    // Substitute generic type parameters
                    ast_type_var_catalog_t catalog;
                    ast_type_var_catalog_init(&catalog);

                    if(template->generics_length != generic_base->generics_length){
                        redprintf("INTERNAL ERROR: Polymorphic struct '%s' type parameter length mismatch when generating runtime type table!\n", generic_base->name);
                        ast_type_free(&struct_value_ast_type);
                        ast_type_var_catalog_free(&catalog);
                        return FAILURE;
                    }

                    for(length_t i = 0; i != template->generics_length; i++){
                        ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
                    }

                    ast_type_t ast_field_type;
                    if(resolve_type_polymorphics(builder->compiler, &catalog, &template->field_types[field_index], &ast_field_type)){
                        ast_type_free(&struct_value_ast_type);
                        ast_type_var_catalog_free(&catalog);
                        return FAILURE;
                    }

                    if(ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &field_type)){
                        ast_type_free(&struct_value_ast_type);
                        ast_type_var_catalog_free(&catalog);
                        return FAILURE;
                    }

                    if(out_expr_type != NULL) *out_expr_type = ast_field_type;
                    else                      ast_type_free(&ast_field_type);

                    ast_type_var_catalog_free(&catalog);
                }
                break;
            default: {
                    char *nonstruct_typename = ast_type_str(&struct_value_ast_type);
                    compiler_panicf(builder->compiler, expr->source, "Can't use member operator on non-struct type '%s'", nonstruct_typename);
                    ast_type_free(&struct_value_ast_type);
                    free(nonstruct_typename);
                    return FAILURE;
                }
            }

            ir_type_t *field_ptr_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            field_ptr_type->kind = TYPE_KIND_POINTER;
            field_ptr_type->extra = field_type;

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_member_t));
            ((ir_instr_member_t*) instruction)->id = INSTRUCTION_MEMBER;
            ((ir_instr_member_t*) instruction)->result_type = field_ptr_type;
            ((ir_instr_member_t*) instruction)->value = struct_value;
            ((ir_instr_member_t*) instruction)->member = field_index;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);

            // If not requested to leave the expression mutable, dereference it
            if(!leave_mutable){
                *ir_value = build_load(builder, *ir_value);
            }

            ast_type_free(&struct_value_ast_type);
        }
        break;
    case EXPR_ADDRESS: {
            // This expression should be able to be mutable (Checked during parsing)
            if(ir_gen_expression(builder, ((ast_expr_unary_t*) expr)->value, ir_value, true, out_expr_type)) return FAILURE;
            if(out_expr_type != NULL) ast_type_prepend_ptr(out_expr_type);
        }
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr_expr = (ast_expr_func_addr_t*) expr;

            funcpair_t pair;

            if(func_addr_expr->match_args == NULL){
                if(ir_gen_find_func_named(builder->compiler, builder->object, func_addr_expr->name, &pair)){
                    compiler_panicf(builder->compiler, expr->source, "Undeclared function '%s'", func_addr_expr->name);
                    return FAILURE;
                }
            } else if(ir_gen_find_func(builder->compiler, builder->object, func_addr_expr->name, func_addr_expr->match_args, func_addr_expr->match_args_length, &pair)){
                compiler_undeclared_function(builder->compiler, builder->object, func_addr_expr->source, func_addr_expr->name, func_addr_expr->match_args, func_addr_expr->match_args_length);
                return FAILURE;
            }

            ir_type_extra_function_t *extra = ir_pool_alloc(builder->pool, sizeof(ir_type_extra_function_t));
            extra->arg_types = pair.ir_func->argument_types;
            extra->arity = pair.ast_func->arity;
            extra->return_type = pair.ir_func->return_type;
            extra->traits = func_addr_expr->traits;

            ir_type_t *ir_funcptr_type = ir_pool_alloc(builder->pool, sizeof(ir_instr_func_address_t));
            ir_funcptr_type->kind = TYPE_KIND_FUNCPTR;
            ir_funcptr_type->extra = extra;

            const char *maybe_name = pair.ast_func->traits & AST_FUNC_FOREIGN ||
                pair.ast_func->traits & AST_FUNC_MAIN ? func_addr_expr->name : NULL;

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_func_address_t));
            ((ir_instr_func_address_t*) instruction)->id = INSTRUCTION_FUNC_ADDRESS;
            ((ir_instr_func_address_t*) instruction)->result_type = ir_funcptr_type;
            ((ir_instr_func_address_t*) instruction)->name = maybe_name;
            ((ir_instr_func_address_t*) instruction)->ir_func_id = pair.ir_func_id;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);

            if(out_expr_type != NULL){
                out_expr_type->elements = malloc(sizeof(ast_elem_t*));
                out_expr_type->elements_length = 1;
                out_expr_type->source = func_addr_expr->source;

                ast_elem_func_t *function_elem = malloc(sizeof(ast_elem_func_t));
                function_elem->id = AST_ELEM_FUNC;
                function_elem->source = func_addr_expr->source;
                function_elem->arg_types = pair.ast_func->arg_types;
                function_elem->arity = pair.ast_func->arity;
                function_elem->return_type = &pair.ast_func->return_type;
                function_elem->traits = pair.ast_func->traits;
                function_elem->ownership = false;

                out_expr_type->elements[0] = (ast_elem_t*) function_elem;
            }
        }
        break;
    case EXPR_DEREFERENCE: {
            ast_expr_unary_t *dereference_expr = (ast_expr_unary_t*) expr;
            ast_type_t expr_type;
            ir_value_t *expr_value;

            if(ir_gen_expression(builder, dereference_expr->value, &expr_value, false, &expr_type)) return FAILURE;

            // Ensure that the result ast_type_t is a pointer type
            if(expr_type.elements_length < 2 || expr_type.elements[0]->id != AST_ELEM_POINTER){
                char *expr_type_str = ast_type_str(&expr_type);
                compiler_panicf(builder->compiler, dereference_expr->source, "Can't dereference non-pointer type '%s'", expr_type_str);
                ast_type_free(&expr_type);
                free(expr_type_str);
                return FAILURE;
            }

            // Modify ast_type_t to remove a pointer element from the front
            // DANGEROUS: Manually deleting ast_elem_pointer_t
            free(expr_type.elements[0]);
            memmove(expr_type.elements, &expr_type.elements[1], sizeof(ast_elem_t*) * (expr_type.elements_length - 1));
            expr_type.elements_length--; // Reduce length accordingly

            // If not requested to leave the expression mutable, dereference it
            if(!leave_mutable){
                // ir_type_t is expected to be of kind pointer
                if(expr_value->type->kind != TYPE_KIND_POINTER){
                    compiler_panic(builder->compiler, dereference_expr->source, "INTERNAL ERROR: Expected ir_type_t to be a pointer inside EXPR_DEREFERENCE of ir_gen_expression()");
                    ast_type_free(&expr_type);
                    return FAILURE;
                }

                // Build and append a load instruction
                *ir_value = build_load(builder, expr_value);

                // ir_type_t is expected to be of kind pointer
                if(expr_value->type->kind != TYPE_KIND_POINTER){
                    compiler_panic(builder->compiler, dereference_expr->source, "INTERNAL ERROR: Expected ir_type_t to be a pointer inside EXPR_DEREFERENCE of ir_gen_expression()");
                    ast_type_free(&expr_type);
                    return FAILURE;
                }
            } else {
                *ir_value = expr_value;
            }

            if(out_expr_type != NULL) *out_expr_type = expr_type;
            else ast_type_free(&expr_type);
        }
        break;
    case EXPR_ARRAY_ACCESS: case EXPR_AT: {
            ast_expr_array_access_t *array_access_expr = (ast_expr_array_access_t*) expr;
            ast_type_t index_type, array_type;
            ir_value_t *index_value, *array_value;

            if(ir_gen_expression(builder, array_access_expr->value, &array_value, true, &array_type)) return FAILURE;
            if(ir_gen_expression(builder, array_access_expr->index, &index_value, false, &index_type)){
                ast_type_free(&array_type);
                return FAILURE;
            }

            if(array_value->type->kind != TYPE_KIND_POINTER){
                char *given_type = ast_type_str(&array_type);

                compiler_panicf(builder->compiler, array_access_expr->source, "Can't use operator %s on temporary value of type '%s'",
                    expr->id == EXPR_ARRAY_ACCESS ? "[]" : "'at'", given_type);

                free(given_type);
                ast_type_free(&array_type);
                ast_type_free(&index_type);
                return FAILURE;
            }

            if(((ir_type_t*) array_value->type->extra)->kind == TYPE_KIND_FIXED_ARRAY){
                // Bitcast reference (that's to a fixed array of element) to pointer of element
                // (*)  [10] int -> *int

                assert(array_type.elements_length != 0);

                ir_type_t *casted_ir_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                casted_ir_type->kind = TYPE_KIND_POINTER;
                casted_ir_type->extra = ((ir_type_extra_fixed_array_t*) ((ir_type_t*) array_value->type->extra)->extra)->subtype;
                array_type.elements[0]->id = AST_ELEM_POINTER;
                array_value = build_bitcast(builder, array_value, casted_ir_type);
            } else if(expr_is_mutable(array_access_expr->value)){
                // Load value reference
                // (*)  int -> int
                array_value = build_load(builder, array_value);
            }

            if(index_value->type->kind < TYPE_KIND_S8 || index_value->type->kind > TYPE_KIND_U64){
                compiler_panic(builder->compiler, array_access_expr->index->source, "Array index value must be an integer type");
                ast_type_free(&array_type);
                ast_type_free(&index_type);
                return FAILURE;
            }
            
            // Ensure array type is a pointer
            if(array_value->type->kind != TYPE_KIND_POINTER || array_type.elements_length < 2 || array_type.elements[0]->id != AST_ELEM_POINTER){
                char *given_type = ast_type_str(&array_type);

                char *s = ir_type_str(array_value->type);
                puts(s);
                free(s);

                compiler_panicf(builder->compiler, array_access_expr->source, "Can't use operator %s on non-array type '%s'",
                    expr->id == EXPR_ARRAY_ACCESS ? "[]" : "'at'", given_type);

                free(given_type);
                ast_type_free(&array_type);
                ast_type_free(&index_type);
                return FAILURE;
            }

            if(expr->id == EXPR_ARRAY_ACCESS){
                // Change 'array_type' to be the type of an element instead of the whole array
                // Modify ast_type_t to remove a pointer element from the front
                // DANGEROUS: Manually deleting ast_elem_pointer_t
                free(array_type.elements[0]);
                memmove(array_type.elements, &array_type.elements[1], sizeof(ast_elem_t*) * (array_type.elements_length - 1));
                array_type.elements_length--; // Reduce length accordingly

                ir_basicblock_new_instructions(builder->current_block, 1);
                instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_array_access_t));
                ((ir_instr_array_access_t*) instruction)->id = INSTRUCTION_ARRAY_ACCESS;
                ((ir_instr_array_access_t*) instruction)->result_type = array_value->type;
                ((ir_instr_array_access_t*) instruction)->value = array_value;
                ((ir_instr_array_access_t*) instruction)->index = index_value;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                *ir_value = build_value_from_prev_instruction(builder);

                // If not requested to leave the expression mutable, dereference it
                if(!leave_mutable){
                    *ir_value = build_load(builder, *ir_value);
                }
            } else /* EXPR_AT */ {
                ir_basicblock_new_instructions(builder->current_block, 1);
                instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_array_access_t));
                ((ir_instr_array_access_t*) instruction)->id = INSTRUCTION_ARRAY_ACCESS;
                ((ir_instr_array_access_t*) instruction)->result_type = array_value->type;
                ((ir_instr_array_access_t*) instruction)->value = array_value;
                ((ir_instr_array_access_t*) instruction)->index = index_value;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                *ir_value = build_value_from_prev_instruction(builder);
            }

            ast_type_free(&index_type);

            if(out_expr_type != NULL) *out_expr_type = array_type; // array_type was modified to be the element type (bad ik but whatever)
            else ast_type_free(&array_type);
        }
        break;
    case EXPR_CAST: {
            ast_expr_cast_t *cast_expr = (ast_expr_cast_t*) expr;
            ast_type_t from_type;

            if(ir_gen_expression(builder, cast_expr->from, ir_value, false, &from_type)) return FAILURE;

            if(!ast_types_conform(builder, ir_value, &from_type, &cast_expr->to, CONFORM_MODE_ALL)){
                char *a_type_str = ast_type_str(&from_type);
                char *b_type_str = ast_type_str(&cast_expr->to);
                compiler_panicf(builder->compiler, expr->source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
                free(a_type_str);
                free(b_type_str);
                ast_type_free(&from_type);
                return FAILURE;
            }

            ast_type_free(&from_type);
            if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&cast_expr->to);
        }
        break;
    case EXPR_SIZEOF: {
            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_sizeof_t));
            ((ir_instr_sizeof_t*) instruction)->id = INSTRUCTION_SIZEOF;
            ((ir_instr_sizeof_t*) instruction)->result_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ((ir_instr_sizeof_t*) instruction)->result_type->kind = TYPE_KIND_U64;
            // result_type->extra should never be accessed

            if(ir_gen_resolve_type(builder->compiler, builder->object, &((ast_expr_sizeof_t*) expr)->type,
                    &((ir_instr_sizeof_t*) instruction)->type)) return FAILURE;

            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);

            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("usize"));
        }
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_expr = (ast_expr_call_method_t*) expr;
            ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (call_expr->arity + 1));
            ast_type_t *arg_types = malloc(sizeof(ast_type_t) * (call_expr->arity + 1));

            // Generate primary argument
            if(ir_gen_expression(builder, call_expr->value, &arg_values[0], true, &arg_types[0])){
                free(arg_types);
                return FAILURE;
            }

            // Validate and prepare primary argument
            ast_elem_t **type_elems = arg_types[0].elements;
            length_t type_elems_length = arg_types[0].elements_length;

            if(type_elems_length == 1 && (type_elems[0]->id == AST_ELEM_BASE || type_elems[0]->id == AST_ELEM_GENERIC_BASE)){
                if(!expr_is_mutable(call_expr->value)){
                    if(call_expr->is_tentative){
                        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                        break;
                    }
                    
                    compiler_panic(builder->compiler, call_expr->source, "Can't call method on immutable value");
                    ast_type_free(&arg_types[0]);
                    free(arg_types);
                    return FAILURE;
                }

                ast_type_prepend_ptr(&arg_types[0]);
            } else if(type_elems_length == 2 && type_elems[0]->id == AST_ELEM_POINTER
                    && (type_elems[1]->id == AST_ELEM_BASE || type_elems[1]->id == AST_ELEM_GENERIC_BASE)){
                // Load the value that's being called on if the expression is mutable
                if(expr_is_mutable(call_expr->value)){
                    arg_values[0] = build_load(builder, arg_values[0]);
                }
            } else {
                if(call_expr->is_tentative){
                    if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                    break;
                }
                
                char *s = ast_type_str(&arg_types[0]);
                compiler_panicf(builder->compiler, call_expr->source, "Can't call methods on type '%s'", s);
                ast_type_free(&arg_types[0]);
                free(arg_types);
                free(s);
                return FAILURE;
            }

            // Generate secondary argument values
            for(length_t a = 0; a != call_expr->arity; a++){
                if(ir_gen_expression(builder, call_expr->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
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
                    if(ir_gen_find_method_conforming(builder, struct_name, call_expr->name,
                            arg_values, arg_types, call_expr->arity + 1, &pair)){
                        if(call_expr->is_tentative){
                            tentative_fell_through = true;
                            break;
                        }

                        compiler_panicf(builder->compiler, call_expr->source, "Undeclared method '%s'", call_expr->name);
                        ast_types_free_fully(arg_types, call_expr->arity + 1);
                        return FAILURE;
                    }
                    
                    break;
                }
            case AST_ELEM_GENERIC_BASE: {
                    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[1];

                    if(generic_base->name_is_polymorphic){
                        if(call_expr->is_tentative){
                            tentative_fell_through = true;
                            break;
                        }

                        compiler_panic(builder->compiler, generic_base->source, "Can't call method on struct value with unresolved polymorphic name");
                        ast_types_free_fully(arg_types, call_expr->arity + 1);
                        return FAILURE;
                    }

                    if(ir_gen_find_generic_base_method_conforming(builder, generic_base->name, call_expr->name, arg_values, arg_types, call_expr->arity + 1, &pair)){
                        if(call_expr->is_tentative){
                            tentative_fell_through = true;
                            break;
                        }

                        compiler_panicf(builder->compiler, call_expr->source, "Undeclared method '%s'", call_expr->name);
                        ast_types_free_fully(arg_types, call_expr->arity + 1);
                        return FAILURE;
                    }

                    break;
                }
            default:
                redprintf("INTERNAL ERROR: EXPR_CALL_METHOD in ir_gen_expr.c received bad primary element id\n")
                ast_types_free_fully(arg_types, call_expr->arity + 1);
                return FAILURE;
            }

            if(tentative_fell_through){
                if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                ast_types_free_fully(arg_types, call_expr->arity + 1);
                break;
            }

            if(pair.ast_func->traits & AST_FUNC_VARARG){
                trait_t arg_type_traits[call_expr->arity + 1];
                memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
                memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (call_expr->arity + 1 - pair.ast_func->arity));
                handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity + 1);
            } else {
                handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, call_expr->arity + 1);
            }

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
            ((ir_instr_call_t*) instruction)->id = INSTRUCTION_CALL;
            ((ir_instr_call_t*) instruction)->result_type = pair.ir_func->return_type;
            ((ir_instr_call_t*) instruction)->values = arg_values;
            ((ir_instr_call_t*) instruction)->values_length = call_expr->arity + 1;
            ((ir_instr_call_t*) instruction)->ir_func_id = pair.ir_func_id;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);

            ast_types_free_fully(arg_types, call_expr->arity + 1);
            if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&pair.ast_func->return_type);
        }
        break;
    case EXPR_NOT: case EXPR_NEGATE: case EXPR_BIT_COMPLEMENT: {
            ast_expr_unary_t *unary_expr = (ast_expr_unary_t*) expr;
            ast_type_t expr_type;
            ir_value_t *expr_value;

            #define MACRO_UNARY_OPERATOR_CHARCTER (expr->id == EXPR_NOT ? '!' : (expr->id == EXPR_NEGATE ? '-' : '~'))

            if(ir_gen_expression(builder, unary_expr->value, &expr_value, false, &expr_type)) return FAILURE;

            if(ir_type_get_catagory(expr_value->type) == PRIMITIVE_NA){
                char *s = ast_type_str(&expr_type);
                compiler_panicf(builder->compiler, expr->source, "Can't use '%c' operator on type '%s'", MACRO_UNARY_OPERATOR_CHARCTER, s);
                ast_type_free(&expr_type);
                free(s);
                return FAILURE;
            }

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_unary_t));
            ((ir_instr_unary_t*) instruction)->value = expr_value;

            if(expr->id == EXPR_NOT){
                // Build and append an 'iszero' instruction
                ((ir_instr_unary_t*) instruction)->id = INSTRUCTION_ISZERO;
                ((ir_instr_unary_t*) instruction)->result_type = ir_builder_bool(builder);
                if(out_expr_type) ast_type_make_base(out_expr_type, strclone("bool"));
            } else {
                // Build and append an 'negate' instruction
                ((ir_instr_unary_t*) instruction)->result_type = expr_value->type;

                switch(((ir_instr_unary_t*) instruction)->value->type->kind){
                case TYPE_KIND_POINTER: case TYPE_KIND_BOOLEAN:
                case TYPE_KIND_U8: case TYPE_KIND_U16: case TYPE_KIND_U32: case TYPE_KIND_U64:
                case TYPE_KIND_S8: case TYPE_KIND_S16: case TYPE_KIND_S32: case TYPE_KIND_S64:
                    instruction->id = (expr->id == EXPR_NEGATE ? INSTRUCTION_NEGATE : INSTRUCTION_BIT_COMPLEMENT);
                    break;
                case TYPE_KIND_HALF: case TYPE_KIND_FLOAT: case TYPE_KIND_DOUBLE:
                    if(expr->id == EXPR_BIT_COMPLEMENT){
                        char *s = ast_type_str(&expr_type);
                        compiler_panicf(builder->compiler, expr->source, "Can't use '%c' operator on type '%s'", MACRO_UNARY_OPERATOR_CHARCTER, s);
                        ast_type_free(&expr_type);
                        free(s);
                        return FAILURE;
                    }
                    instruction->id = INSTRUCTION_FNEGATE; break;
                default: {
                        char *s = ast_type_str(&expr_type);
                        compiler_panicf(builder->compiler, expr->source, "Can't use '%c' operator on type '%s'", MACRO_UNARY_OPERATOR_CHARCTER, s);
                        ast_type_free(&expr_type);
                        free(s);
                        return FAILURE;
                    }
                }

                if(out_expr_type) *out_expr_type = ast_type_clone(&expr_type);
            }

            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);
            ast_type_free(&expr_type);
        }
        break;
    case EXPR_NEW: {
            ir_type_t *ir_type;
            ast_type_t multiplier_type;
            ir_value_t *amount = NULL;
            if(ir_gen_resolve_type(builder->compiler, builder->object, &((ast_expr_new_t*) expr)->type, &ir_type)) return FAILURE;

            if( ((ast_expr_new_t*) expr)->amount != NULL ){
                if(ir_gen_expression(builder, ((ast_expr_new_t*) expr)->amount, &amount, false, &multiplier_type)) return FAILURE;
                unsigned int multiplier_typekind = amount->type->kind;

                if(multiplier_typekind < TYPE_KIND_S8 || multiplier_typekind > TYPE_KIND_U64){
                    char *typename = ast_type_str(&multiplier_type);
                    compiler_panicf(builder->compiler, expr->source, "Can't specify length using non-integer type '%s'", typename);
                    ast_type_free(&multiplier_type);
                    free(typename);
                    return FAILURE;
                }

                ast_type_free(&multiplier_type);
            }

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_malloc_t));
            ((ir_instr_malloc_t*) instruction)->id = INSTRUCTION_MALLOC;
            ((ir_instr_malloc_t*) instruction)->result_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ((ir_instr_malloc_t*) instruction)->result_type->kind = TYPE_KIND_POINTER;
            ((ir_instr_malloc_t*) instruction)->result_type->extra = ir_type;
            ((ir_instr_malloc_t*) instruction)->type = ir_type;
            ((ir_instr_malloc_t*) instruction)->amount = amount;
            ((ir_instr_malloc_t*) instruction)->is_undef = ((ast_expr_new_t*) expr)->is_undef;

            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);

            if(out_expr_type != NULL){
                *out_expr_type = ast_type_clone(&((ast_expr_new_t*) expr)->type);
                ast_type_prepend_ptr(out_expr_type);
            }
        }
        break;
    case EXPR_NEW_CSTRING: {
            ast_expr_new_cstring_t *new_cstring_expr = (ast_expr_new_cstring_t*) expr;
            
            ir_type_t *ubyte = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ubyte->kind = TYPE_KIND_U8;

            ir_type_t *ubyte_ptr = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ubyte_ptr->kind = TYPE_KIND_POINTER;
            ubyte_ptr->extra = ubyte;

            length_t value_length = strlen(new_cstring_expr->value);

            ir_value_t *bytes_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
            bytes_value->value_type = VALUE_TYPE_LITERAL;
            bytes_value->type = ir_builder_usize(builder);
            bytes_value->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
            *((unsigned long long*) bytes_value->extra) = value_length + 1;

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_malloc_t));
            ((ir_instr_malloc_t*) instruction)->id = INSTRUCTION_MALLOC;
            ((ir_instr_malloc_t*) instruction)->result_type = ubyte_ptr;
            ((ir_instr_malloc_t*) instruction)->type = ubyte;
            ((ir_instr_malloc_t*) instruction)->amount = bytes_value;

            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            ir_value_t *heap_memory = build_value_from_prev_instruction(builder);

            ir_value_t *cstring_value = build_literal_cstr(builder, new_cstring_expr->value);

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_memcpy_t));
            ((ir_instr_memcpy_t*) instruction)->id = INSTRUCTION_MEMCPY;
            ((ir_instr_memcpy_t*) instruction)->result_type = NULL;
            ((ir_instr_memcpy_t*) instruction)->destination = heap_memory;
            ((ir_instr_memcpy_t*) instruction)->value = cstring_value;
            ((ir_instr_memcpy_t*) instruction)->bytes = bytes_value;
            ((ir_instr_memcpy_t*) instruction)->is_volatile = false;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;

            *ir_value = heap_memory;
            if(out_expr_type != NULL){
                ast_type_make_base_ptr(out_expr_type, strclone("ubyte"));
            }
        }
        break;
    case EXPR_ENUM_VALUE: {
            ast_expr_enum_value_t *enum_value_expr = (ast_expr_enum_value_t*) expr;
            length_t enum_kind_id;

            maybe_index_t enum_index = find_enum(builder->object->ast.enums, builder->object->ast.enums_length, enum_value_expr->enum_name);

            if(enum_index == -1){
                compiler_panicf(builder->compiler, enum_value_expr->source, "Failed to enum '%s'", enum_value_expr->enum_name);
                return FAILURE;
            }

            ast_enum_t *inum = &builder->object->ast.enums[enum_index];

            if(!ast_enum_find_kind(inum, enum_value_expr->kind_name, &enum_kind_id)){
                compiler_panicf(builder->compiler, enum_value_expr->source, "Failed to find member '%s' of enum '%s'", enum_value_expr->kind_name, enum_value_expr->enum_name);
                return FAILURE;
            }

            *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
            (*ir_value)->value_type = VALUE_TYPE_LITERAL;
            ir_type_map_find(builder->type_map, enum_value_expr->enum_name, &((*ir_value)->type));
            (*ir_value)->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
            *((unsigned long long*) (*ir_value)->extra) = enum_kind_id;

            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone(enum_value_expr->enum_name));
        }
        break;
    case EXPR_BIT_AND:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_AND, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'and' on those types"); break;
    case EXPR_BIT_OR:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_OR, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'or' on those types"); break;
    case EXPR_BIT_XOR:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_XOR, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'or' on those types"); break;
    case EXPR_BIT_LSHIFT:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'left shift' on those types"); break;
    case EXPR_BIT_RSHIFT:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_RSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'right shift' on those types"); break;
    case EXPR_BIT_LGC_LSHIFT:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'left shift' on those types"); break;
    case EXPR_BIT_LGC_RSHIFT:
        BUILD_MATH_OP_IvF_MACRO(INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'right shift' on those types"); break;
    case EXPR_STATIC_ARRAY: {
            ast_expr_static_data_t *static_array_expr = (ast_expr_static_data_t*) expr;

            ir_type_t *array_ir_type;
            if(ir_gen_resolve_type(builder->compiler, builder->object, &static_array_expr->type, &array_ir_type)){
                return FAILURE;
            }

            ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * static_array_expr->length);
            length_t length = static_array_expr->length;
            ir_type_t *type = array_ir_type;

            for(length_t i = 0; i != static_array_expr->length; i++){
                ast_type_t member_type;

                if(ir_gen_expression(builder, static_array_expr->values[i], &values[i], false, &member_type)){
                    return FAILURE;
                }

                if(!ast_types_conform(builder, &values[i], &member_type, &static_array_expr->type, CONFORM_MODE_STANDARD)){
                    char *a_type_str = ast_type_str(&member_type);
                    char *b_type_str = ast_type_str(&static_array_expr->type);
                    compiler_panicf(builder->compiler, static_array_expr->type.source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&member_type);
                    return FAILURE;
                }

                if(!VALUE_TYPE_IS_CONSTANT(values[i]->value_type)){
                    compiler_panicf(builder->compiler, static_array_expr->values[i]->source, "Can't put non-constant value into constant array");
                    ast_type_free(&member_type);
                    return FAILURE;
                }

                ast_type_free(&member_type);
            }

            *ir_value = build_static_array(builder->pool, type, values, length);

            if(out_expr_type != NULL){
                *out_expr_type = ast_type_clone(&static_array_expr->type);
                ast_type_prepend_ptr(out_expr_type);
            }
        }
        break;
    case EXPR_STATIC_STRUCT: {
            // NOTE: Assumes a structure names that exists, cause we already checked in 'infer'
            // NOTE: Also assumes that lengths of the struct literal and the struct match up (because we already checked)

            ast_expr_static_data_t *static_struct_expr = (ast_expr_static_data_t*) expr;

            ir_type_t *struct_ir_type;
            if(ir_gen_resolve_type(builder->compiler, builder->object, &static_struct_expr->type, &struct_ir_type)){
                return FAILURE;
            }

            const char *base = ((ast_elem_base_t*) static_struct_expr->type.elements[0])->base;
            ast_struct_t *structure = ast_struct_find(&builder->object->ast, base);

            ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * static_struct_expr->length);
            length_t length = static_struct_expr->length;
            ir_type_t *type = struct_ir_type;

            for(length_t i = 0; i != static_struct_expr->length; i++){
                ast_type_t member_type;

                if(ir_gen_expression(builder, static_struct_expr->values[i], &values[i], false, &member_type)){
                    return FAILURE;
                }

                if(!ast_types_conform(builder, &values[i], &member_type, &structure->field_types[i], CONFORM_MODE_STANDARD)){
                    char *a_type_str = ast_type_str(&member_type);
                    char *b_type_str = ast_type_str(&static_struct_expr->type);
                    compiler_panicf(builder->compiler, static_struct_expr->values[i]->source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&member_type);
                    return FAILURE;
                }

                if(!VALUE_TYPE_IS_CONSTANT(values[i]->value_type)){
                    compiler_panicf(builder->compiler, static_struct_expr->values[i]->source, "Can't put non-constant value into constant array");
                    ast_type_free(&member_type);
                    return FAILURE;
                }

                ast_type_free(&member_type);
            }

            *ir_value = build_static_struct(&builder->object->ir_module, type, values, length, true);

            if(out_expr_type != NULL){
                *out_expr_type = ast_type_clone(&static_struct_expr->type);
                ast_type_prepend_ptr(out_expr_type);
            }
        }
        break;
    case EXPR_TYPEINFO: {
            ast_expr_typeinfo_t *typeinfo = (ast_expr_typeinfo_t*) expr;
            
            if(builder->compiler->traits & COMPILER_NO_TYPE_INFO){
                compiler_panic(builder->compiler, typeinfo->source, "Unable to use runtime type info when runtime type information is disabled");
                return FAILURE;
            }

            *ir_value = rtti_for(builder, &typeinfo->target, typeinfo->source);
            if(*ir_value == NULL) return FAILURE;

            if(out_expr_type)
                ast_type_make_base_ptr(out_expr_type, strclone("AnyType"));
        }
        break;
    case EXPR_TERNARY: {
            ast_expr_ternary_t *ternary = (ast_expr_ternary_t*) expr;

            ir_value_t *condition, *if_true, *if_false;
            ast_type_t condition_type, if_true_type, if_false_type;

            if(ir_gen_expression(builder, ternary->condition, &condition, false, &condition_type))
                return FAILURE;

            if(!ast_types_conform(builder, &condition, &condition_type, &builder->static_bool, CONFORM_MODE_CALCULATION)){
                char *condition_type_str = ast_type_str(&condition_type);
                compiler_panicf(builder->compiler, ternary->condition->source, "Received type '%s' when conditional expects type 'bool'", condition_type_str);
                free(condition_type_str);
                ast_type_free(&condition_type);
                return FAILURE;
            }

            ast_type_free(&condition_type);

            if(ir_gen_expression(builder, ternary->if_true, &if_true, false, &if_true_type))
                return FAILURE;

            if(ir_gen_expression(builder, ternary->if_false, &if_false, false, &if_false_type)){
                ast_type_free(&if_true_type);
                return FAILURE;
            }

            if(!ast_types_identical(&if_true_type, &if_false_type)){
                char *if_true_typename = ast_type_str(&if_true_type);
                char *if_false_typename = ast_type_str(&if_false_type);
                compiler_panicf(builder->compiler, ternary->source, "ternary operator could result in different types '%s' and '%s'", if_true_typename, if_false_typename);
                ast_type_free(&if_true_type);
                ast_type_free(&if_false_type);
                free(if_true_typename);
                free(if_false_typename);
                return FAILURE;
            }

            ir_type_t *result_ir_type;
            if(ir_gen_resolve_type(builder->compiler, builder->object, &if_true_type, &result_ir_type)){
                ast_type_free(&if_true_type);
                ast_type_free(&if_false_type);
                return FAILURE;
            }

            if(out_expr_type){
                *out_expr_type = if_true_type;
            } else {
                ast_type_free(&if_true_type);
            }

            ast_type_free(&if_false_type);

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_select_t));
            ((ir_instr_select_t*) instruction)->id = INSTRUCTION_SELECT;
            ((ir_instr_select_t*) instruction)->result_type = result_ir_type;
            ((ir_instr_select_t*) instruction)->condition = condition;
            ((ir_instr_select_t*) instruction)->if_true = if_true;
            ((ir_instr_select_t*) instruction)->if_false = if_false;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            *ir_value = build_value_from_prev_instruction(builder);
        }
        break;
    case EXPR_PREINCREMENT: case EXPR_PREDECREMENT:
    case EXPR_POSTINCREMENT: case EXPR_POSTDECREMENT: {
            ir_value_t *before_value;
            ast_type_t before_ast_type;
            ast_expr_unary_t *unary = (ast_expr_unary_t*) expr;

            // NOTE: unary->value is guaranteed to be a mutable expression
            if(ir_gen_expression(builder, unary->value, &before_value, true, &before_ast_type))
                return FAILURE;
            
            if(before_value->type->kind != TYPE_KIND_POINTER){
                compiler_panic(builder->compiler, unary->value->source, "INTERNAL ERROR: ir_gen_expr() EXPR_xCREMENT expected mutable value");
                ast_type_free(&before_ast_type);
                return FAILURE;
            }

            ir_value_t *one = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
            one->value_type = VALUE_TYPE_LITERAL;
            one->type = (ir_type_t*) before_value->type->extra;
            one->extra = NULL;
            
            switch(one->type->kind){
            case TYPE_KIND_S8:
                one->extra = ir_pool_alloc(builder->pool, sizeof(char));
                *((char*) one->extra) = 1;
                break;
            case TYPE_KIND_U8:
                one->extra = ir_pool_alloc(builder->pool, sizeof(unsigned char));
                *((unsigned char*) one->extra) = 1;
                break;
            case TYPE_KIND_S16:
                // stored w/ extra precision
                one->extra = ir_pool_alloc(builder->pool, sizeof(int));
                *((int*) one->extra) = 1;
                break;
            case TYPE_KIND_U16:
                // stored w/ extra precision
                one->extra = ir_pool_alloc(builder->pool, sizeof(unsigned int));
                *((unsigned int*) one->extra) = 1;
                break;
            case TYPE_KIND_S32:
                one->extra = ir_pool_alloc(builder->pool, sizeof(long long));
                *((long long*) one->extra) = 1;
                break;
            case TYPE_KIND_U32:
                one->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long long));
                *((unsigned long long*) one->extra) = 1;
                break;
            case TYPE_KIND_S64:
                one->extra = ir_pool_alloc(builder->pool, sizeof(long long));
                *((long long*) one->extra) = 1;
                break;
            case TYPE_KIND_U64:
                one->extra = ir_pool_alloc(builder->pool, sizeof(unsigned long));
                *((unsigned long long*) one->extra) = 1;
                break;
            }

            if(one->extra == NULL){
                char *typename = ast_type_str(&before_ast_type);
                compiler_panicf(builder->compiler, unary->source, "Can't %s type '%s'",
                    expr->id == EXPR_PREINCREMENT || expr->id == EXPR_POSTINCREMENT ? "increment" : "decrement", typename);
                ast_type_free(&before_ast_type);
                free(typename);
                return FAILURE;
            }

            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_math_t));

            ((ir_instr_math_t*) instruction)->id = INSTRUCTION_NONE; // Will be determined
            ((ir_instr_math_t*) instruction)->a = build_load(builder, before_value);
            ((ir_instr_math_t*) instruction)->b = one;
            ((ir_instr_math_t*) instruction)->result_type = before_value->type; 

            if(expr->id == EXPR_PREINCREMENT || expr->id == EXPR_POSTINCREMENT){
                if(i_vs_f_instruction((ir_instr_math_t*) instruction, INSTRUCTION_ADD, INSTRUCTION_FADD)){
                    char *typename = ast_type_str(&before_ast_type);
                    compiler_panicf(builder->compiler, unary->source, "Can't %s type '%s'", "increment", typename);
                    ast_type_free(&before_ast_type);
                    free(typename);
                    return FAILURE;
                }
            } else {
                if(i_vs_f_instruction((ir_instr_math_t*) instruction, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT)){
                    char *typename = ast_type_str(&before_ast_type);
                    compiler_panicf(builder->compiler, unary->source, "Can't %s type '%s'", "decrement", typename);
                    ast_type_free(&before_ast_type);
                    free(typename);
                    return FAILURE;
                }
            }

            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
            ir_value_t *modified = build_value_from_prev_instruction(builder);
            build_store(builder, modified, before_value);

            if(out_expr_type){
                *out_expr_type = before_ast_type;
            } else {
                ast_type_free(&before_ast_type);
            }

            if(expr->id == EXPR_PREINCREMENT || expr->id == EXPR_PREDECREMENT){
                *ir_value = leave_mutable ? before_value : build_load(builder, before_value);
            } else {
                *ir_value = leave_mutable ? before_value : ((ir_instr_math_t*) instruction)->a;
            }
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = ((ast_expr_inline_declare_t*) expr);

            // Search for existing variable named that
            if(bridge_var_scope_already_in_list(builder->var_scope, def->name)){
                compiler_panicf(builder->compiler, def->source, "Variable '%s' already declared", def->name);
                return FAILURE;
            }

            ir_type_t *ir_decl_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            if(ir_gen_resolve_type(builder->compiler, builder->object, &def->type, &ir_decl_type)) return FAILURE;

            ir_type_t *var_pointer_type = ir_type_pointer_to(builder->pool, ir_decl_type);

            if(def->value != NULL){
                // Regular inline declare statement initial assign value
                ir_value_t *initial;
                ast_type_t temporary_type;
                if(ir_gen_expression(builder, def->value, &initial, false, &temporary_type)) return FAILURE;

                if(!ast_types_conform(builder, &initial, &temporary_type, &def->type, CONFORM_MODE_ASSIGNING)){
                    char *a_type_str = ast_type_str(&temporary_type);
                    char *b_type_str = ast_type_str(&def->type);
                    compiler_panicf(builder->compiler, def->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
                    free(a_type_str);
                    free(b_type_str);
                    ast_type_free(&temporary_type);
                    return FAILURE;
                }


                ir_value_t *destination = build_varptr(builder, var_pointer_type, builder->next_var_id);
                add_variable(builder, def->name, &def->type, ir_decl_type, def->is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

                if(def->is_assign_pod || !handle_assign_management(builder, initial, &temporary_type, destination, &def->type, true)){
                    build_store(builder, initial, destination);
                }

                ast_type_free(&temporary_type);
                *ir_value = destination;
            } else if(def->id == EXPR_ILDECLAREUNDEF && !(builder->compiler->traits & COMPILER_NO_UNDEF)){
                // Mark the variable as undefined memory so it isn't auto-initialized later on
                add_variable(builder, def->name, &def->type, ir_decl_type, def->is_pod ? BRIDGE_VAR_UNDEF | BRIDGE_VAR_POD : BRIDGE_VAR_UNDEF);

                *ir_value = build_varptr(builder, var_pointer_type, builder->next_var_id - 1);
            } else /* plain ILDECLARE or --no-undef ILDECLAREUNDEF */ {
                // Variable declaration without default value
                add_variable(builder, def->name, &def->type, ir_decl_type, def->is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

                // Zero initialize the variable
                instruction = build_instruction(builder, sizeof(ir_instr_varzeroinit_t));
                ((ir_instr_varzeroinit_t*) instruction)->id = INSTRUCTION_VARZEROINIT;
                ((ir_instr_varzeroinit_t*) instruction)->result_type = NULL;
                ((ir_instr_varzeroinit_t*) instruction)->index = builder->next_var_id - 1;
                
                *ir_value = build_varptr(builder, var_pointer_type, builder->next_var_id - 1);
            }

            if(out_expr_type){
                ast_type_t ast_pointer = ast_type_clone(&def->type);
                ast_type_prepend_ptr(&ast_pointer);
                *out_expr_type = ast_pointer;
            }
        }
        break;
    default:
        compiler_panic(builder->compiler, expr->source, "Unknown expression type id in expression");
        return FAILURE;
    }

    #undef BUILD_LITERAL_IR_VALUE
    #undef BUILD_MATH_OP_IvF_MACRO
    #undef BUILD_MATH_OP_UvSvF_MACRO

    return SUCCESS;
}

errorcode_t differentiate_math_operation(ir_builder_t *builder, ast_expr_math_t *math_expr, ir_value_t **ir_value, ast_type_t *out_expr_type,
        unsigned int instr1, unsigned int instr2, unsigned int instr3, const char *op_verb, const char *overload, bool result_is_boolean){

    // NOTE: If instr3 is INSTRUCTION_NONE then the operation will be differentiated for Integer vs. Float (using instr1 and instr2)
    //       Otherwise, the operation will be differentiated for Unsigned Integer vs. Signed Integer vs. Float (using instr1, instr2 & instr3)

    ir_pool_snapshot_t tmp_pool_snapshot;
    ast_type_t ast_type_a, ast_type_b;
    ir_value_t *lhs, *rhs;

    if(ir_gen_expression(builder, math_expr->a, &lhs, false, &ast_type_a)) return FAILURE;
    if(ir_gen_expression(builder, math_expr->b, &rhs, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        return FAILURE;
    }

    if(!ast_types_conform(builder, &rhs, &ast_type_b, &ast_type_a, CONFORM_MODE_CALCULATION)){
        if(overload){
            *ir_value = handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, out_expr_type, overload);

            if(*ir_value != NULL){
                ast_type_free(&ast_type_a);
                ast_type_free(&ast_type_b);
                return SUCCESS;
            }
        }

        char *a_type_str = ast_type_str(&ast_type_a);
        char *b_type_str = ast_type_str(&ast_type_b);
        compiler_panicf(builder->compiler, math_expr->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
        return FAILURE;
    }

    // Add math instruction template
    ir_pool_snapshot_capture(builder->pool, &tmp_pool_snapshot);
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    instruction->a = lhs;
    instruction->b = rhs;
    instruction->id = INSTRUCTION_NONE; // For safety
    instruction->result_type = lhs->type;

    if(instr3 == INSTRUCTION_NONE
            ? i_vs_f_instruction((ir_instr_math_t*) instruction, instr1, instr2) == FAILURE
            : u_vs_s_vs_float_instruction((ir_instr_math_t*) instruction, instr1, instr2, instr3) == FAILURE
    ){
        // Remove math instruction template
        ir_pool_snapshot_restore(builder->pool, &tmp_pool_snapshot);
        builder->current_block->instructions_length--;

        *ir_value = handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, out_expr_type, overload);
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);

        if(*ir_value == NULL){
            compiler_panicf(builder->compiler, math_expr->source, "Can't %s those types", op_verb);
            return FAILURE;
        }

        return SUCCESS;
    }

    *ir_value = build_value_from_prev_instruction(builder);
    if(out_expr_type != NULL){
        if(result_is_boolean) ast_type_make_base(out_expr_type, strclone("bool"));
        else                  *out_expr_type = ast_type_clone(&ast_type_a);
    }

    ast_type_free(&ast_type_a);
    ast_type_free(&ast_type_b);
    return SUCCESS;
}

ir_instr_t* ir_gen_math_operands(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, unsigned int op_res, ast_type_t *out_expr_type){
    // ir_gen the operends of an expression into instructions and gives back an instruction with unknown id

    // NOTE: Returns the generated instruction
    // NOTE: The instruction id still must be specified after this call
    // NOTE: Returns NULL if an error occured
    // NOTE:'op_res' should be either MATH_OP_RESULT_MATCH, MATH_OP_RESULT_BOOL or MATH_OP_ALL_BOOL

    ir_value_t *a, *b;
    ir_instr_t **instruction;
    ast_type_t ast_type_a, ast_type_b;
    *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    (*ir_value)->value_type = VALUE_TYPE_RESULT;

    if(ir_gen_expression(builder, ((ast_expr_math_t*) expr)->a, &a, false, &ast_type_a)) return NULL;
    if(ir_gen_expression(builder, ((ast_expr_math_t*) expr)->b, &b, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        return NULL;
    }

    if(op_res == MATH_OP_ALL_BOOL){
        // Conform expression 'a' to type 'bool' to ensure 'b' will also have to conform
        // Use 'out_expr_type' to store bool type (will stay there anyways cause resulting type is a bool)
        ast_type_make_base(out_expr_type, strclone("bool"));

        if(!ast_types_identical(&ast_type_a, out_expr_type) && !ast_types_conform(builder, &a, &ast_type_a, out_expr_type, CONFORM_MODE_CALCULATION)){
            char *a_type_str = ast_type_str(&ast_type_a);
            compiler_panicf(builder->compiler, expr->source, "Failed to convert value of type '%s' to type 'bool'", a_type_str);
            free(a_type_str);
            ast_type_free(&ast_type_a);
            ast_type_free(&ast_type_b);
            ast_type_free(out_expr_type);
            return NULL;
        }

        if(!ast_types_identical(&ast_type_b, out_expr_type) && !ast_types_conform(builder, &b, &ast_type_b, out_expr_type, CONFORM_MODE_CALCULATION)){
            char *b_type_str = ast_type_str(&ast_type_b);
            compiler_panicf(builder->compiler, expr->source, "Failed to convert value of type '%s' to type 'bool'", b_type_str);
            free(b_type_str);
            ast_type_free(&ast_type_a);
            ast_type_free(&ast_type_b);
            ast_type_free(out_expr_type);
            return NULL;
        }

        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
        ast_type_a = *out_expr_type;
        ast_type_b = *out_expr_type;
    }

    if(!ast_types_conform(builder, &b, &ast_type_b, &ast_type_a, CONFORM_MODE_CALCULATION)){
        char *a_type_str = ast_type_str(&ast_type_a);
        char *b_type_str = ast_type_str(&ast_type_b);
        compiler_panicf(builder->compiler, expr->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);

        if(op_res != MATH_OP_ALL_BOOL){
            ast_type_free(&ast_type_a);
            ast_type_free(&ast_type_b);
        } else {
            ast_type_free(out_expr_type);
        }

        return NULL;
    }

    (*ir_value)->extra = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
    ((ir_value_result_t*) (*ir_value)->extra)->block_id = builder->current_block_id;
    ((ir_value_result_t*) (*ir_value)->extra)->instruction_id = builder->current_block->instructions_length;

    ir_basicblock_new_instructions(builder->current_block, 1);
    instruction = &builder->current_block->instructions[builder->current_block->instructions_length++];
    *instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_math_t));
    ((ir_instr_math_t*) *instruction)->a = a;
    ((ir_instr_math_t*) *instruction)->b = b;
    ((ir_instr_math_t*) *instruction)->id = INSTRUCTION_NONE; // For safety

    switch(op_res){
    case MATH_OP_RESULT_MATCH:
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&ast_type_a);
        ((ir_instr_math_t*) *instruction)->result_type = a->type;
        (*ir_value)->type = a->type;
        break;
    case MATH_OP_RESULT_BOOL:
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("bool"));
        // Fall through to MATH_OP_ALL_BOOL
    case MATH_OP_ALL_BOOL:
        ((ir_instr_math_t*) *instruction)->result_type = (ir_type_t*) ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        ((ir_instr_math_t*) *instruction)->result_type->kind = TYPE_KIND_BOOLEAN;
        (*ir_value)->type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*ir_value)->type->kind = TYPE_KIND_BOOLEAN;
        // Assigning result_type->extra to null is unnecessary
        break;
    default:
        compiler_panic(builder->compiler, expr->source, "ir_gen_math_operands() got invalid op_res value");
        if(op_res != MATH_OP_ALL_BOOL){
            ast_type_free(&ast_type_a);
            ast_type_free(&ast_type_b);
        } else {
            ast_type_free(out_expr_type);
        }
        return NULL;
    }

    if(op_res != MATH_OP_ALL_BOOL){
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
    }

    return *instruction;
}

errorcode_t i_vs_f_instruction(ir_instr_math_t *instruction, unsigned int i_instr, unsigned int f_instr){
    // NOTE: Sets the instruction id to 'i_instr' if operating on intergers
    //       Sets the instruction id to 'f_instr' if operating on floats
    // NOTE: If target instruction id is INSTRUCTION_NONE, then 1 is returned
    // NOTE: Returns 1 if unsupported type

    switch(instruction->a->type->kind){
    case TYPE_KIND_POINTER: case TYPE_KIND_BOOLEAN: case TYPE_KIND_FUNCPTR:
    case TYPE_KIND_U8: case TYPE_KIND_U16: case TYPE_KIND_U32: case TYPE_KIND_U64:
    case TYPE_KIND_S8: case TYPE_KIND_S16: case TYPE_KIND_S32: case TYPE_KIND_S64:
        if(i_instr == INSTRUCTION_NONE) return FAILURE;
        instruction->id = i_instr; break;
    case TYPE_KIND_HALF: case TYPE_KIND_FLOAT: case TYPE_KIND_DOUBLE:
        if(f_instr == INSTRUCTION_NONE) return FAILURE;
        instruction->id = f_instr; break;
    default: return FAILURE;
    }
    return SUCCESS;
}

errorcode_t u_vs_s_vs_float_instruction(ir_instr_math_t *instruction, unsigned int u_instr, unsigned int s_instr, unsigned int f_instr){
    // NOTE: Sets the instruction id to 'u_instr' if operating on unsigned intergers
    //       Sets the instruction id to 's_instr' if operating on signed intergers
    //       Sets the instruction id to 'f_instr' if operating on floats
    // NOTE: If target instruction id is INSTRUCTION_NONE, then 1 is returned
    // NOTE: Returns 1 if unsupported type

    switch(instruction->a->type->kind){
    case TYPE_KIND_POINTER: case TYPE_KIND_BOOLEAN: case TYPE_KIND_FUNCPTR:
    case TYPE_KIND_U8: case TYPE_KIND_U16: case TYPE_KIND_U32: case TYPE_KIND_U64:
        if(u_instr == INSTRUCTION_NONE) return FAILURE;
        instruction->id = u_instr; break;
    case TYPE_KIND_S8: case TYPE_KIND_S16: case TYPE_KIND_S32: case TYPE_KIND_S64:
        if(s_instr == INSTRUCTION_NONE) return FAILURE;
        instruction->id = s_instr; break;
    case TYPE_KIND_HALF: case TYPE_KIND_FLOAT: case TYPE_KIND_DOUBLE:
        if(f_instr == INSTRUCTION_NONE) return FAILURE;
        instruction->id = f_instr; break;
    default: return FAILURE;
    }
    return SUCCESS;
}

char ir_type_get_catagory(ir_type_t *type){
    switch(type->kind){
    case TYPE_KIND_POINTER: case TYPE_KIND_BOOLEAN: case TYPE_KIND_FUNCPTR:
    case TYPE_KIND_U8: case TYPE_KIND_U16: case TYPE_KIND_U32: case TYPE_KIND_U64:
        return PRIMITIVE_UI;
    case TYPE_KIND_S8: case TYPE_KIND_S16: case TYPE_KIND_S32: case TYPE_KIND_S64:
        return PRIMITIVE_SI;
    case TYPE_KIND_HALF: case TYPE_KIND_FLOAT: case TYPE_KIND_DOUBLE:
        return PRIMITIVE_FP;
    }
    return PRIMITIVE_NA; // No catagory
}
