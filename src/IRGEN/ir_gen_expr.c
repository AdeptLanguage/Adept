
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

errorcode_t ir_gen_expr(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    // NOTE: Generates an ir_value_t from an ast_expr_t
    // NOTE: Will write determined ast_type_t to 'out_expr_type' (Can use NULL to ignore)

    ir_instr_t *instruction;
    ir_value_t *dummy_temporary_ir_value;

    if(ir_value == NULL){
        // If it's a statement (it doesn't care about the resulting value), then
        // use a dummy for usage for ir value
        ir_value = &dummy_temporary_ir_value;
        leave_mutable = true;
    }

    switch(expr->id){

    #define build_literal_ir_value(ast_expr_type, typename, storage_type) {              \
        /* Allocate memory for literal value */                                          \
        *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));                    \
        (*ir_value)->value_type = VALUE_TYPE_LITERAL;                                    \
                                                                                         \
        /* Store the literal value and resolve the IR type */                            \
        ir_type_map_find(builder->type_map, typename, &((*ir_value)->type));             \
        (*ir_value)->extra = ir_pool_alloc(builder->pool, sizeof(storage_type));         \
        *((storage_type*) (*ir_value)->extra) = ((ast_expr_type*) expr)->value;          \
                                                                                         \
        /* Result type is an AST type with that typename */                              \
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone(typename)); \
    }

    case EXPR_BYTE:
        build_literal_ir_value(ast_expr_byte_t, "byte", adept_byte);
        break;
    case EXPR_UBYTE:
        build_literal_ir_value(ast_expr_ubyte_t, "ubyte", adept_ubyte);
        break;
    case EXPR_SHORT:
        build_literal_ir_value(ast_expr_short_t, "short", adept_short);
        break;
    case EXPR_USHORT:
        build_literal_ir_value(ast_expr_ushort_t, "ushort", adept_ushort);
        break;
    case EXPR_INT:
        build_literal_ir_value(ast_expr_int_t, "int", adept_int);
        break;
    case EXPR_UINT:
        build_literal_ir_value(ast_expr_uint_t, "uint", adept_uint);
        break;
    case EXPR_LONG:
        build_literal_ir_value(ast_expr_long_t, "long", adept_long);
        break;
    case EXPR_ULONG:
        build_literal_ir_value(ast_expr_ulong_t, "ulong", adept_ulong);
        break;
    case EXPR_USIZE:
        build_literal_ir_value(ast_expr_usize_t, "usize", adept_usize);
        break;
    case EXPR_FLOAT:
        build_literal_ir_value(ast_expr_float_t, "float", adept_float);
        break;
    case EXPR_DOUBLE:
        build_literal_ir_value(ast_expr_double_t, "double", adept_double);
        break;
    case EXPR_BOOLEAN:
        build_literal_ir_value(ast_expr_boolean_t, "bool", adept_bool);
        break;

    #undef build_literal_ir_value

    #define build_math_ivf_operation(ints_instr, floats_instr, op_result_kind, error_message) {     \
        /* Create undesignated math operation */                                                    \
        instruction = ir_gen_math_operands(builder, expr, ir_value, op_result_kind, out_expr_type); \
        if(instruction == NULL) return FAILURE;                                                     \
                                                                                                    \
        /* Designated math operation based on operand types */                                      \
        if(i_vs_f_instruction((ir_instr_math_t*) instruction, ints_instr, floats_instr)){           \
            compiler_panic(builder->compiler, ((ast_expr_math_t*) expr)->source, error_message);    \
            ast_type_free(out_expr_type);                                                           \
            return FAILURE;                                                                         \
        }                                                                                           \
    }
    
    case EXPR_BIT_AND:
        build_math_ivf_operation(INSTRUCTION_BIT_AND, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'and' on those types");
        break;
    case EXPR_BIT_OR:
        build_math_ivf_operation(INSTRUCTION_BIT_OR, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'or' on those types");
        break;
    case EXPR_BIT_XOR:
        build_math_ivf_operation(INSTRUCTION_BIT_XOR, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'xor' on those types");
        break;
    case EXPR_BIT_LSHIFT:
        build_math_ivf_operation(INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'left shift' on those types");
        break;
    case EXPR_BIT_LGC_LSHIFT:
        build_math_ivf_operation(INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'left shift' on those types");
        break;
    case EXPR_BIT_LGC_RSHIFT:
        build_math_ivf_operation(INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'right shift' on those types");
        break;

    #undef build_math_ivf_operation
    
    #define build_math_uvsvf_operation(uints_instr, sints_instr, floats_instr, op_result_kind, error_message) {  \
        /* Create undesignated math operation */                                                                 \
        instruction = ir_gen_math_operands(builder, expr, ir_value, op_result_kind, out_expr_type);              \
        if(instruction == NULL) return FAILURE;                                                                  \
                                                                                                                 \
        /* Designated math operation based on operand types */                                                   \
        if(u_vs_s_vs_float_instruction((ir_instr_math_t*) instruction, uints_instr, sints_instr, floats_instr)){ \
            compiler_panic(builder->compiler, ((ast_expr_math_t*) expr)->source, error_message);                 \
            ast_type_free(out_expr_type);                                                                        \
            return FAILURE;                                                                                      \
        }                                                                                                        \
    }

    case EXPR_BIT_RSHIFT:
        build_math_uvsvf_operation(INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_BIT_RSHIFT, INSTRUCTION_NONE, MATH_OP_RESULT_MATCH, "Can't perform bitwise 'right shift' on those types");
        break;

    #undef build_math_uvsvf_operation
    
    case EXPR_NULL:
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("ptr"));
        *ir_value = build_null_pointer(builder->pool);
        break;
    case EXPR_ADD:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ADD, INSTRUCTION_FADD, INSTRUCTION_NONE, "add", "__add__", false))
            return FAILURE;
        break;
    case EXPR_SUBTRACT:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT, INSTRUCTION_NONE, "subtract", "__subtract__", false))
            return FAILURE;
        break;
    case EXPR_MULTIPLY:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_MULTIPLY, INSTRUCTION_FMULTIPLY, INSTRUCTION_NONE, "multiply", "__multiply__", false))
            return FAILURE;
        break;
    case EXPR_DIVIDE:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UDIVIDE, INSTRUCTION_SDIVIDE, INSTRUCTION_FDIVIDE, "divide", "__divide__", false))
            return FAILURE;
        break;
    case EXPR_MODULUS:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UMODULUS, INSTRUCTION_SMODULUS, INSTRUCTION_FMODULUS, "take the modulus of", "__modulus__", false))
            return FAILURE;
        break;
    case EXPR_EQUALS:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_EQUALS, INSTRUCTION_FEQUALS, INSTRUCTION_NONE, "test equality for", "__equals__", true))
            return FAILURE;
        break;
    case EXPR_NOTEQUALS:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_NOTEQUALS, INSTRUCTION_FNOTEQUALS, INSTRUCTION_NONE, "test inequality for", "__not_equals__", true))
            return FAILURE;
        break;
    case EXPR_GREATER:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UGREATER, INSTRUCTION_SGREATER, INSTRUCTION_FGREATER, "compare", "__greater_than__", true))
            return FAILURE;
        break;
    case EXPR_LESSER:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ULESSER, INSTRUCTION_SLESSER, INSTRUCTION_FLESSER, "compare", "__less_than__", true))
            return FAILURE;
        break;
    case EXPR_GREATEREQ:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_UGREATEREQ, INSTRUCTION_SGREATEREQ, INSTRUCTION_FGREATEREQ, "compare", "__greater_than_or_equal__", true))
            return FAILURE;
        break;
    case EXPR_LESSEREQ:
        if(ir_gen_expr_math(builder, (ast_expr_math_t*) expr, ir_value, out_expr_type, INSTRUCTION_ULESSEREQ, INSTRUCTION_SLESSEREQ, INSTRUCTION_FLESSEREQ, "compare", "__less_than_or_equal__", true))
            return FAILURE;
        break;
    case EXPR_AND:
        if(ir_gen_expr_and(builder, (ast_expr_and_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_OR:
        if(ir_gen_expr_or(builder, (ast_expr_or_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_STR:
        if(ir_gen_expr_str(builder, (ast_expr_str_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_CSTR:
        if(ir_gen_expr_cstr(builder, (ast_expr_cstr_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_VARIABLE:
        if(ir_gen_expr_variable(builder, (ast_expr_variable_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_CALL:
        if(ir_gen_expr_call(builder, (ast_expr_call_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(ir_gen_expr_member(builder, (ast_expr_member_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_ADDRESS:
        if(ir_gen_expr_address(builder, (ast_expr_address_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_VA_ARG:
        if(ir_gen_expr_va_arg(builder, (ast_expr_va_arg_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR:
        if(ir_gen_expr_func_addr(builder, (ast_expr_func_addr_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_DEREFERENCE:
        if(ir_gen_expr_dereference(builder, (ast_expr_address_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_ARRAY_ACCESS: case EXPR_AT:
        if(ir_gen_expr_array_access(builder, (ast_expr_array_access_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_CAST:
        if(ir_gen_expr_cast(builder, (ast_expr_cast_t*) expr, ir_value, out_expr_type)) return FAILURE;
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
    case EXPR_PHANTOM: {
            *ir_value = (ir_value_t*) ((ast_expr_phantom_t*) expr)->ir_value;
            if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&((ast_expr_phantom_t*) expr)->type);
        }
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_expr = (ast_expr_call_method_t*) expr;
            ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (call_expr->arity + 1));
            ast_type_t *arg_types = malloc(sizeof(ast_type_t) * (call_expr->arity + 1));
            ir_value_t *stack_pointer = NULL;

            // Generate primary argument
            if(ir_gen_expr(builder, call_expr->value, &arg_values[0], true, &arg_types[0])){
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
                        ast_types_free_fully(arg_types, 1);
                        break;
                    }

                    stack_pointer = build_stack_save(builder);
                    ir_value_t *temporary_mutable = build_alloc(builder, arg_values[0]->type);
                    build_store(builder, arg_values[0], temporary_mutable, expr->source);
                    arg_values[0] = temporary_mutable;
                }

                ast_type_prepend_ptr(&arg_types[0]);
            } else if(type_elems_length == 2 && type_elems[0]->id == AST_ELEM_POINTER
                    && (type_elems[1]->id == AST_ELEM_BASE || type_elems[1]->id == AST_ELEM_GENERIC_BASE)){
                // Load the value that's being called on if the expression is mutable
                if(expr_is_mutable(call_expr->value)){
                    arg_values[0] = build_load(builder, arg_values[0], expr->source);
                }
            } else {
                if(call_expr->is_tentative){
                    if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                    ast_types_free_fully(arg_types, 1);
                    break;
                }
                
                char *s = ast_type_str(&arg_types[0]);
                compiler_panicf(builder->compiler, call_expr->source, "Can't call methods on type '%s'", s);
                ast_types_free_fully(arg_types, 1);
                free(s);
                return FAILURE;
            }

            // Generate secondary argument values
            for(length_t a = 0; a != call_expr->arity; a++){
                if(ir_gen_expr(builder, call_expr->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
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

                        compiler_undeclared_method(builder->compiler, builder->object, expr->source, call_expr->name, arg_types, call_expr->arity);
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

                        ast_types_free_fully(arg_types, call_expr->arity + 1);
                        compiler_panic(builder->compiler, generic_base->source, "Can't call method on struct value with unresolved polymorphic name");
                        return FAILURE;
                    }

                    if(ir_gen_find_generic_base_method_conforming(builder, generic_base->name, call_expr->name, arg_values, arg_types, call_expr->arity + 1, &pair)){
                        if(call_expr->is_tentative){
                            tentative_fell_through = true;
                            break;
                        }

                        compiler_undeclared_method(builder->compiler, builder->object, expr->source, call_expr->name, arg_types, call_expr->arity);
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

            // Found function that fits given name and arguments
            length_t arity = call_expr->arity + 1;

            // If the found function has default arguments and we are missing arguments, fill them in
            if(pair.ast_func->arg_defaults && arity != pair.ast_func->arity){
                // ------------------------------------------------
                // | 0 | 1 | 2 |                    Supplied
                // | 0 | 1 | 2 | 3 | 4 |            Required
                // |   | 1 |   | 3 | 4 |            Defaults
                // ------------------------------------------------
                
                ir_value_t **new_args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * pair.ast_func->arity);
                ast_type_t *new_arg_types = malloc(sizeof(ast_type_t) * pair.ast_func->arity);
                ast_expr_t **arg_defaults = pair.ast_func->arg_defaults;

                // Copy given argument values into new array
                memcpy(new_args, arg_values, sizeof(ir_value_t*) * arity);

                // Copy AST types of given arguments into new array
                memcpy(new_arg_types, arg_types, sizeof(ast_type_t) * arity);

                // Attempt to fill in missing values
                for(length_t i = arity; i < pair.ast_func->arity; i++){
                    if(arg_defaults[i] == NULL){
                        // NOTE: We should have never received a function that can't be completed using its default values
                        compiler_panicf(builder->compiler, call_expr->source, "INTERNAL ERROR: Failed to fill in default value for argument %d", i);
                        ast_types_free(&new_arg_types[arity], i - arity);
                        ast_types_free_fully(arg_types, arity);
                        return FAILURE;
                    }

                    if(ir_gen_expr(builder, arg_defaults[i], &new_args[i], false, &new_arg_types[i])){
                        ast_types_free(&new_arg_types[arity], i - arity);
                        ast_types_free_fully(arg_types, arity);
                        return FAILURE;
                    }
                }

                // NOTE: We are discarding the memory held by 'arg_values'
                //       Since it is a part of the pool, it'll just remain stagnant until
                //       the pool is freed. As far as I can tell, the costs of trying to reuse
                //       the memory isn't worth it ('cause it's only like 8 bytes per entry and memory is cheap)
                // TODO: CLEANUP: Maybe not abandon this memory
                arg_values = new_args;
                arity = pair.ast_func->arity;

                // Replace argument types array
                free(arg_types);
                arg_types = new_arg_types;
            }
            
            if(pair.ast_func->traits & AST_FUNC_VARARG){
                trait_t arg_type_traits[arity];
                memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
                memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (arity - pair.ast_func->arity));

                // Use padded 'arg_type_traits' with TRAIT_NONE for variable argument functions    
                if(handle_pass_management(builder, arg_values, arg_types, arg_type_traits, arity)){
                    ast_types_free_fully(arg_types, arity);
                    return FAILURE;
                }
            } else if(handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, arity)){
                ast_types_free_fully(arg_types, arity);
                return FAILURE;
            }

            // Revalidate 'ast_func' and 'ir_func'
            pair.ast_func = &builder->object->ast.funcs[pair.ast_func_id];
            pair.ir_func = &builder->object->ir_module.funcs[pair.ir_func_id];
            
            ir_basicblock_new_instructions(builder->current_block, 1);
            instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
            ((ir_instr_call_t*) instruction)->id = INSTRUCTION_CALL;
            ((ir_instr_call_t*) instruction)->result_type = pair.ir_func->return_type;
            ((ir_instr_call_t*) instruction)->values = arg_values;
            ((ir_instr_call_t*) instruction)->values_length = arity;
            ((ir_instr_call_t*) instruction)->ir_func_id = pair.ir_func_id;
            builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;

            // Don't even bother with result unless we care about the it
            if(ir_value){
                *ir_value = build_value_from_prev_instruction(builder);
            }

            if(stack_pointer){
                // Dereference arg_types[0] and call __defer__ on arg_values[0]
                if(arg_types[0].elements_length == 0 || arg_types[0].elements[0]->id != AST_ELEM_POINTER){
                    compiler_panicf(builder->compiler, call_expr->source, "INTERNAL ERROR: Temporary mutable value location has non-pointer type");
                    ast_types_free_fully(arg_types, arity);
                    return FAILURE;
                }
                
                ast_type_dereference(&arg_types[0]);
                
                if(handle_single_deference(builder, &arg_types[0], arg_values[0]) == ALT_FAILURE) return FAILURE;
                build_stack_restore(builder, stack_pointer);
            }

            ast_types_free_fully(arg_types, arity);
            if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&pair.ast_func->return_type);
        }
        break;
    case EXPR_NOT: case EXPR_NEGATE: case EXPR_BIT_COMPLEMENT: {
            ast_expr_unary_t *unary_expr = (ast_expr_unary_t*) expr;
            ast_type_t expr_type;
            ir_value_t *expr_value;

            #define MACRO_UNARY_OPERATOR_CHARCTER (expr->id == EXPR_NOT ? '!' : (expr->id == EXPR_NEGATE ? '-' : '~'))

            if(ir_gen_expr(builder, unary_expr->value, &expr_value, false, &expr_type)) return FAILURE;

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
                if(ir_gen_expr(builder, ((ast_expr_new_t*) expr)->amount, &amount, false, &multiplier_type)) return FAILURE;
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

            ir_type_t *ubyte_ptr = ir_type_pointer_to(builder->pool, ubyte);
            length_t value_length = strlen(new_cstring_expr->value);

            ir_value_t *bytes_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
            bytes_value->value_type = VALUE_TYPE_LITERAL;
            bytes_value->type = ir_builder_usize(builder);
            bytes_value->extra = ir_pool_alloc(builder->pool, sizeof(adept_usize));
            *((adept_usize*) bytes_value->extra) = value_length + 1;

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

            maybe_index_t enum_index = ast_find_enum(builder->object->ast.enums, builder->object->ast.enums_length, enum_value_expr->enum_name);

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
            (*ir_value)->extra = ir_pool_alloc(builder->pool, sizeof(adept_usize));
            *((adept_usize*) (*ir_value)->extra) = enum_kind_id;

            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone(enum_value_expr->enum_name));
        }
        break;
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

                if(ir_gen_expr(builder, static_array_expr->values[i], &values[i], false, &member_type)){
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

                if(ir_gen_expr(builder, static_struct_expr->values[i], &values[i], false, &member_type)){
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
            
            if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
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

            if(ir_gen_expr(builder, ternary->condition, &condition, false, &condition_type))
                return FAILURE;

            if(!ast_types_conform(builder, &condition, &condition_type, &builder->static_bool, CONFORM_MODE_CALCULATION)){
                char *condition_type_str = ast_type_str(&condition_type);
                compiler_panicf(builder->compiler, ternary->condition->source, "Received type '%s' when conditional expects type 'bool'", condition_type_str);
                free(condition_type_str);
                ast_type_free(&condition_type);
                return FAILURE;
            }

            ast_type_free(&condition_type);

            length_t when_true_block_id = build_basicblock(builder);
            length_t when_false_block_id = build_basicblock(builder);

            build_cond_break(builder, condition, when_true_block_id, when_false_block_id);

            // Generate instructions for when condition is true
            build_using_basicblock(builder, when_true_block_id);
            if(ir_gen_expr(builder, ternary->if_true, &if_true, false, &if_true_type))
                return FAILURE;
            length_t when_true_landing = builder->current_block_id;

            // Generate instructions for when condition is false
            build_using_basicblock(builder, when_false_block_id);
            if(ir_gen_expr(builder, ternary->if_false, &if_false, false, &if_false_type)){
                ast_type_free(&if_true_type);
                return FAILURE;
            }
            length_t when_false_landing = builder->current_block_id;
            
            if(!ast_types_identical(&if_true_type, &if_false_type)){
                // Try to autocast to larger type of the two if there is one
                bool conflict_resolved = ir_gen_resolve_ternay_conflict(builder, &if_true, &if_false, &if_true_type, &if_false_type, &when_true_landing, &when_false_landing);
                
                if(!conflict_resolved){
                    char *if_true_typename = ast_type_str(&if_true_type);
                    char *if_false_typename = ast_type_str(&if_false_type);
                    compiler_panicf(builder->compiler, ternary->source, "Ternary operator could result in different types '%s' and '%s'", if_true_typename, if_false_typename);
                    ast_type_free(&if_true_type);
                    ast_type_free(&if_false_type);
                    free(if_true_typename);
                    free(if_false_typename);
                    return FAILURE;
                }
            }

            // Break from both landing blocks to the merge block
            length_t merge_block_id = build_basicblock(builder);
            build_using_basicblock(builder, when_false_landing);
            build_break(builder, merge_block_id);
            build_using_basicblock(builder, when_true_landing);
            build_break(builder, merge_block_id);

            // Merge to grab the result
            build_using_basicblock(builder, merge_block_id);

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

            *ir_value = build_phi2(builder, result_ir_type, if_true, if_false, when_true_landing, when_false_landing);
        }
        break;
    case EXPR_PREINCREMENT: case EXPR_PREDECREMENT:
    case EXPR_POSTINCREMENT: case EXPR_POSTDECREMENT: {
            ir_value_t *before_value;
            ast_type_t before_ast_type;
            ast_expr_unary_t *unary = (ast_expr_unary_t*) expr;

            // NOTE: unary->value is guaranteed to be a mutable expression
            if(ir_gen_expr(builder, unary->value, &before_value, true, &before_ast_type))
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
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_byte));
                *((adept_byte*) one->extra) = 1;
                break;
            case TYPE_KIND_U8:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_ubyte));
                *((adept_ubyte*) one->extra) = 1;
                break;
            case TYPE_KIND_S16:
                // stored w/ extra precision
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_short));
                *((adept_short*) one->extra) = 1;
                break;
            case TYPE_KIND_U16:
                // stored w/ extra precision
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_ushort));
                *((adept_ushort*) one->extra) = 1;
                break;
            case TYPE_KIND_S32:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_int));
                *((adept_int*) one->extra) = 1;
                break;
            case TYPE_KIND_U32:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_uint));
                *((adept_uint*) one->extra) = 1;
                break;
            case TYPE_KIND_S64:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_long));
                *((adept_long*) one->extra) = 1;
                break;
            case TYPE_KIND_U64:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_ulong));
                *((adept_ulong*) one->extra) = 1;
                break;
            case TYPE_KIND_FLOAT:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_float));
                *((adept_float*) one->extra) = 1.0f;
                break;
            case TYPE_KIND_DOUBLE:
                one->extra = ir_pool_alloc(builder->pool, sizeof(adept_double));
                *((adept_double*) one->extra) = 1.0f;
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
            ((ir_instr_math_t*) instruction)->a = build_load(builder, before_value, expr->source);
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
            build_store(builder, modified, before_value, expr->source);

            if(out_expr_type){
                *out_expr_type = before_ast_type;
            } else {
                ast_type_free(&before_ast_type);
            }

            // Don't even bother with result unless we care about the it
            if(ir_value){
                if(expr->id == EXPR_PREINCREMENT || expr->id == EXPR_PREDECREMENT){
                    *ir_value = leave_mutable ? before_value : build_load(builder, before_value, expr->source);
                } else {
                    *ir_value = leave_mutable ? before_value : ((ir_instr_math_t*) instruction)->a;
                }
            }
        }
        break;
    case EXPR_TOGGLE: {
            ast_expr_unary_t *toggle_expr = (ast_expr_unary_t*) expr;
            ir_value_t *mutable_value;
            ast_type_t ast_type;

            if(ir_gen_expr(builder, toggle_expr->value, &mutable_value, true, &ast_type)) return FAILURE;

            ir_type_t *dereferenced_ir_type = ir_type_dereference(mutable_value->type);

            if(!ast_type_is_base_of(&ast_type, "bool") || dereferenced_ir_type == NULL || dereferenced_ir_type->kind != TYPE_KIND_BOOLEAN){
                char *t = ast_type_str(&ast_type);
                compiler_panicf(builder->compiler, toggle_expr->source, "Cannot toggle non-boolean type '%s'", t);
                ast_type_free(&ast_type);
                free(t);
                return FAILURE;
            }

            // Load and not the boolean value
            ir_value_t *loaded = build_load(builder, mutable_value, toggle_expr->source);
            
            ir_instr_t *built_instr = build_instruction(builder, sizeof(ir_instr_unary_t));
            ((ir_instr_unary_t*) built_instr)->id = INSTRUCTION_ISZERO;
            ((ir_instr_unary_t*) built_instr)->result_type = ir_builder_bool(builder);
            ((ir_instr_unary_t*) built_instr)->value = loaded;
            ir_value_t *notted = build_value_from_prev_instruction(builder);
            
            // Store it back into memory
            build_store(builder, notted, mutable_value, toggle_expr->source);

            if(out_expr_type){
                *out_expr_type = ast_type;
            } else {
                ast_type_free(&ast_type);
            }

            // Don't even bother with expression result unless we care about the it
            if(ir_value){
                *ir_value = leave_mutable ? mutable_value : notted;
            }
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = ((ast_expr_inline_declare_t*) expr);

            // Search for existing variable named that
            if(bridge_scope_var_already_in_list(builder->scope, def->name)){
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
                if(ir_gen_expr(builder, def->value, &initial, false, &temporary_type)) return FAILURE;

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
                    build_store(builder, initial, destination, expr->source);
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

    return SUCCESS;
}

errorcode_t ir_gen_expr_and(ir_builder_t *builder, ast_expr_and_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_value_t *a, *b;
    length_t landing_a_block_id, landing_b_block_id, landing_more_block_id;

    if(ir_gen_expr_pre_andor(builder, expr, &a, &b, &landing_a_block_id, &landing_b_block_id, &landing_more_block_id, out_expr_type)) return FAILURE;

    // Merge evaluation with short circuit
    length_t merge_block_id = build_basicblock(builder);
    build_break(builder, merge_block_id);
    build_using_basicblock(builder, landing_a_block_id);
    build_cond_break(builder, a, landing_more_block_id, merge_block_id);

    build_using_basicblock(builder, merge_block_id);
    *ir_value = build_phi2(builder, ir_builder_bool(builder), build_bool(builder->pool, false), b, landing_a_block_id, landing_b_block_id);
    return SUCCESS;
}

errorcode_t ir_gen_expr_or(ir_builder_t *builder, ast_expr_or_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_value_t *a, *b;
    length_t landing_a_block_id, landing_b_block_id, landing_more_block_id;

    if(ir_gen_expr_pre_andor(builder, expr, &a, &b, &landing_a_block_id, &landing_b_block_id, &landing_more_block_id, out_expr_type)) return FAILURE;

    // Merge evaluation
    length_t merge_block_id = build_basicblock(builder);
    build_break(builder, merge_block_id);
    build_using_basicblock(builder, landing_a_block_id);
    build_cond_break(builder, a, merge_block_id, landing_more_block_id);
    build_using_basicblock(builder, merge_block_id);

    *ir_value = build_phi2(builder, ir_builder_bool(builder), build_bool(builder->pool, true), b, landing_a_block_id, landing_b_block_id);
    return SUCCESS;
}

errorcode_t ir_gen_expr_pre_andor(ir_builder_t *builder, ast_expr_math_t *andor_expr, ir_value_t **a, ir_value_t **b,
        length_t *landing_a_block_id, length_t *landing_b_block_id, length_t *landing_more_block_id, ast_type_t *out_expr_type){
    
    ast_type_t ast_type_a, ast_type_b;

    // Conform expression 'a' to type 'bool' to ensure 'b' will also have to conform
    // Use 'out_expr_type' to store bool type (will stay there anyways cause resulting type is a bool)
    ast_type_make_base(out_expr_type, strclone("bool"));

    // Generate value for 'a' expression
    if(ir_gen_expr(builder, andor_expr->a, a, false, &ast_type_a)){
        ast_type_free(out_expr_type);
        return FAILURE;
    }

    // Force 'a' value to be a boolean
    if(!ast_types_identical(&ast_type_a, out_expr_type) && !ast_types_conform(builder, a, &ast_type_a, out_expr_type, CONFORM_MODE_CALCULATION)){
        char *a_type_str = ast_type_str(&ast_type_a);
        compiler_panicf(builder->compiler, andor_expr->source, "Failed to convert value of type '%s' to type 'bool'", a_type_str);
        free(a_type_str);
        ast_type_free(&ast_type_a);
        ast_type_free(out_expr_type);
        return FAILURE;
    }

    *landing_a_block_id = builder->current_block_id;
    *landing_more_block_id = build_basicblock(builder);
    build_using_basicblock(builder, *landing_more_block_id);

    // Generate value for 'b' expression
    if(ir_gen_expr(builder, ((ast_expr_math_t*) andor_expr)->b, b, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        ast_type_free(out_expr_type);
        return FAILURE;
    }

    // Force 'b' value to be a boolean
    if(!ast_types_identical(&ast_type_b, out_expr_type) && !ast_types_conform(builder, b, &ast_type_b, out_expr_type, CONFORM_MODE_CALCULATION)){
        char *b_type_str = ast_type_str(&ast_type_b);
        compiler_panicf(builder->compiler, andor_expr->source, "Failed to convert value of type '%s' to type 'bool'", b_type_str);
        free(b_type_str);
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
        ast_type_free(out_expr_type);
        return FAILURE;
    }

    // Each parameter will be a boolean
    ast_type_free(&ast_type_a);
    ast_type_free(&ast_type_b);

    *landing_b_block_id = builder->current_block_id;
    return SUCCESS;
}

errorcode_t ir_gen_expr_str(ir_builder_t *builder, ast_expr_str_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    *ir_value = build_literal_str(builder, expr->array, expr->length);
    if(*ir_value == NULL) return FAILURE;
    
    // Has type of 'String'
    if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("String"));
    return SUCCESS;
}

errorcode_t ir_gen_expr_cstr(ir_builder_t *builder, ast_expr_cstr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    *ir_value = build_literal_cstr(builder, expr->value);

    // Has type of '*ubyte'
    if(out_expr_type != NULL) ast_type_make_base_ptr(out_expr_type, strclone("ubyte"));
    return SUCCESS;
}

errorcode_t ir_gen_expr_variable(ir_builder_t *builder, ast_expr_variable_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    char *variable_name = expr->name;
    bridge_var_t *variable = bridge_scope_find_var(builder->scope, variable_name);

    // Found variable in nearby scope
    if(variable){
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(variable->ast_type);
        ir_type_t *ir_ptr_type = ir_type_pointer_to(builder->pool, variable->ir_type);

        // Variable-Pointer instruction to get pointer to stack variable
        *ir_value = build_varptr(builder, ir_ptr_type, variable->id);

        // Load if the variable is a reference
        if(variable->traits & BRIDGE_VAR_REFERENCE) *ir_value = build_load(builder, *ir_value, expr->source);

        // Unless requested to leave the expression mutable, dereference it
        if(!leave_mutable) *ir_value = build_load(builder, *ir_value, expr->source);
        return SUCCESS;
    }

    ast_t *ast = &builder->object->ast;
    ir_module_t *ir_module = &builder->object->ir_module;

    // Attempt to find global variable
    maybe_index_t global_variable_index = ast_find_global(ast->globals, ast->globals_length, variable_name);

    // Found variable as global variable
    if(global_variable_index != -1){
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&ast->globals[global_variable_index].type);

        // DANGEROUS: Using AST global variable index as IR global variable index
        ir_global_t *ir_global = &ir_module->globals[global_variable_index];
        *ir_value = build_gvarptr(builder, ir_type_pointer_to(builder->pool, ir_global->type), global_variable_index);
        
        // If not requested to leave the expression mutable, dereference it
        if(!leave_mutable) *ir_value = build_load(builder, *ir_value, expr->source);
        return SUCCESS;
    }

    // No suitable variable or global variable found
    compiler_panicf(builder->compiler, ((ast_expr_variable_t*) expr)->source, "Undeclared variable '%s'", variable_name);
    const char *nearest = bridge_scope_var_nearest(builder->scope, variable_name);
    if(nearest) printf("\nDid you mean '%s'?\n", nearest);
    return FAILURE;
}

errorcode_t ir_gen_expr_call(ir_builder_t *builder, ast_expr_call_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ast_t *ast = &builder->object->ast;

    length_t arity = expr->arity;
    ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * arity);
    ast_type_t *arg_types = malloc(sizeof(ast_type_t) * arity);

    // Temporary values used to hold type of variable/global-variable
    ast_type_t *tmp_ast_variable_type;
    ir_type_t *tmp_ir_variable_type;

    // Generate values for function arguments
    for(length_t a = 0; a != arity; a++){
        if(ir_gen_expr(builder, expr->args[a], &arg_values[a], false, &arg_types[a])){
            ast_types_free_fully(arg_types, a);
            return FAILURE;
        }
    }

    // Check for variable of name in nearby scope
    bridge_var_t *var = bridge_scope_find_var(builder->scope, expr->name);
    bool hard_break = false;

    // Found variable of name in nearby scope
    if(var){
        // Get AST and IR type of function from variable
        tmp_ast_variable_type = var->ast_type;
        tmp_ir_variable_type = ir_type_pointer_to(builder->pool, var->ir_type);

        // Fail if AST type isn't a function pointer type
        if(tmp_ast_variable_type->elements_length != 1 || tmp_ast_variable_type->elements[0]->id != AST_ELEM_FUNC){
            // If call is tentative, then don't complain and just continue as if nothing happened
            if(expr->is_tentative) return SUCCESS;
            
            // Otherwise print error message
            char *s = ast_type_str(tmp_ast_variable_type);
            compiler_panicf(builder->compiler, expr->source, "Can't call value of non function type '%s'", s);
            ast_types_free_fully(arg_types, arity);
            free(s);
            return FAILURE;
        }

        // Load function pointer value from variable
        *ir_value = build_varptr(builder, tmp_ir_variable_type, var->id);
        *ir_value = build_load(builder, *ir_value, expr->source);

        // Call function pointer value
        errorcode_t error = ir_gen_call_function_value(builder, tmp_ast_variable_type, tmp_ir_variable_type, expr, arg_values, arg_types, ir_value, out_expr_type);
        ast_types_free_fully(arg_types, arity);

        // Propagate failure if failed to call function pointer value
        if(error == FAILURE) return FAILURE;

        // The function pointer couldn't be called, but the call is tentative, so we pretend like it didn't happen
        if(error == ALT_FAILURE){
            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
            return SUCCESS;
        }
        
        // Otherwise, we successful called the function pointer value from a nearby scoped variable
        return SUCCESS;
    }

    // If there doesn't exist a nearby scoped variable with the same name, look for function
    funcpair_t pair;
    errorcode_t error = ir_gen_find_func_conforming(builder, expr->name, arg_values, arg_types, arity, &pair);
    
    // Propagate failure if something went wrong during the search
    if(error == ALT_FAILURE){
        ast_types_free_fully(arg_types, arity);
        return FAILURE;
    }

    // Found function that fits given name and arguments
    if(error == SUCCESS){

        // If the found function has default arguments and we are missing arguments, fill them in
        if(pair.ast_func->arg_defaults && arity != pair.ast_func->arity){
            // ------------------------------------------------
            // | 0 | 1 | 2 |                    Supplied
            // | 0 | 1 | 2 | 3 | 4 |            Required
            // |   | 1 |   | 3 | 4 |            Defaults
            // ------------------------------------------------

            ast_expr_t **arg_defaults = pair.ast_func->arg_defaults;
            
            // Allocate memory to hold full version of argument list
            ir_value_t **new_args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * pair.ast_func->arity);
            ast_type_t *new_arg_types = malloc(sizeof(ast_type_t) * pair.ast_func->arity);

            // Copy given argument values into new array
            memcpy(new_args, arg_values, sizeof(ir_value_t*) * arity);

            // Copy AST types of given arguments into new array
            memcpy(new_arg_types, arg_types, sizeof(ast_type_t) * arity);

            // Attempt to fill in missing values
            for(length_t i = arity; i != pair.ast_func->arity; i++){
                // We should have never received a function that can't be completed using its default values
                if(arg_defaults[i] == NULL){
                    compiler_panicf(builder->compiler, expr->source, "INTERNAL ERROR: Failed to fill in default value for argument %d", i);
                    ast_types_free(&new_arg_types[arity], i - arity);
                    ast_types_free_fully(arg_types, expr->arity);
                    return FAILURE;
                }

                // Generate IR value for given default expression
                if(ir_gen_expr(builder, arg_defaults[i], &new_args[i], false, &new_arg_types[i])){
                    ast_types_free(&new_arg_types[arity], i - arity);
                    ast_types_free_fully(arg_types, expr->arity);
                    return FAILURE;
                }
            }

            // Swap out partial argument list for full argument list.

            // NOTE: We are abandoning the memory held by 'arg_values'
            //       Since it is a part of the pool, it'll just remain stagnant until
            //       the pool is freed
            arg_values = new_args;
            arity = pair.ast_func->arity;

            // Replace argument types array
            free(arg_types);
            arg_types = new_arg_types;
        }
        
        // Handle __pass__ calls for argument values being passed
        if(pair.ast_func->traits & AST_FUNC_VARARG || pair.ast_func->traits & AST_FUNC_VARIADIC){
            // Use padded 'arg_type_traits' with TRAIT_NONE for variable argument functions    
            trait_t arg_type_traits[arity];
            memcpy(arg_type_traits, pair.ast_func->arg_type_traits, sizeof(trait_t) * pair.ast_func->arity);
            memset(&arg_type_traits[pair.ast_func->arity], TRAIT_NONE, sizeof(trait_t) * (arity - pair.ast_func->arity));

            if(handle_pass_management(builder, arg_values, arg_types, arg_type_traits, arity)){
                ast_types_free_fully(arg_types, arity);
                return FAILURE;
            }
        } else if(handle_pass_management(builder, arg_values, arg_types, pair.ast_func->arg_type_traits, arity)){
            ast_types_free_fully(arg_types, arity);
            return FAILURE;
        }

        // Revalidate 'ast_func' and 'ir_func'
        pair.ast_func = &builder->object->ast.funcs[pair.ast_func_id];
        pair.ir_func = &builder->object->ir_module.funcs[pair.ir_func_id];

        // Prepare for potential stack allocation
        ir_value_t *saved_stack = NULL;

        // Pack variadic arguments into variadic array if applicable
        if(pair.ast_func->traits & AST_FUNC_VARIADIC){
            // Calculate number of variadic arguments
            length_t variadic_count = arity - pair.ast_func->arity;

            // Do __builtin_warn_bad_printf_format if applicable
            if(pair.ast_func->traits & AST_FUNC_WARN_BAD_PRINTF_FORMAT){
                if(ir_gen_do_builtin_warn_bad_printf_format(builder, pair, arg_types, arg_values, expr->source, variadic_count)){
                    // Free AST types of the expressions given for the arguments
                    ast_types_free(arg_types, arity);
                    return FAILURE;
                }
            }

            // Save stack pointer
            saved_stack = build_stack_save(builder);
            
            ir_value_t *raw_types_array = NULL;
            ir_type_t *pAnyType_ir_type = NULL;

            // Obtain IR type of '*AnyType' type if typeinfo is enabled
            if(!(builder->compiler->traits & COMPILER_NO_TYPEINFO)){
                ast_type_t any_type;
                ast_type_make_base_ptr(&any_type, strclone("AnyType"));

                if(ir_gen_resolve_type(builder->compiler, builder->object, &any_type, &pAnyType_ir_type)){
                    ast_type_free(&any_type);
                    ast_types_free(arg_types, arity);
                    return FAILURE;
                }
                ast_type_free(&any_type);
            }

            // Calculate total size of raw data array in bytes
            ir_type_t *usize_type = ir_builder_usize(builder);
            ir_value_t *bytes = NULL;
            
            if(variadic_count != 0){
                bytes = build_const_sizeof(builder->pool, usize_type, arg_values[pair.ast_func->arity]->type);
            } else {
                bytes = build_literal_usize(builder->pool, 0);
            }

            for(length_t i = 1; i < variadic_count; i++){
                ir_value_t *more_bytes = build_const_sizeof(builder->pool, usize_type, arg_values[pair.ast_func->arity + i]->type);
                bytes = build_const_add(builder->pool, bytes, more_bytes);
            }

            // Obtain IR type for 'ubyte' type
            ir_type_t *ubyte_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ubyte_type->kind = TYPE_KIND_S8;
            /* neglect ubyte_type->extra */

            // Allocate memory on the stack to hold raw data array
            ir_value_t *raw_data_array = NULL;
            
            if(variadic_count != 0){
                raw_data_array = build_alloc_array(builder, ubyte_type, bytes);

                // Bitcast the value fromm '[n] ubyte' type to '*ubyte' type
                raw_data_array = build_bitcast(builder, raw_data_array, ir_type_pointer_to(builder->pool, ubyte_type));
            } else {
                raw_data_array = build_null_pointer(builder->pool);
            }

            // Use iterative value 'current_offset' to store variadic arguments values into the raw data array
            ir_value_t *current_offset = build_literal_usize(builder->pool, 0);

            for(length_t i = 0; i < variadic_count; i++){
                ir_value_t *arg = build_array_access(builder, raw_data_array, current_offset, expr->source);
                arg = build_bitcast(builder, arg, ir_type_pointer_to(builder->pool, arg_values[pair.ast_func->arity + i]->type));
                build_store(builder, arg_values[pair.ast_func->arity + i], arg, expr->source);

                // Move iterative offset 'current_offset' along by the size of the type we just wrote
                if(i + 1 < variadic_count){
                    current_offset = build_const_add(builder->pool, current_offset, build_const_sizeof(builder->pool, usize_type, arg_values[pair.ast_func->arity + i]->type));
                }
            }
            
            // Create raw types array of '*AnyType' values if runtime type info is enabled
            if(pAnyType_ir_type && variadic_count != 0){
                raw_types_array = build_alloc_array(builder, pAnyType_ir_type, build_literal_usize(builder->pool, variadic_count));
                raw_types_array = build_bitcast(builder, raw_types_array, ir_type_pointer_to(builder->pool, pAnyType_ir_type));

                for(length_t i = 0; i < variadic_count; i++){
                    ir_value_t *arg_type = build_array_access(builder, raw_types_array, build_literal_usize(builder->pool, i), expr->source);
                    build_store(builder, rtti_for(builder, &arg_types[pair.ast_func->arity + i], expr->source), arg_type, expr->source);
                }
            }

            // Create temporary IR '*s8' type to use
            ir_type_t *ptr_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
            ptr_type->kind = TYPE_KIND_S8;
            /* neglect ptr_type->extra */
            ptr_type = ir_type_pointer_to(builder->pool, ptr_type);

            // If runtime type info is disabled, then just set 'raw_types_array' to a null pointer
            if(raw_types_array == NULL) raw_types_array = build_null_pointer_of_type(builder->pool, ptr_type);

            // '__variadic_array__' arguments:
            // pointer = raw_data_array
            // bytes = bytes
            // length = variadic_count
            // maybe_types = raw_types_array

            ir_value_t **variadic_array_function_arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 4);
            variadic_array_function_arguments[0] = build_bitcast(builder, raw_data_array, ptr_type);
            variadic_array_function_arguments[1] = bytes;
            variadic_array_function_arguments[2] = build_literal_usize(builder->pool, variadic_count);
            variadic_array_function_arguments[3] = build_bitcast(builder, raw_types_array, ptr_type);
            
            // Create variadic array value using the special function '__variadic_array__'
            ir_instr_call_t *instruction = (ir_instr_call_t*) build_instruction(builder, sizeof(ir_instr_call_t));
            instruction->id = INSTRUCTION_CALL;
            instruction->result_type = builder->object->ir_module.common.ir_variadic_array;
            instruction->values = variadic_array_function_arguments;
            instruction->values_length = 4;
            instruction->ir_func_id = builder->object->ir_module.common.variadic_ir_func_id;

            // Free AST types of the expressions given for the arguments
            ast_types_free(arg_types, arity);

            // Make space for variadic array argument
            if(variadic_count == 0){
                ir_value_t **new_args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (pair.ast_func->arity + 1));

                // Copy given argument values into new array
                // NOTE: We are abandoning the old arg_values array
                memcpy(new_args, arg_values, sizeof(ir_value_t*) * (pair.ast_func->arity + 1));
                arg_values = new_args;
            }

            // Replace argument values after regular arguments with single variadic array argument value
            arg_values[pair.ast_func->arity] = build_value_from_prev_instruction(builder);
            arity = pair.ast_func->arity + 1;

            // Now that the argument values are correct, we can continue calling the function as normal
        } else {
            // Free AST types of the expressions given for the arguments
            ast_types_free(arg_types, arity);
        }
        
        // Call the actual function
        ir_instr_call_t *instruction = (ir_instr_call_t*) build_instruction(builder, sizeof(ir_instr_call_t));
        instruction->id = INSTRUCTION_CALL;
        instruction->result_type = pair.ir_func->return_type;
        instruction->values = arg_values;
        instruction->values_length = arity;
        instruction->ir_func_id = pair.ir_func_id;

        // Store resulting value from call expression if requested
        if(ir_value) *ir_value = build_value_from_prev_instruction(builder);

        // Restore the stack if we allocated memory on it
        if(saved_stack) build_stack_restore(builder, saved_stack);

        // Give back return type of the called function
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&pair.ast_func->return_type);
        return SUCCESS;
    }

    // If no function exists, look for global
    maybe_index_t global_variable_index = ast_find_global(ast->globals, ast->globals_length, expr->name);

    if(global_variable_index != -1){
        tmp_ast_variable_type = &ast->globals[global_variable_index].type;

        // Get IR type of pointer to the variable type
        tmp_ir_variable_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        tmp_ir_variable_type->kind = TYPE_KIND_POINTER;

        if(ir_gen_resolve_type(builder->compiler, builder->object, tmp_ast_variable_type, (ir_type_t**) &tmp_ir_variable_type->extra)){
            ast_types_free_fully(arg_types, expr->arity);
            return FAILURE;
        }

        // Ensure the type of the global variable is a function pointer
        if(tmp_ast_variable_type->elements_length != 1 || tmp_ast_variable_type->elements[0]->id != AST_ELEM_FUNC){
            // The type of the global variable isn't a function pointer
            // Ignore failure if the call expression is tentative
            if(expr->is_tentative){
                if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
                ast_types_free_fully(arg_types, expr->arity);
                hard_break = true;
                return SUCCESS;
            }

            // Otherwise print error message
            char *s = ast_type_str(tmp_ast_variable_type);
            compiler_panicf(builder->compiler, expr->source, "Can't call value of non function type '%s'", s);
            ast_types_free_fully(arg_types, expr->arity);
            free(s);
            return FAILURE;
        }

        // Load the value of the function pointer from the global variable
        // DANGEROUS: Using AST global variable index as IR global variable index
        *ir_value = build_load(builder, build_gvarptr(builder, tmp_ir_variable_type, global_variable_index), expr->source);

        // Call the function pointer value
        errorcode_t error = ir_gen_call_function_value(builder, tmp_ast_variable_type, tmp_ir_variable_type, expr, arg_values, arg_types, ir_value, out_expr_type);
        ast_types_free_fully(arg_types, expr->arity);

        // Propogate failure to call function pointer value
        if(error == FAILURE) return FAILURE;

        // If calling the function pointer value failed, but the call was tentative, then ignore the failure
        if(error == ALT_FAILURE){
            if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
            return SUCCESS;
        }

        return SUCCESS;
    }
    
    // Otherwise no function or variable with a matching name was found
    // ...

    // If the call expression was tentative, then ignore the failure
    if(expr->is_tentative){
        if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("void"));
        ast_types_free_fully(arg_types, expr->arity);
        return SUCCESS;
    }

    // Otherwise, print an error messsage
    compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, arg_types, expr->arity);
    ast_types_free_fully(arg_types, expr->arity);
    return FAILURE;
}

errorcode_t ir_gen_expr_member(ir_builder_t *builder, ast_expr_member_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ir_value_t *struct_value;
    ast_type_t struct_value_ast_type;

    // This expression should be able to be mutable (Checked during parsing)
    if(ir_gen_expr(builder, expr->value, &struct_value, true, &struct_value_ast_type)) return FAILURE;

    // Ensure that IR value we get back is a pointer, cause if it isn't, then it isn't mutable
    if(struct_value->type->kind != TYPE_KIND_POINTER){
        char *given_type = ast_type_str(&struct_value_ast_type);
        compiler_panicf(builder->compiler, expr->source, "Can't use member operator '.' on temporary value of type '%s'", given_type);
        free(given_type);
        ast_type_free(&struct_value_ast_type);
        return FAILURE;
    }

    // Auto-derefernce non-ptr pointer types that don't point to other pointers
    if(ast_type_is_pointer(&struct_value_ast_type) && struct_value_ast_type.elements[1]->id != AST_ELEM_POINTER){
        ast_type_dereference(&struct_value_ast_type);
        
        // Defererence one layer if struct value is **StructType
        if(expr_is_mutable(expr->value)) struct_value = build_load(builder, struct_value, expr->source);
    }

    // Get member field information
    length_t field_index;
    ir_type_t *field_type;
    ast_elem_t *elem = struct_value_ast_type.elements[0];
    bool is_via_union;

    if(ir_gen_expr_member_get_field_info(builder, expr, elem, &struct_value_ast_type, &field_index, &field_type, &is_via_union, out_expr_type)){
        ast_type_free(&struct_value_ast_type);
        return FAILURE;
    }

    // Build the member access
    if(is_via_union){
        *ir_value = build_bitcast(builder, struct_value, ir_type_pointer_to(builder->pool, field_type));
    } else {
        *ir_value = build_member(builder, struct_value, field_index, ir_type_pointer_to(builder->pool, field_type), expr->source);
    }
    
    // If not requested to leave the expression mutable, dereference it
    if(!leave_mutable) *ir_value = build_load(builder, *ir_value, expr->source);

    ast_type_free(&struct_value_ast_type);
    return SUCCESS;
}

errorcode_t ir_gen_expr_member_get_field_info(ir_builder_t *builder, ast_expr_member_t *expr, ast_elem_t *elem, ast_type_t *struct_value_ast_type,
        length_t *field_index, ir_type_t **field_type, bool *is_via_union, ast_type_t *out_expr_type){
    
    if(elem->id == AST_ELEM_BASE){
        // Basic 'Struct' structure type
        weak_cstr_t struct_name = ((ast_elem_base_t*) elem)->base;
        ast_struct_t *target = ast_struct_find(&builder->object->ast, struct_name);

        // If we didn't find the structure, show an error message and return failure
        if(target == NULL){
            weak_cstr_t message_format = typename_is_entended_builtin_type(struct_name) ? "Can't use member operator on built-in type '%s'" : "INTERNAL ERROR: Failed to find struct '%s' that should exist";
            compiler_panicf(builder->compiler, expr->source, message_format, struct_name);
            return FAILURE;
        }

        // Find the field of the structure by name
        if(!ast_struct_find_field(target, expr->member, field_index)){
            char *struct_typename = ast_type_str(struct_value_ast_type);
            compiler_panicf(builder->compiler, expr->source, "Field '%s' doesn't exist in struct '%s'", expr->member, struct_typename);
            free(struct_typename);
            return FAILURE;
        }

        // Resolve AST field type to IR field type
        if(ir_gen_resolve_type(builder->compiler, builder->object, &target->field_types[*field_index], field_type)) return FAILURE;

        // Result type is the type of that member
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&target->field_types[*field_index]);
        *is_via_union = target->traits & AST_STRUCT_IS_UNION;
        return SUCCESS;
    }

    if(elem->id == AST_ELEM_GENERIC_BASE){
        // Complex '<$T> Struct' structure type
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) elem;

        weak_cstr_t struct_name = generic_base->name;
        ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&builder->object->ast, struct_name);

        // Find the polymorphic structure
        if(template == NULL){
            compiler_panicf(builder->compiler, ((ast_expr_member_t*) expr)->source, "INTERNAL ERROR: Failed to find polymorphic struct '%s' that should exist", struct_name);
            return FAILURE;
        }

        // Find the field of the polymorphic structure by name
        if(!ast_struct_find_field((ast_struct_t*) template, expr->member, field_index)){
            char *struct_typename = ast_type_str(struct_value_ast_type);
            compiler_panicf(builder->compiler, expr->source, "Field '%s' doesn't exist in struct '%s'", expr->member, struct_typename);
            free(struct_typename);
            return FAILURE;
        }

        // Create a catalog of all the known polymorphic type substitutions
        ast_type_var_catalog_t catalog;
        ast_type_var_catalog_init(&catalog);

        // Ensure that the number of parameters given for the generic base is the same as expected for the polymorphic structure
        if(template->generics_length != generic_base->generics_length){
            compiler_panicf(builder->compiler, expr->source, "Polymorphic struct '%s' type parameter length mismatch!", generic_base->name);
            ast_type_var_catalog_free(&catalog);
            return FAILURE;
        }

        // Add each entry given for the generic base structure type to the list of known polymorphic type substitutions
        for(length_t i = 0; i != template->generics_length; i++){
            ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
        }

        // Get the AST field type of the target field by index and resolve any polymorphs
        ast_type_t ast_field_type;
        if(resolve_type_polymorphics(builder->compiler, builder->type_table, &catalog, &template->field_types[*field_index], &ast_field_type)){
            ast_type_var_catalog_free(&catalog);
            return FAILURE;
        }

        // Get the IR type of the target field
        if(ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, field_type)){
            ast_type_var_catalog_free(&catalog);
            return FAILURE;
        }

        // Result type is the AST type of that member
        if(out_expr_type != NULL) *out_expr_type = ast_field_type;
        else                      ast_type_free(&ast_field_type);

        // Write whether member access should take place via union access
        *is_via_union = template->traits & AST_STRUCT_IS_UNION;

        // Dispose of the polymorphic substituions catalog
        ast_type_var_catalog_free(&catalog);
        return SUCCESS;
    }

    // Otherwise, we got a value that isn't a structure type
    char *typename = ast_type_str(struct_value_ast_type);
    compiler_panicf(builder->compiler, expr->source, "Can't use member operator on non-struct type '%s'", typename);
    free(typename);
    return FAILURE;
}

errorcode_t ir_gen_expr_address(ir_builder_t *builder, ast_expr_address_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // NOTE: The child expression should be able to be mutable (Checked during parsing)

    // Generate the child expression, and leave it as mutable
    if(ir_gen_expr(builder, expr->value, ir_value, true, out_expr_type)) return FAILURE;

    // Result type is just a pointer to the expression type
    if(out_expr_type != NULL) ast_type_prepend_ptr(out_expr_type);
    return SUCCESS;
}

errorcode_t ir_gen_expr_va_arg(ir_builder_t *builder, ast_expr_va_arg_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_value_t *va_list_value;
    ir_type_t *arg_type;
    ast_type_t temporary_type;

    // Generate IR value from expression given for list variable
    if(ir_gen_expr(builder, expr->va_list, &va_list_value, true, &temporary_type)) return FAILURE;

    // Ensure the type of the expression 
    if(!ast_type_is_base_of(&temporary_type, "va_list")){
        char *t = ast_type_str(&temporary_type);
        compiler_panicf(builder->compiler, expr->source, "Can't pass non-'va_list' type '%s' to va_arg", t);
        ast_type_free(&temporary_type);
        free(t);
        return FAILURE;
    }

    // Dispose of the temporary list expression AST type
    ast_type_free(&temporary_type);

    // Ensure the given list value is mutable
    if(!expr_is_mutable(expr->va_list)){
        compiler_panic(builder->compiler, expr->source, "Value passed for va_list to va_arg must be mutable");
        return FAILURE;
    }

    // Resolve the target AST type to and IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->arg_type, &arg_type)) return FAILURE;

    // Cast the list value from *va_list to *s8
    va_list_value = build_bitcast(builder, va_list_value, builder->ptr_type);
    
    ir_instr_va_arg_t *instruction = (ir_instr_va_arg_t*) build_instruction(builder, sizeof(ir_instr_va_arg_t));
    instruction->id = INSTRUCTION_VA_ARG;
    instruction->result_type = arg_type;
    instruction->va_list = va_list_value;
    *ir_value = build_value_from_prev_instruction(builder);

    // Result type is the AST type given to va_arg expression
    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&expr->arg_type);
    return SUCCESS;
}

errorcode_t ir_gen_expr_func_addr(ir_builder_t *builder, ast_expr_func_addr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    bool is_unique;
    funcpair_t pair;

    // No arguments given to match against
    if(expr->has_match_args == false){
        if(ir_gen_find_func_named(builder->compiler, builder->object, expr->name, &is_unique, &pair)){
            // If nothing exists and the lookup is tentative, fail tentatively
            if(expr->tentative) goto fail_tentatively;

            // Otherwise, we failed to find a function we were expecting to find
            compiler_panicf(builder->compiler, expr->source, "Undeclared function '%s'", expr->name);
            return FAILURE;
        }
        
        // Warn of multiple possibilities if the resulting function isn't unique in its name
        if(!is_unique && compiler_warnf(builder->compiler, expr->source, "Multiple functions named '%s', using the first of them", expr->name)){
            return FAILURE;
        }
    }
    
    // Otherwise we have arguments we can try to match against
    else if(ir_gen_find_func(builder, expr->name, expr->match_args, expr->match_args_length, &pair)){
        // If nothing exists and the lookup is tentative, fail tentatively
        if(expr->tentative) goto fail_tentatively;

        // Otherwise, we failed to find a function we were expecting to find
        compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, expr->match_args, expr->match_args_length);
        return FAILURE;
    }

    // Create the IR function pointer type
    ir_type_extra_function_t *extra = ir_pool_alloc(builder->pool, sizeof(ir_type_extra_function_t));
    extra->arg_types = pair.ir_func->argument_types;
    extra->arity = pair.ast_func->arity;
    extra->return_type = pair.ir_func->return_type;
    extra->traits = expr->traits;

    ir_type_t *ir_funcptr_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
    ir_funcptr_type->kind = TYPE_KIND_FUNCPTR;
    ir_funcptr_type->extra = extra;

    // If the function is only referenced by C-mangled name, remember its name
    const char *maybe_name = (pair.ast_func->traits & AST_FUNC_FOREIGN || pair.ast_func->traits & AST_FUNC_MAIN) ? expr->name : NULL;

    // Build 'get function address' instruction
    ir_instr_func_address_t *instruction = (ir_instr_func_address_t*) build_instruction(builder, sizeof(ir_instr_func_address_t));
    instruction->id = INSTRUCTION_FUNC_ADDRESS;
    instruction->result_type = ir_funcptr_type;
    instruction->name = maybe_name;
    instruction->ir_func_id = pair.ir_func_id;
    *ir_value = build_value_from_prev_instruction(builder);

    // Write resulting type if requested
    if(out_expr_type != NULL){
        ast_func_t *ast_func = pair.ast_func;
        ast_type_make_func_ptr(out_expr_type, expr->source, ast_func->arg_types, ast_func->arity, &ast_func->return_type, ast_func->traits, false);
    }
    
    // We successfully got the function address of the function
    return SUCCESS;

fail_tentatively:
    // Since we cannot know the exact type of the function returned (because none exist)
    // we must return a generic null pointer of type 'ptr'
    // I'm not sure whether this is the best choice, but it feels right
    // - Isaac (Mar 21 2020)
    *ir_value = build_null_pointer(builder->pool);
    if(out_expr_type != NULL) ast_type_make_base(out_expr_type, strclone("ptr"));
    return SUCCESS;
}

errorcode_t ir_gen_expr_dereference(ir_builder_t *builder, ast_expr_dereference_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ir_value_t *expr_value;
    ast_type_t expr_type;

    if(ir_gen_expr(builder, expr->value, &expr_value, false, &expr_type)) return FAILURE;

    // Ensure that the result ast_type_t is a pointer type
    if(!ast_type_is_pointer(&expr_type)){
        char *expr_type_str = ast_type_str(&expr_type);
        compiler_panicf(builder->compiler, expr->source, "Can't dereference non-pointer type '%s'", expr_type_str);
        ast_type_free(&expr_type);
        free(expr_type_str);
        return FAILURE;
    }

    // Dereference the pointer type
    ast_type_dereference(&expr_type);

    // If not requested to leave the expression mutable, dereference it
    if(!leave_mutable){
        // ir_type_t is expected to be of kind pointer and it's child to also be of kind pointer
        if(expr_value->type->kind != TYPE_KIND_POINTER){
            compiler_panic(builder->compiler, expr->source, "INTERNAL ERROR: Expected ir_type_t to be a pointer inside ir_gen_expr_dereference()");
            ast_type_free(&expr_type);
            return FAILURE;
        }

        // Build and append a load instruction
        *ir_value = build_load(builder, expr_value, expr->source);
    } else {
        *ir_value = expr_value;
    }

    // Result type is the deferenced type, if not requested, we'll dispose of it
    if(out_expr_type != NULL) *out_expr_type = expr_type;
    else ast_type_free(&expr_type);
    return SUCCESS;
}

errorcode_t ir_gen_expr_array_access(ir_builder_t *builder, ast_expr_array_access_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ast_type_t index_type, array_type;
    ir_value_t *index_value, *array_value;
    weak_cstr_t error_with_type;
    
    // Generate IR values for array value and index value
    if(ir_gen_expr(builder, expr->value, &array_value, true, &array_type)) return FAILURE;
    if(ir_gen_expr(builder, expr->index, &index_value, false, &index_type)){
        ast_type_free(&array_type);
        return FAILURE;
    }

    // Ensure array value is mutable
    if(array_value->type->kind != TYPE_KIND_POINTER){
        error_with_type = ast_type_str(&array_type);

        compiler_panicf(builder->compiler, expr->source, "Can't use operator %s on temporary value of type '%s'",
            expr->id == EXPR_ARRAY_ACCESS ? "[]" : "'at'", error_with_type);
        
        free(error_with_type);
        ast_type_free(&array_type);
        ast_type_free(&index_type);
        return FAILURE;
    }

    // Prepare different types of array values
    if(((ir_type_t*) array_value->type->extra)->kind == TYPE_KIND_FIXED_ARRAY){
        // Bitcast reference (that's to a fixed array of element) to pointer of element
        // (*)  [10] int -> *int

        assert(array_type.elements_length != 0);
        array_type.elements[0]->id = AST_ELEM_POINTER;

        ir_type_t *casted_ir_type = ir_type_pointer_to(builder->pool, ((ir_type_extra_fixed_array_t*) ((ir_type_t*) array_value->type->extra)->extra)->subtype);
        array_value = build_bitcast(builder, array_value, casted_ir_type);
    } else if(((ir_type_t*) array_value->type->extra)->kind == TYPE_KIND_STRUCTURE){
        // Keep structure value mutable
    } else if(expr_is_mutable(expr->value)){
        // Load value reference
        // (*)  *int -> *int
        array_value = build_load(builder, array_value, expr->source);
    }

    // Standard [] access for pointers
    if(ast_type_is_pointer(&array_type)){
        // Ensure the given value for the index is an integer
        if(!global_type_kind_is_integer[index_value->type->kind]){
            compiler_panic(builder->compiler, expr->index->source, "Array index value must be an integer type");
            ast_type_free(&array_type);
            ast_type_free(&index_type);
            return FAILURE;
        }

        // Access array via index
        *ir_value = build_array_access(builder, array_value, index_value, expr->source);
    } else if(ir_type_is_pointer_to(array_value->type, TYPE_KIND_STRUCTURE)){
        // If standard [] access isn't allowed on this type, then try
        // to use the __access__ management method (if the array value is a structure)
        
        ast_type_t element_pointer_type;
        *ir_value = handle_access_management(builder, array_value, index_value, expr->source, &array_type, &index_type, &element_pointer_type);
        if(*ir_value == NULL) goto array_access_not_allowed;

        // If successful in calling __access__ method, then swap the array type to the *Element type
        ast_type_free(&array_type);
        array_type = element_pointer_type;
    } else {
        // We couldn't find a way to use the [] operator on this type
        goto array_access_not_allowed;
    }
    
    if(expr->id == EXPR_ARRAY_ACCESS){
        // For [] expressions, and not 'at' expressions, we need to dereference the array type
        // before giving it back as the result type
        ast_type_dereference(&array_type);
        
        // If not requested to leave the expression mutable, dereference it
        if(!leave_mutable) *ir_value = build_load(builder, *ir_value, expr->source);
    }

    // Dispose of the AST type of the index value
    ast_type_free(&index_type);

    // Result type is the AST type of the array value (cause we modified it to be)
    if(out_expr_type != NULL) *out_expr_type = array_type;
    else ast_type_free(&array_type);
    return SUCCESS;

array_access_not_allowed:
    error_with_type = ast_type_str(&array_type);

    compiler_panicf(builder->compiler, expr->source, "Can't use operator %s on non-array type '%s'",
        expr->id == EXPR_ARRAY_ACCESS ? "[]" : "'at'", error_with_type);

    free(error_with_type);
    ast_type_free(&array_type);
    ast_type_free(&index_type);
    return FAILURE;
}

errorcode_t ir_gen_expr_cast(ir_builder_t *builder, ast_expr_cast_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ast_type_t from_type;

    // Generate the IR value for the un-casted expression
    if(ir_gen_expr(builder, expr->from, ir_value, false, &from_type)) return FAILURE;

    // Attempt to cast the value to the given type, by any means necessary
    if(!ast_types_conform(builder, ir_value, &from_type, &expr->to, CONFORM_MODE_ALL)){
        char *a_type_str = ast_type_str(&from_type);
        char *b_type_str = ast_type_str(&expr->to);
        compiler_panicf(builder->compiler, expr->source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);
        ast_type_free(&from_type);
        return FAILURE;
    }

    // Dispose of the temporary AST type of the un-casted value
    ast_type_free(&from_type);

    // Result type is the type we casted to
    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&expr->to);
    return SUCCESS;
}

errorcode_t ir_gen_expr_math(ir_builder_t *builder, ast_expr_math_t *math_expr, ir_value_t **ir_value, ast_type_t *out_expr_type,
        unsigned int instr1, unsigned int instr2, unsigned int instr3, const char *op_verb, const char *overload, bool result_is_boolean){

    // NOTE: If instr3 is INSTRUCTION_NONE then the operation will be differentiated for Integer vs. Float (using instr1 and instr2)
    //       Otherwise, the operation will be differentiated for Unsigned Integer vs. Signed Integer vs. Float (using instr1, instr2 & instr3)

    ir_pool_snapshot_t snapshot;
    ast_type_t ast_type_a, ast_type_b;
    ir_value_t *lhs, *rhs;

    // Generate IR values for left and right sides of the math operator
    if(ir_gen_expr(builder, math_expr->a, &lhs, false, &ast_type_a)) return FAILURE;
    if(ir_gen_expr(builder, math_expr->b, &rhs, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        return FAILURE;
    }

    // Conform the type of the second value to the first
    if(!ast_types_conform(builder, &rhs, &ast_type_b, &ast_type_a, CONFORM_MODE_CALCULATION)){

        // Failed to conform the values, if we have an overload function, we can use the argument
        // types for that and continue on our way
        if(overload){
            *ir_value = handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, out_expr_type, overload);

            // We successfully used the overload function
            if(*ir_value != NULL){
                ast_type_free(&ast_type_a);
                ast_type_free(&ast_type_b);
                return SUCCESS;
            }
        }

        // Otherwise, we couldn't do anything the the types given to us
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
    ir_pool_snapshot_capture(builder->pool, &snapshot);
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    instruction->a = lhs;
    instruction->b = rhs;
    instruction->id = INSTRUCTION_NONE; // For safety
    instruction->result_type = result_is_boolean ? ir_builder_bool(builder) : lhs->type;

    // Determine which instruction to use
    errorcode_t error = instr3 ? u_vs_s_vs_float_instruction(instruction, instr1, instr2, instr3) : i_vs_f_instruction(instruction, instr1, instr2);

    // We couldn't use the built-in instructions to operate on these types
    if(error){
        // Undo last instruction we attempted to build
        ir_pool_snapshot_restore(builder->pool, &snapshot);
        builder->current_block->instructions_length--;

        // Try to use the overload function if it exists
        *ir_value = overload ? handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, out_expr_type, overload) : NULL;
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);

        // If we got a value back, then we successfully used the overload function instead
        if(*ir_value != NULL) return SUCCESS;

        // Otherwise, we don't have a way to operate on these types
        compiler_panicf(builder->compiler, math_expr->source, "Can't %s those types", op_verb);
        return FAILURE;
    }

    // We successfully used the built-in instructions to perform the operation
    *ir_value = build_value_from_prev_instruction(builder);

    // Write the result type, will either be a boolean or the same type as the given arguments
    if(out_expr_type != NULL){
        if(result_is_boolean){
            ast_type_make_base(out_expr_type, strclone("bool"));
        } else {
            *out_expr_type = ast_type_clone(&ast_type_a);
        }
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

    if(ir_gen_expr(builder, ((ast_expr_math_t*) expr)->a, &a, false, &ast_type_a)) return NULL;
    if(ir_gen_expr(builder, ((ast_expr_math_t*) expr)->b, &b, false, &ast_type_b)){
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

errorcode_t ir_gen_call_function_value(ir_builder_t *builder, ast_type_t *tmp_ast_variable_type,
        ir_type_t *tmp_ir_variable_type, ast_expr_call_t *call, ir_value_t **arg_values, ast_type_t *arg_types,
        ir_value_t **inout_ir_value, ast_type_t *out_expr_type){
    // NOTE: arg_types is not freed
    // NOTE: inout_ir_value is not modified unless 'SUCCESS'ful
    // NOTE: out_expr_type is unchanged if not 'SUCCESS'ful
    // NOTE: out_expr_type may be NULL

    // Requires that tmp_ast_variable_type is a function pointer type
    if(tmp_ast_variable_type->elements_length != 1 || tmp_ast_variable_type->elements[0]->id != AST_ELEM_FUNC){
        compiler_panic(builder->compiler, tmp_ast_variable_type->source, "INTERNAL ERROR: ir_gen_call_function_value excepted function pointer AST var type");
        return FAILURE;
    }

    ast_elem_func_t *function_elem = (ast_elem_func_t*) tmp_ast_variable_type->elements[0];
    ast_type_t *ast_function_return_type = function_elem->return_type;

    ir_type_t *ir_return_type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, ast_function_return_type, &ir_return_type)){
        return FAILURE;
    }

    if(function_elem->traits & AST_FUNC_VARARG){
        if(function_elem->arity > call->arity){
            if(call->is_tentative) return ALT_FAILURE;
            compiler_panicf(builder->compiler, call->source, "Incorrect argument count (at least %d expected, %d given)", (int) function_elem->arity, (int) call->arity);
            return FAILURE;
        }
    } else if(function_elem->arity != call->arity){
        if(call->is_tentative) return ALT_FAILURE;
        compiler_panicf(builder->compiler, call->source, "Incorrect argument count (%d expected, %d given)", (int) function_elem->arity, (int) call->arity);
        return FAILURE;
    }

    for(length_t a = 0; a != function_elem->arity; a++){
        if(!ast_types_conform(builder, &arg_values[a], &arg_types[a], &function_elem->arg_types[a], CONFORM_MODE_CALL_ARGUMENTS_LOOSE)){
            if(call->is_tentative) return ALT_FAILURE;

            char *s1 = ast_type_str(&function_elem->arg_types[a]);
            char *s2 = ast_type_str(&arg_types[a]);
            compiler_panicf(builder->compiler, call->args[a]->source, "Required argument type '%s' is incompatible with type '%s'", s1, s2);
            free(s1);
            free(s2);
            return FAILURE;
        }
    }

    // Handle __pass__ management for values that need it
    if(handle_pass_management(builder, arg_values, arg_types, NULL, call->arity)) return FAILURE;

    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_call_address_t *instruction = (ir_instr_call_address_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_call_address_t));
    instruction->id = INSTRUCTION_CALL_ADDRESS;
    instruction->result_type = ir_return_type;
    instruction->address = *inout_ir_value;
    instruction->values = arg_values;
    instruction->values_length = call->arity;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    *inout_ir_value = build_value_from_prev_instruction(builder);

    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(function_elem->return_type);
    return SUCCESS;
}

successful_t ir_gen_resolve_ternay_conflict(ir_builder_t *builder, ir_value_t **a, ir_value_t **b, ast_type_t *a_type, ast_type_t *b_type,
        length_t *inout_a_basicblock, length_t *inout_b_basicblock){
    
    if(!ast_type_is_base(a_type) || !ast_type_is_base(b_type)) return UNSUCCESSFUL;
    if(!typename_is_entended_builtin_type( ((ast_elem_base_t*) a_type->elements[0])->base )) return UNSUCCESSFUL;
    if(!typename_is_entended_builtin_type( ((ast_elem_base_t*) b_type->elements[0])->base )) return UNSUCCESSFUL;
    if(global_type_kind_signs[(*a)->type->kind] != global_type_kind_signs[(*b)->type->kind]) return UNSUCCESSFUL;
    if(global_type_kind_is_float[(*a)->type->kind] != global_type_kind_is_float[(*b)->type->kind]) return UNSUCCESSFUL;
    if(global_type_kind_is_integer[(*a)->type->kind] != global_type_kind_is_integer[(*b)->type->kind]) return UNSUCCESSFUL;
    
    size_t a_size = global_type_kind_sizes_in_bits_64[(*a)->type->kind];
    size_t b_size = global_type_kind_sizes_in_bits_64[(*b)->type->kind];

    ir_value_t **smaller_value;
    ast_type_t *smaller_type;
    ast_type_t *bigger_type;

    if(a_size < b_size){
        smaller_value = a;
        smaller_type = a_type;
        bigger_type = b_type;
        build_using_basicblock(builder, *inout_a_basicblock);
    } else {
        smaller_value = b;
        smaller_type = b_type;
        bigger_type = a_type;
        build_using_basicblock(builder, *inout_b_basicblock);
    }

    successful_t successful = ast_types_conform(builder, smaller_value, smaller_type, bigger_type, CONFORM_MODE_PRIMITIVES);

    if(successful){
        *(a_size < b_size ? inout_a_basicblock : inout_b_basicblock) = builder->current_block_id;
        ast_type_free(smaller_type);
        *smaller_type = ast_type_clone(bigger_type);
    }
    
    return successful;
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
        return PRIMITIVE_UI; // Unsigned integer like values
    case TYPE_KIND_S8: case TYPE_KIND_S16: case TYPE_KIND_S32: case TYPE_KIND_S64:
        return PRIMITIVE_SI; // Signed integer like values
    case TYPE_KIND_HALF: case TYPE_KIND_FLOAT: case TYPE_KIND_DOUBLE:
        return PRIMITIVE_FP; // Floating point values
    }
    return PRIMITIVE_NA; // No catagory
}
