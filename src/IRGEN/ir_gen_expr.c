
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGE/bridge.h"
#include "UTIL/func_pair.h"
#include "BRIDGEIR/rtti.h"
#include "BRIDGE/type_table.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_module.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_qualifiers.h"
#include "IRGEN/ir_gen_stmt.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

errorcode_t ir_gen_expr(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    // NOTE: Generates an ir_value_t from an ast_expr_t
    // NOTE: Will write determined ast_type_t to 'out_expr_type' (Can use NULL to ignore)

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
        if(out_expr_type != NULL){                                                       \
            *out_expr_type = ast_type_make_base(strclone(typename));                     \
        }\
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
    
    case EXPR_BIT_AND:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_AND, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'and' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_OR:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_OR, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'or' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_XOR:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_XOR, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'xor' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_LSHIFT:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_LSHIFT, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'left shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_LGC_LSHIFT:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'logical left shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_LGC_RSHIFT:
        if(ir_gen_expr_math_ivf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'logical right shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_BIT_RSHIFT:
        if(ir_gen_expr_math_uvsvf(builder, (ast_expr_math_t*) expr, INSTRUCTION_BIT_LGC_RSHIFT, INSTRUCTION_BIT_RSHIFT, INSTRUCTION_NONE, ir_value, out_expr_type)){
            compiler_panic(builder->compiler, expr->source, "Can't perform bitwise 'right shift' on those types");
            return FAILURE;
        }
        break;
    case EXPR_NULL:
        if(out_expr_type != NULL){
            *out_expr_type = ast_type_make_base(strclone("ptr"));
        }

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
    case EXPR_INITLIST:
        if(ir_gen_expr_initlist(builder, (ast_expr_initlist_t*) expr, ir_value, out_expr_type)) return FAILURE;
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
    case EXPR_SIZEOF:
        if(ir_gen_expr_sizeof(builder, (ast_expr_sizeof_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_SIZEOF_VALUE:
        if(ir_gen_expr_sizeof_value(builder, (ast_expr_sizeof_value_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_PHANTOM:
        if(ir_gen_expr_phantom((ast_expr_phantom_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_CALL_METHOD:
        if(ir_gen_expr_call_method(builder, (ast_expr_call_method_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_NOT: case EXPR_NEGATE: case EXPR_BIT_COMPLEMENT:
        if(ir_gen_expr_unary(builder, (ast_expr_unary_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_NEW:
        if(ir_gen_expr_new(builder, (ast_expr_new_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_NEW_CSTRING:
        if(ir_gen_expr_new_cstring(builder, (ast_expr_new_cstring_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_ENUM_VALUE:
        if(ir_gen_expr_enum_value(builder, (ast_expr_enum_value_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_STATIC_ARRAY:
        if(ir_gen_expr_static_array(builder, (ast_expr_static_data_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_STATIC_STRUCT:
        if(ir_gen_expr_static_struct(builder, (ast_expr_static_data_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_TYPEINFO:
        if(ir_gen_expr_typeinfo(builder, (ast_expr_typeinfo_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_TERNARY:
        if(ir_gen_expr_ternary(builder, (ast_expr_ternary_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_PREINCREMENT: case EXPR_PREDECREMENT: case EXPR_POSTINCREMENT: case EXPR_POSTDECREMENT:
        if(ir_gen_expr_crement(builder, (ast_expr_unary_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_TOGGLE:
        if(ir_gen_expr_toggle(builder, (ast_expr_unary_t*) expr, ir_value, leave_mutable, out_expr_type)) return FAILURE;
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF:
        if(ir_gen_expr_inline_declare(builder, (ast_expr_inline_declare_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_POLYCOUNT:
        compiler_panic(builder->compiler, expr->source, "Unresolved polymorphic count variable in expression");
        return FAILURE;
    case EXPR_TYPENAMEOF:
        if(ir_gen_expr_typenameof(builder, (ast_expr_typenameof_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_EMBED:
        if(ir_gen_expr_embed(builder, (ast_expr_embed_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    case EXPR_ALIGNOF:
        if(ir_gen_expr_alignof(builder, (ast_expr_alignof_t*) expr, ir_value, out_expr_type)) return FAILURE;
        break;
    default:
        compiler_panic(builder->compiler, expr->source, "INTERNAL ERROR: Unknown expression type id in expression");
        return FAILURE;
    }

    return SUCCESS;
}

ir_value_t *ir_gen_conforming_expr(ir_builder_t *builder, ast_expr_t *ast_value, ast_type_t *to_type, unsigned int conform_mode, source_t source, const char *error_format){
    ir_value_t *ir_value;
    ast_type_t ast_type;

    if(ir_gen_expr(builder, ast_value, &ir_value, false, &ast_type)) return NULL;

    if(!ast_types_conform(builder, &ir_value, &ast_type, to_type, conform_mode)){
        strong_cstr_t a_type_str = ast_type_str(&ast_type);
        strong_cstr_t b_type_str = ast_type_str(to_type);
        compiler_panicf(builder->compiler, source, error_format, a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);
        ast_type_free(&ast_type);
        return NULL;
    }

    ast_type_free(&ast_type);
    return ir_value;
}

errorcode_t ir_gen_expr_math_ivf(ir_builder_t *builder, ast_expr_math_t *expr, unsigned int ints_instr, unsigned int floats_instr,
        ir_value_t **ir_value, ast_type_t *out_expr_type){
    
    // Create undesignated math operation
    ir_instr_math_t *instruction = ir_gen_math_operands(builder, expr, ir_value, out_expr_type);
    if(instruction == NULL) return FAILURE;

    // Designated math operation based on operand types 
    if(i_vs_f_instruction(instruction, ints_instr, floats_instr)){
        ast_type_free(out_expr_type);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_math_uvsvf(ir_builder_t *builder, ast_expr_math_t *expr, unsigned int uints_instr, unsigned int sints_instr,
        unsigned int floats_instr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    
    // Create undesignated math operation
    ir_instr_math_t *instruction = ir_gen_math_operands(builder, expr, ir_value, out_expr_type);
    if(instruction == NULL) return FAILURE;

    // Designated math operation based on operand types 
    if(u_vs_s_vs_float_instruction(instruction, uints_instr, sints_instr, floats_instr)){
        ast_type_free(out_expr_type);
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

    // Generate value for 'a' expression
    if(ir_gen_expr(builder, andor_expr->a, a, false, &ast_type_a)){
        return FAILURE;
    }

    ast_type_t bool_ast_type = ast_type_make_base(strclone("bool"));

    // Force 'a' value to be a boolean
    if(!ast_types_identical(&ast_type_a, &bool_ast_type) && !ast_types_conform(builder, a, &ast_type_a, &bool_ast_type, CONFORM_MODE_CALCULATION)){
        char *a_type_str = ast_type_str(&ast_type_a);
        compiler_panicf(builder->compiler, andor_expr->source, "Failed to convert value of type '%s' to type 'bool'", a_type_str);
        free(a_type_str);
        ast_type_free(&ast_type_a);
        ast_type_free(&bool_ast_type);
        return FAILURE;
    }

    *landing_a_block_id = builder->current_block_id;
    *landing_more_block_id = build_basicblock(builder);
    build_using_basicblock(builder, *landing_more_block_id);

    // Generate value for 'b' expression
    if(ir_gen_expr(builder, ((ast_expr_math_t*) andor_expr)->b, b, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        ast_type_free(&bool_ast_type);
        return FAILURE;
    }

    // Force 'b' value to be a boolean
    if(!ast_types_identical(&ast_type_b, &bool_ast_type) && !ast_types_conform(builder, b, &ast_type_b, &bool_ast_type, CONFORM_MODE_CALCULATION)){
        char *b_type_str = ast_type_str(&ast_type_b);
        compiler_panicf(builder->compiler, andor_expr->source, "Failed to convert value of type '%s' to type 'bool'", b_type_str);
        free(b_type_str);
        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
        ast_type_free(&bool_ast_type);
        return FAILURE;
    }

    ast_type_free(&ast_type_a);
    ast_type_free(&ast_type_b);

    *landing_b_block_id = builder->current_block_id;

    if(out_expr_type != NULL){
        *out_expr_type = bool_ast_type;
    } else {
        ast_type_free(&bool_ast_type);
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_str(ir_builder_t *builder, ast_expr_str_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    *ir_value = build_literal_str(builder, expr->array, expr->length);
    if(*ir_value == NULL) return FAILURE;
    
    // Has type of 'String'
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("String"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_cstr(ir_builder_t *builder, ast_expr_cstr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    *ir_value = build_literal_cstr(builder, expr->value);

    // Has type of '*ubyte'
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base_ptr(strclone("ubyte"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_variable(ir_builder_t *builder, ast_expr_variable_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    char *variable_name = expr->name;
    bridge_var_t *variable = bridge_scope_find_var(builder->scope, variable_name);

    // Found variable in nearby scope
    if(variable){
        if(out_expr_type != NULL){
            *out_expr_type = ast_type_clone(variable->ast_type);
        }

        ir_type_t *ir_ptr_type = ir_type_make_pointer_to(builder->pool, variable->ir_type);

        // Variable-Pointer instruction to get pointer to stack variable
        *ir_value = build_varptr(builder, ir_ptr_type, variable);

        // Load if the variable is a reference
        if(variable->traits & BRIDGE_VAR_REFERENCE){
            *ir_value = build_load(builder, *ir_value, expr->source);
        }

        // Unless requested to leave the expression mutable, dereference it
        if(!leave_mutable){
            *ir_value = build_load(builder, *ir_value, expr->source);
        }

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
        *ir_value = build_gvarptr(builder, ir_type_make_pointer_to(builder->pool, ir_global->type), global_variable_index);
        
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
    ir_pool_snapshot_t pool_snapshot;
    instructions_snapshot_t instructions_snapshot;

    // Take snapshot of construction state,
    // so that if this call ends up to be a no-op,
    // we can reset back as if nothing happened
    ir_pool_snapshot_capture(builder->pool, &pool_snapshot);
    instructions_snapshot_capture(builder, &instructions_snapshot);

    ir_value_t **arg_values;
    ast_type_t *arg_types;
    ast_t *ast = &builder->object->ast;

    if(ir_gen_arguments(builder, expr->args, expr->arity, &arg_values, &arg_types)){
        return FAILURE;
    }

    // TODO: Clean up this code
    // Temporary values used to hold type of variable/global-variable
    ast_type_t *tmp_ast_variable_type;
    ir_type_t *tmp_ir_variable_type;

    // Check for variable of name in nearby scope
    bridge_var_t *var = bridge_scope_find_var(builder->scope, expr->name);
    bool is_var_function_like = var && ast_type_is_func(var->ast_type);

    // Found variable of name in nearby scope
    if(is_var_function_like){
        // Get IR type of function from variable
        tmp_ir_variable_type = ir_type_make_pointer_to(builder->pool, var->ir_type);

        if(expr->gives.elements_length != 0){
            compiler_panicf(builder->compiler, expr->source, "Can't specify return type when calling function variable");
            ast_types_free_fully(arg_types, expr->arity);
            return FAILURE;
        }

        // Load function pointer value from variable
        *ir_value = build_varptr(builder, tmp_ir_variable_type, var);
        *ir_value = build_load(builder, *ir_value, expr->source);

        // Call function pointer value
        errorcode_t error = ir_gen_call_function_value(builder, var->ast_type, expr, arg_values, arg_types, ir_value, out_expr_type);
        ast_types_free_fully(arg_types, expr->arity);

        // Propagate failure if failed to call function pointer value
        if(error == FAILURE) return FAILURE;

        // The function pointer couldn't be called, but the call is tentative, so we pretend like it didn't happen
        if(error == ALT_FAILURE){
            if(out_expr_type != NULL){
                *out_expr_type = ast_type_make_base(strclone("void"));
            }

            return SUCCESS;
        }
        
        // Otherwise, we successful called the function pointer value from a nearby scoped variable
        return SUCCESS;
    }

    // If there doesn't exist a nearby scoped variable with the same name, look for function
    optional_func_pair_t result;
    length_t arg_arity = expr->arity;
    errorcode_t error = ir_gen_find_func_conforming(builder, expr->name, &arg_values, &arg_types, &arg_arity, &expr->gives, expr->no_user_casts, expr->source, &result);

    // Propagate failure if something went wrong during the search
    if(error == ALT_FAILURE){
        ast_types_free_fully(arg_types, arg_arity);
        return FAILURE;
    }

    // Found function that fits given name and arguments
    if(error == SUCCESS){
        if(!result.has){
            // This function call is a no-op
            instructions_snapshot_restore(builder, &instructions_snapshot);
            ir_pool_snapshot_restore(builder->pool, &pool_snapshot);

            if(!expr->is_tentative){
                compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, arg_types, expr->arity, &expr->gives, false);
                ast_types_free_fully(arg_types, arg_arity);
                return FAILURE;
            }

            if(out_expr_type != NULL){
                *out_expr_type = ast_type_make_base(strclone("void"));
            }

            ast_types_free_fully(arg_types, arg_arity);
            return SUCCESS;
        }

        func_pair_t pair = result.value;

        trait_t ast_func_traits;
        length_t ast_func_arity;
        trait_t *arg_type_traits;
        ast_type_t ast_func_return_type;

        {
            ast_func_t *ast_func = &ast->funcs[pair.ast_func_id];
            ast_func_traits = ast_func->traits;
            ast_func_arity = ast_func->arity;
            arg_type_traits = ast_func->arg_type_traits;
            ast_func_return_type = ast_func->return_type;
        }

        // If requires implicit, fail if conforming function isn't marked as implicit
        if(expr->only_implicit && expr->is_tentative && !(ast_func_traits & AST_FUNC_IMPLICIT)){
            if(out_expr_type != NULL){
                *out_expr_type = ast_type_make_base(strclone("void"));
            }

            ast_types_free_fully(arg_types, arg_arity);
            return SUCCESS;
        }

        if(ensure_not_violating_disallow(builder->compiler, expr->source, &ast->funcs[pair.ast_func_id])
        || ensure_not_violating_no_discard(builder->compiler, expr->no_discard, expr->source, &ast->funcs[pair.ast_func_id])){
            ast_types_free_fully(arg_types, arg_arity);
            return ALT_FAILURE;
        }

        if(ir_gen_expr_call_procedure_handle_pass_management(builder, arg_arity, arg_values, arg_types, ast_func_traits, arg_type_traits, ast_func_arity)){
            ast_types_free_fully(arg_types, arg_arity);
            return FAILURE;
        }

        // Prepare for potential stack allocation
        ir_value_t *stack_pointer = NULL;

        // Remember arity before variadic argument packing
        length_t unpacked_arity = arg_arity;

        // Handle variadic argument packing if applicable
        if(ir_gen_expr_call_procedure_handle_variadic_packing(builder, &arg_values, arg_types, &arg_arity, &pair, &stack_pointer, expr->source)){
            ast_types_free_fully(arg_types, unpacked_arity);
            return FAILURE;
        }
        
        // Free AST types of the expressions given for the arguments
        ast_types_free_fully(arg_types, unpacked_arity);
        
        // Call the actual function and store resulting value from call expression if requested
        ir_type_t *result_type = builder->object->ir_module.funcs.funcs[pair.ir_func_id].return_type;

        if(ir_value){
            *ir_value = build_call(builder, pair.ir_func_id, result_type, arg_values, arg_arity);
        } else {
            build_call_ignore_result(builder, pair.ir_func_id, result_type, arg_values, arg_arity);
        }

        // Restore the stack if we allocated memory on it
        if(stack_pointer) build_stack_restore(builder, stack_pointer);

        // Give back return type of the called function
        if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&ast_func_return_type);
        return SUCCESS;
    }

    // If no function exists, look for global
    maybe_index_t global_variable_index = ast_find_global(ast->globals, ast->globals_length, expr->name);

    if(global_variable_index != -1){
        tmp_ast_variable_type = &ast->globals[global_variable_index].type;

        // Get IR type of pointer to the variable type
        tmp_ir_variable_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        tmp_ir_variable_type->kind = TYPE_KIND_POINTER;

        if(expr->gives.elements_length != 0){
            compiler_panicf(builder->compiler, expr->source, "Can't specify return type when calling function via global variable");
            ast_types_free_fully(arg_types, arg_arity);
            return FAILURE;
        }

        if(ir_gen_resolve_type(builder->compiler, builder->object, tmp_ast_variable_type, (ir_type_t**) &tmp_ir_variable_type->extra)){
            ast_types_free_fully(arg_types, arg_arity);
            return FAILURE;
        }

        // Ensure the type of the global variable is a function pointer
        if(tmp_ast_variable_type->elements_length != 1 || tmp_ast_variable_type->elements[0]->id != AST_ELEM_FUNC){
            // The type of the global variable isn't a function pointer
            // Ignore failure if the call expression is tentative
            if(expr->is_tentative){
                if(out_expr_type != NULL){
                    *out_expr_type = ast_type_make_base(strclone("void"));
                }

                ast_types_free_fully(arg_types, arg_arity);
                return SUCCESS;
            }

            // Otherwise print error message
            strong_cstr_t s = ast_type_str(tmp_ast_variable_type);
            compiler_panicf(builder->compiler, expr->source, "Can't call value of non function type '%s'", s);
            ast_types_free_fully(arg_types, arg_arity);
            free(s);
            return FAILURE;
        }

        // Load the value of the function pointer from the global variable
        // DANGEROUS: Using AST global variable index as IR global variable index
        *ir_value = build_load(builder, build_gvarptr(builder, tmp_ir_variable_type, global_variable_index), expr->source);

        // Call the function pointer value
        errorcode_t error = ir_gen_call_function_value(builder, tmp_ast_variable_type, expr, arg_values, arg_types, ir_value, out_expr_type);
        ast_types_free_fully(arg_types, arg_arity);

        // Propagate failure to call function pointer value
        if(error == FAILURE) return FAILURE;

        // If calling the function pointer value failed, but the call was tentative, then ignore the failure
        if(error == ALT_FAILURE){
            if(out_expr_type != NULL){
                *out_expr_type = ast_type_make_base(strclone("void"));
            }

            return SUCCESS;
        }

        return SUCCESS;
    }
    
    // Otherwise no function or variable with a matching name was found
    // ...

    // If the call expression was tentative, then ignore the failure
    if(expr->is_tentative){
        if(out_expr_type != NULL){
            *out_expr_type = ast_type_make_base(strclone("void"));
        }

        ast_types_free_fully(arg_types, arg_arity);
        return SUCCESS;
    }

    // Otherwise, print an error message
    compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, arg_types, expr->arity, &expr->gives, false);
    ast_types_free_fully(arg_types, arg_arity);
    return FAILURE;
}

errorcode_t ir_gen_arguments(ir_builder_t *builder, ast_expr_t **args, length_t arity, ir_value_t ***out_arg_values, ast_type_t **out_arg_types){
    // Setup for generating call
    ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * arity);
    ast_type_t *arg_types = malloc(sizeof(ast_type_t) * arity);

    // Generate values for function arguments
    for(length_t i = 0; i != arity; i++){
        if(ir_gen_expr(builder, args[i], &arg_values[i], false, &arg_types[i])){
            ast_types_free_fully(arg_types, i);
            return FAILURE;
        }

        if(ast_type_is_void(&arg_types[i])){
            compiler_panicf(builder->compiler, args[i]->source, "Cannot pass 'void' to function");
            ast_types_free_fully(arg_types, i);
            return FAILURE;
        }
    }

    *out_arg_values = arg_values;
    *out_arg_types = arg_types;
    return SUCCESS;
}

errorcode_t ir_gen_expr_call_procedure_handle_pass_management(
    ir_builder_t *builder,
    length_t arity,
    ir_value_t **arg_values,
    ast_type_t *final_arg_types,
    trait_t target_traits,
    trait_t *target_arg_type_traits,
    length_t arity_without_variadic_arguments
){
    // Handle __pass__ calls for argument values being passed
    length_t extra_argument_count = arity - arity_without_variadic_arguments;

    if(extra_argument_count != 0 && (target_traits & AST_FUNC_VARARG || target_traits & AST_FUNC_VARIADIC)){
        // Additional argument traits are needed for the extra optional arguments
        trait_t padded_type_traits[arity];
        
        // Copy type traits for required arguments with the specified argument traits
        memcpy(padded_type_traits, target_arg_type_traits, sizeof(trait_t) * arity_without_variadic_arguments);

        // Fill in type traits for additional optional arguments with regular argument traits
        memset(&padded_type_traits[arity_without_variadic_arguments], TRAIT_NONE, sizeof(trait_t) * extra_argument_count);
        
        // Use padded argument traits for functions with variable arity
        if(handle_pass_management(builder, arg_values, final_arg_types, padded_type_traits, arity)) return FAILURE;
    } else {
        // Otherwise no padding is required,
        if(handle_pass_management(builder, arg_values, final_arg_types, target_arg_type_traits, arity)) return FAILURE;
    }

    // Successfully called __pass__ for the arguments that needed it
    return SUCCESS;
}

errorcode_t ir_gen_expr_call_procedure_handle_variadic_packing(ir_builder_t *builder, ir_value_t ***arg_values, ast_type_t *arg_types,
        length_t *arity, func_pair_t *pair, ir_value_t **stack_pointer, source_t source_on_failure){

    // Pack variadic arguments into variadic array if applicable
    // -----
    
    ast_t *ast = &builder->object->ast;
    trait_t ast_func_traits = ast->funcs[pair->ast_func_id].traits;
    
    // Don't pack arguments unless function is variadic
    if(!(ast_func_traits & AST_FUNC_VARIADIC)) return SUCCESS;

    // Everything after the natural arity of the function will be packed inside the variadic argument list
    length_t natural_arity = ast->funcs[pair->ast_func_id].arity;

    // Calculate number of variadic arguments
    length_t variadic_count = *arity - natural_arity;

    // Do __builtin_warn_bad_printf_format if applicable
    if(ast_func_traits & AST_FUNC_WARN_BAD_PRINTF_FORMAT
    && ir_gen_do_builtin_warn_bad_printf_format(builder, *pair, arg_types, *arg_values, source_on_failure, variadic_count)){
        return FAILURE;
    }

    // Save stack pointer, unless its already saved
    if(*stack_pointer == NULL) *stack_pointer = build_stack_save(builder);
    
    ir_value_t *raw_types_array = NULL;
    ir_type_t *pAnyType_ir_type = NULL;

    // Obtain IR type of '*AnyType' type if typeinfo is enabled
    if(!(builder->compiler->traits & COMPILER_NO_TYPEINFO)){
        ast_type_t any_type = ast_type_make_base_ptr(strclone("AnyType"));

        if(ir_gen_resolve_type(builder->compiler, builder->object, &any_type, &pAnyType_ir_type)){
            ast_type_free(&any_type);
            return FAILURE;
        }

        ast_type_free(&any_type);
    }

    // Calculate total size of raw data array in bytes
    ir_type_t *usize_type = ir_builder_usize(builder);
    ir_value_t *bytes = NULL;
    
    if(variadic_count != 0){
        bytes = build_const_sizeof(builder->pool, usize_type, (*arg_values)[natural_arity]->type);
    } else {
        bytes = build_literal_usize(builder->pool, 0);
    }

    for(length_t i = 1; i < variadic_count; i++){
        ir_type_t *ir_type = (*arg_values)[natural_arity + i]->type;
        
        if(ir_type->kind == TYPE_KIND_VOID){
            compiler_panicf(builder->compiler, source_on_failure, "Cannot pass 'void' in variadic arguments");
            return FAILURE;
        }

        ir_value_t *more_bytes = build_const_sizeof(builder->pool, usize_type, ir_type);
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
        raw_data_array = build_bitcast(builder, raw_data_array, ir_type_make_pointer_to(builder->pool, ubyte_type));
    } else {
        raw_data_array = build_null_pointer(builder->pool);
    }

    // Use iterative value 'current_offset' to store variadic arguments values into the raw data array
    ir_value_t *current_offset = build_literal_usize(builder->pool, 0);

    for(length_t i = 0; i < variadic_count; i++){
        ir_value_t *arg = build_array_access(builder, raw_data_array, current_offset, source_on_failure);
        arg = build_bitcast(builder, arg, ir_type_make_pointer_to(builder->pool, (*arg_values)[natural_arity + i]->type));
        build_store(builder, (*arg_values)[natural_arity + i], arg, source_on_failure);

        // Move iterative offset 'current_offset' along by the size of the type we just wrote
        if(i + 1 < variadic_count){
            current_offset = build_const_add(builder->pool, current_offset, build_const_sizeof(builder->pool, usize_type, (*arg_values)[natural_arity + i]->type));
        }
    }
    
    // Create raw types array of '*AnyType' values if runtime type info is enabled
    if(pAnyType_ir_type && variadic_count != 0){
        raw_types_array = build_alloc_array(builder, pAnyType_ir_type, build_literal_usize(builder->pool, variadic_count));
        raw_types_array = build_bitcast(builder, raw_types_array, ir_type_make_pointer_to(builder->pool, pAnyType_ir_type));

        for(length_t i = 0; i < variadic_count; i++){
            ir_value_t *arg_type = build_array_access(builder, raw_types_array, build_literal_usize(builder->pool, i), source_on_failure);
            build_store(builder, rtti_for(builder, &arg_types[natural_arity + i], source_on_failure), arg_type, source_on_failure);
        }
    }
    
    // If runtime type info is disabled, then just set 'raw_types_array' to a null pointer
    if(raw_types_array == NULL) raw_types_array = build_null_pointer_of_type(builder->pool, builder->ptr_type);

    // '__variadic_array__' arguments:
    // pointer = raw_data_array
    // bytes = bytes
    // length = variadic_count
    // maybe_types = raw_types_array

    ir_value_t **variadic_array_function_arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 4);
    variadic_array_function_arguments[0] = build_bitcast(builder, raw_data_array, builder->ptr_type);
    variadic_array_function_arguments[1] = bytes;
    variadic_array_function_arguments[2] = build_literal_usize(builder->pool, variadic_count);
    variadic_array_function_arguments[3] = build_bitcast(builder, raw_types_array, builder->ptr_type);
    
    // Create variadic array value using the special function '__variadic_array__'
    func_id_t ir_func_id = builder->object->ir_module.common.variadic_ir_func_id;
    ir_type_t *result_type = builder->object->ir_module.common.ir_variadic_array;
    ir_value_t *returned_value = build_call(builder, ir_func_id, result_type, variadic_array_function_arguments, 4);

    // Make space for variadic array argument
    if(variadic_count == 0){
        ir_value_t **new_args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * (natural_arity + 1));

        // Copy given argument values into new array
        // NOTE: We are abandoning the old arg_values array
        memcpy(new_args, *arg_values, sizeof(ir_value_t*) * (natural_arity + 1));
        (*arg_values) = new_args;
    }

    // Replace argument values after regular arguments with single variadic array argument value
    (*arg_values)[natural_arity] = returned_value;
    *arity = natural_arity + 1;
    return SUCCESS;
}

errorcode_t ir_gen_expr_member(ir_builder_t *builder, ast_expr_member_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ir_value_t *value_of_composite;
    ast_type_t ast_type_of_composite;

    // This expression should be able to be mutable (Checked during parsing)
    if(ir_gen_expr(builder, expr->value, &value_of_composite, true, &ast_type_of_composite)) return FAILURE;

    // Ensure that IR value we get back is a pointer, cause if it isn't, then it isn't mutable
    if(value_of_composite->type->kind != TYPE_KIND_POINTER){
        char *given_type = ast_type_str(&ast_type_of_composite);
        compiler_panicf(builder->compiler, expr->source, "Can't use member operator '.' on temporary value of type '%s'", given_type);
        free(given_type);
        ast_type_free(&ast_type_of_composite);
        return FAILURE;
    }

    // Auto-deference non-ptr pointer types that don't point to other pointers
    if(ast_type_is_pointer(&ast_type_of_composite) && ast_type_of_composite.elements[1]->id != AST_ELEM_POINTER){
        ast_type_dereference(&ast_type_of_composite);
        
        // Defererence one layer if struct value is **StructType
        if(expr_is_mutable(expr->value)) value_of_composite = build_load(builder, value_of_composite, expr->source);
    }

    ir_type_t *type;
    ir_field_info_t field_info;

    if(ir_gen_get_field_info(builder->compiler, builder->object, expr->member, expr->source, &ast_type_of_composite, &field_info)
    || ir_gen_resolve_type(builder->compiler, builder->object, &ast_type_of_composite, &type)){
        ast_type_free(&ast_type_of_composite);
        return FAILURE;
    }

    *ir_value = value_of_composite;
    
    for(length_t i = 0; i < AST_LAYOUT_MAX_DEPTH && field_info.endpoint.indices[i] != AST_LAYOUT_ENDPOINT_END_INDEX; i++){
        // Get IR type for waypoint
        // Assumes IR type is a composite IR type
        ir_type_extra_composite_t *composite_extra = (ir_type_extra_composite_t*) type->extra;
        ir_type_t *field_type = composite_extra->subtypes[field_info.endpoint.indices[i]];

        ast_layout_waypoint_t waypoint = field_info.path.waypoints[i];

        switch(waypoint.kind){
        case AST_LAYOUT_WAYPOINT_BITCAST:
            *ir_value = build_bitcast(builder, *ir_value, ir_type_make_pointer_to(builder->pool, field_type));
            break;
        case AST_LAYOUT_WAYPOINT_OFFSET:
            *ir_value = build_member(builder, *ir_value, waypoint.index, ir_type_make_pointer_to(builder->pool, field_type), expr->source);
            break;
        default:
            internalerrorprintf("ir_gen_expr_member() - Unrecognized waypoint kind\n");
            ast_type_free(&field_info.ast_type);
            ast_type_free(&ast_type_of_composite);
            return FAILURE;
        }

        type = field_type;
    }

    if(type->kind == TYPE_KIND_POINTER){
        // Bitcast the pointer that we got from inside the composite,
        // since we store them as 'ptr' inside of composites.
        // Because of this, we have to bitcast it to the proper type
        // when we access ir via member access

        *ir_value = build_bitcast(builder, *ir_value, ir_type_make_pointer_to(builder->pool, field_info.ir_type));
    }

    if(out_expr_type){
        *out_expr_type = field_info.ast_type;
    } else {
        ast_type_free(&field_info.ast_type);
    }

    // If not requested to leave the expression mutable, dereference it
    if(!leave_mutable) *ir_value = build_load(builder, *ir_value, expr->source);

    ast_type_free(&ast_type_of_composite);
    return SUCCESS;
}

errorcode_t ir_gen_get_field_info(compiler_t *compiler, object_t *object, weak_cstr_t member, source_t source, ast_type_t *ast_type_of_composite, ir_field_info_t *out_field_info){
    // TODO: CLEANUP: Clean up the code in this function

    ast_elem_t *elem = ast_type_of_composite->elements[0];
    type_table_t *type_table = object->ast.type_table;

    if(elem->id == AST_ELEM_BASE){
        // Non-polymorphic composite type
        weak_cstr_t composite_name = ((ast_elem_base_t*) elem)->base;
        ast_composite_t *target = ast_composite_find_exact(&object->ast, composite_name);

        // If we didn't find the composite, show an error message and return failure
        if(target == NULL){
            weak_cstr_t message_format = typename_is_extended_builtin_type(composite_name) ? "Can't use member operator on built-in type '%s'" : "INTERNAL ERROR: Failed to find composite '%s' that should exist";
            compiler_panicf(compiler, source, message_format, composite_name);
            return FAILURE;
        }

        // Find the field of the structure by name
        if(!ast_composite_find_exact_field(target, member, &out_field_info->endpoint, &out_field_info->path)){
            strong_cstr_t s = ast_type_str(ast_type_of_composite);
            compiler_panicf(compiler, source, "Field '%s' doesn't exist in %s '%s'", member, ast_layout_kind_name(target->layout.kind), s);
            free(s);
            return FAILURE;
        }

        ast_type_t *field_type = ast_layout_skeleton_get_type(&target->layout.skeleton, out_field_info->endpoint);

        if(field_type == NULL){
            internalerrorprintf("ir_gen_get_field_info() - Failed to get AST type for endpoint of field '%s'\n", member);
            return FAILURE;
        }

        // Resolve AST field type to IR field type
        if(ir_gen_resolve_type(compiler, object, field_type, &out_field_info->ir_type)) return FAILURE;

        out_field_info->ast_type = ast_type_clone(field_type);
        return SUCCESS;
    }

    if(elem->id == AST_ELEM_GENERIC_BASE){
        // Polymorphic composite type
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) elem;

        weak_cstr_t composite_name = generic_base->name;
        ast_poly_composite_t *template = ast_poly_composite_find_exact(&object->ast, composite_name);

        // Find the polymorphic structure
        if(template == NULL){
            compiler_panicf(compiler, source, "INTERNAL ERROR: Failed to find polymorphic composite '%s' that should exist", composite_name);
            return FAILURE;
        }

        // Find the field of the polymorphic structure by name
        if(!ast_composite_find_exact_field((ast_composite_t*) template, member, &out_field_info->endpoint, &out_field_info->path)){
            strong_cstr_t s = ast_type_str(ast_type_of_composite);
            compiler_panicf(compiler, source, "Field '%s' doesn't exist in %s '%s'", member, ast_layout_kind_name(template->layout.kind), s);
            free(s);
            return FAILURE;
        }

        ast_type_t *maybe_polymorphic_field_type = ast_layout_skeleton_get_type(&template->layout.skeleton, out_field_info->endpoint);
        if(maybe_polymorphic_field_type == NULL) return FAILURE;

        // Create a catalog of all the known polymorphic type substitutions
        ast_poly_catalog_t catalog;
        ast_poly_catalog_init(&catalog);

        // Ensure that the number of parameters given for the generic base is the same as expected for the polymorphic structure
        if(template->generics_length != generic_base->generics_length){
            compiler_panicf(compiler, source, "Polymorphic %s '%s' type parameter length mismatch!", ast_layout_kind_name(template->layout.kind), generic_base->name);
            ast_poly_catalog_free(&catalog);
            return FAILURE;
        }

        // Add each entry given for the generic base structure type to the list of known polymorphic type substitutions
        for(length_t i = 0; i != template->generics_length; i++){
            ast_poly_catalog_add_type(&catalog, template->generics[i], &generic_base->generics[i]);
        }

        // Get the AST field type of the target field by index and resolve any polymorphs
        ast_type_t field_type;
        if(ast_resolve_type_polymorphs(compiler, type_table, &catalog, maybe_polymorphic_field_type, &field_type)){
            ast_poly_catalog_free(&catalog);
            return FAILURE;
        }

        // Resolve AST field type to IR field type
        if(ir_gen_resolve_type(compiler, object, &field_type, &out_field_info->ir_type)){
            ast_type_free(&field_type);
            ast_poly_catalog_free(&catalog);
            return FAILURE;
        }

        out_field_info->ast_type = field_type;

        // Dispose of the polymorphic substitutions catalog
        ast_poly_catalog_free(&catalog);
        return SUCCESS;
    }

    if(elem->id == AST_ELEM_LAYOUT){
        // Anonymous composite type

        ast_elem_layout_t *layout_elem = (ast_elem_layout_t*) elem;
        ast_layout_t *layout = &layout_elem->layout;

        if(!ast_field_map_find(&layout->field_map, member, &out_field_info->endpoint)
        || !ast_layout_get_path(layout, out_field_info->endpoint, &out_field_info->path)){
            strong_cstr_t s = ast_type_str(ast_type_of_composite);
            compiler_panicf(compiler, source, "Field '%s' doesn't exist in %s '%s'", member, ast_layout_kind_name(layout->kind), s);
            free(s);
            return FAILURE;
        }

        ast_type_t *field_type = ast_layout_skeleton_get_type(&layout->skeleton, out_field_info->endpoint);

        if(field_type == NULL){
            internalerrorprintf("ir_gen_get_field_info() - Failed to get AST type for invalid endpoint (of anonymous composite)\n");
            return FAILURE;
        }

        // Resolve AST field type to IR field type
        if(ir_gen_resolve_type(compiler, object, field_type, &out_field_info->ir_type)) return FAILURE;

        out_field_info->ast_type = ast_type_clone(field_type);
        return SUCCESS;
    }

    // Otherwise, we got a value that isn't a structure type
    strong_cstr_t s = ast_type_str(ast_type_of_composite);
    compiler_panicf(compiler, source, "Can't use member operator on non-composite type '%s'", s);
    free(s);
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

errorcode_t ir_gen_expr_initlist(ir_builder_t *builder, ast_expr_initlist_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ast_type_t master_type;
    ast_type_t temporary_type;
    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * expr->length);

    if(expr->length == 0){
        compiler_panicf(builder->compiler, expr->source, "Empty initializer lists are not supported");
        return FAILURE;
    }

    memset(&master_type, 0, sizeof(ast_type_t));

    // Generate IR values for every element
    for(length_t i = 0; i != expr->length; i++){
        if(ir_gen_expr(builder, expr->elements[i], &values[i], false, &temporary_type)){
            if(master_type.elements_length != 0) ast_type_free(&master_type);
            return FAILURE;
        }

        if(i == 0){
            // If first element in list, then its type will be the master type
            master_type = temporary_type;
        } else {
            // Otherwise, conform the IR value to the master type
            if(!ast_types_conform(builder, &values[i], &temporary_type, &master_type, CONFORM_MODE_ALL)){
                char *master_string = ast_type_str(&master_type);
                char *element_string = ast_type_str(&temporary_type);

                compiler_panicf(builder->compiler, expr->elements[i]->source, "%d%s element of initializer list cannot be converted from type '%s' to '%s'",
                    i + 1, get_numeric_ending(i + 1), element_string, master_string);
                
                free(element_string);
                free(master_string);

                ast_type_free(&master_type);
                ast_type_free(&temporary_type);
                return FAILURE;
            }
            ast_type_free(&temporary_type);
        }
    }

    // Allocate memory on stack
    ir_value_t *memory = build_alloc_array(builder, values[0]->type, build_literal_usize(builder->pool, expr->length));

    // Store POD element values
    // TODO: Maybe don't unroll loop for large initializer lists
    for(length_t i = 0; i != expr->length; i++){
        build_store(builder, values[i], build_array_access(builder, memory, build_literal_usize(builder->pool, i), expr->source), expr->source);
    }

    // Create AST phantom value for memory pointer
    ast_type_prepend_ptr(&master_type);

    ast_expr_phantom_t phantom_memory_value = (ast_expr_phantom_t){
        .id = EXPR_PHANTOM,
        .ir_value = memory,
        .source = NULL_SOURCE,
        .is_mutable = false,
        .type = master_type,
    };

    // Create AST phantom value for memory length
    ast_expr_phantom_t phantom_length_value = (ast_expr_phantom_t){
        .id = EXPR_PHANTOM,
        .ir_value = build_literal_usize(builder->pool, expr->length),
        .source = NULL_SOURCE,
        .is_mutable = false,
        .type = ast_type_make_base(strclone("usize")),
    };

    // Setup AST values to call special function
    ast_expr_t *args[2] = {
        (ast_expr_t*) &phantom_memory_value,
        (ast_expr_t*) &phantom_length_value,
    };

    // DANGEROUS: Using stack-allocated argument expressions,
    // we must reset 'arity' to zero and 'args' to NULL before
    // freeing it
    // DANGEROUS: Using constant string for strong_cstr_t name,
    // we must reset 'name' to be NULL before freeing it
    ast_expr_call_t as_call;
    ast_expr_create_call_in_place(&as_call, "__initializer_list__", 2, args, true, NULL, NULL_SOURCE);

    ir_value_t *initializer_list;
    errorcode_t error = ir_gen_expr(builder, (ast_expr_t*) &as_call, &initializer_list, false, &temporary_type);

    as_call.name = NULL;
    as_call.args = NULL;
    as_call.arity = 0;
    ast_expr_free((ast_expr_t*) &as_call);

    ast_type_free(&phantom_memory_value.type);
    ast_type_free(&phantom_length_value.type);

    // Ensure tentative call was successful
    if(error) return FAILURE;

    if(ast_type_is_void(&temporary_type)){
        compiler_panicf(builder->compiler, expr->source, "__initializer_list__ must be defined in order to use initializer lists");
        printf("\nTry importing '%s/InitializerList.adept'\n", ADEPT_VERSION_STRING);
        ast_type_free(&temporary_type);
        return FAILURE;
    }
    
    // Result value is the received initializer list
    *ir_value = initializer_list;

    // Result type is received AST type of initializer list value
    if(out_expr_type){
        *out_expr_type = temporary_type;
    } else {
        ast_type_free(&temporary_type);
    }
    return SUCCESS;
}

errorcode_t ir_gen_expr_func_addr(ir_builder_t *builder, ast_expr_func_addr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    func_pair_t pair;

    // No arguments given to match against
    if(expr->has_match_args == false){
        bool is_unique;

        if(ir_gen_find_func_named(builder->object, expr->name, &is_unique, &pair, false)){
            // If nothing exists and the lookup is tentative, fail tentatively
            if(expr->tentative) goto fail_tentatively;

            // Otherwise, we failed to find a function we were expecting to find
            compiler_panicf(builder->compiler, expr->source, "Undeclared function '%s'", expr->name);
            return FAILURE;
        }

        // Ensure found function is not disallowed
        if(ensure_not_violating_disallow(builder->compiler, expr->source, &builder->object->ast.funcs[pair.ast_func_id])){
            return FAILURE;
        }
        
        // Warn of multiple possibilities if the resulting function isn't unique in its name
        if(!is_unique && compiler_warnf(builder->compiler, expr->source, "Multiple functions named '%s', using the first of them", expr->name)){
            return FAILURE;
        }
    }
    
    // Otherwise we have arguments we can try to match against
    else {
        optional_func_pair_t result;

        if(ir_gen_find_func_regular(builder->compiler, builder->object, expr->name, expr->match_args, expr->match_args_length, TRAIT_NONE, TRAIT_NONE, expr->source, &result)
        || !result.has){
            // If nothing exists and the lookup is tentative, fail tentatively
            if(expr->tentative) goto fail_tentatively;

            // For '__defer__' that doesn't exist, return no-op function for backwards compatibility
            if(streq(expr->name, "__defer__") && expr->match_args_length == 1 && ast_type_is_pointer(&expr->match_args[0])){
                return ir_gen_expr_func_addr_noop_result_for_defer(builder, &expr->match_args[0], expr->source, ir_value, out_expr_type);
            } else {
                // Otherwise, we failed to find a function we were expecting to find
                compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, expr->match_args, expr->match_args_length, NULL, false);
                return FAILURE;
            }
        }

        pair = result.value;

        // Ensure found function is not disallowed
        if(ensure_not_violating_disallow(builder->compiler, expr->source, &builder->object->ast.funcs[pair.ast_func_id])){
            return FAILURE;
        }
    }

    ast_t *ast = &builder->object->ast;
    ir_module_t *module = &builder->object->ir_module;
    trait_t ast_func_traits = TRAIT_NONE;

    // Create the IR function pointer type
    ir_type_t *ir_funcptr_type;

    {
        ast_func_t *ast_func = &ast->funcs[pair.ast_func_id];
        ir_func_t *ir_func = &module->funcs.funcs[pair.ir_func_id];

        ast_func_traits = ast_func->traits;

        length_t arity = ast_func->arity;
        ir_type_t **arg_types = ir_func->argument_types;
        ir_type_t *return_type = ast_func->traits & AST_FUNC_MAIN ? ir_type_make(builder->pool, TYPE_KIND_S32, NULL) : ir_func->return_type;

        ir_funcptr_type = ir_type_make_function_pointer(builder->pool, arg_types, arity, return_type, expr->traits);
    }

    // If the function is only referenced by C-mangled name, then get its address by it
    // Otherwise, just get its function address like normal using its IR function id
    if(ast_func_traits & AST_FUNC_FOREIGN || ast_func_traits & AST_FUNC_MAIN){
        *ir_value = build_func_addr_by_name(builder->pool, ir_funcptr_type, expr->name);
    } else {
        *ir_value = build_func_addr(builder->pool, ir_funcptr_type, pair.ir_func_id);
    }

    // Write resulting type if requested
    if(out_expr_type != NULL){
        ast_func_t *ast_func = &ast->funcs[pair.ast_func_id];

        // Force return type of result type of be 'int' if target function is entry point function
        // (e.g. func &main will give back either 'func() int' or 'func(int, **ubyte) int', but never func() void)
        if(ast_func->traits & AST_FUNC_MAIN){
            *out_expr_type = ast_type_make_func_ptr(expr->source, ast_func->arg_types, ast_func->arity, &ast->common.ast_int_type, ast_func->traits, false);
        } else {
            *out_expr_type = ast_type_make_func_ptr(expr->source, ast_func->arg_types, ast_func->arity, &ast_func->return_type, ast_func->traits, false);
        }
    }
    
    // We successfully got the function address of the function
    return SUCCESS;

fail_tentatively:
    // Since we cannot know the exact type of the function returned (because none exist)
    // we must return a generic null pointer of type 'ptr'
    // I'm not sure whether this is the best choice, but it feels right
    // - Isaac (Mar 21 2020)
    *ir_value = build_null_pointer(builder->pool);

    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("ptr"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_func_addr_noop_result_for_defer(ir_builder_t *builder, ast_type_t *match_arg, source_t source_on_error, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // NOTE: CLEANUP: Cleanup this code

    func_id_t ir_func_id;
    if(ir_builder_get_noop_defer_func(builder, source_on_error, &ir_func_id)) return FAILURE;

    ir_module_t *module = &builder->object->ir_module;
    ir_func_t *ir_func = &module->funcs.funcs[ir_func_id];

    // Get function pointer type for no-op defer function
    ir_type_t *ir_noop_funcptr_type = ir_type_make_function_pointer(builder->pool, ir_func->argument_types, 1, ir_func->return_type, TRAIT_NONE);

    // Resolve argument types
    ir_type_t **arg_types = ir_pool_alloc(builder->pool, sizeof(ir_type_t));

    if(ir_gen_resolve_type(builder->compiler, builder->object, match_arg, &arg_types[0])){
        return FAILURE;
    }

    // Get function pointer type
    ir_type_t *ir_final_funcptr_type = ir_type_make_function_pointer(builder->pool, arg_types, 1, ir_func->return_type, TRAIT_NONE);

    // Get function address
    *ir_value = build_func_addr(builder->pool, ir_noop_funcptr_type, ir_func_id);

    // Cast to proper type
    *ir_value = build_const_bitcast(builder->pool, *ir_value, module->common.ir_ptr);
    *ir_value = build_const_bitcast(builder->pool, *ir_value, ir_final_funcptr_type);

    // Write resulting type if requested
    if(out_expr_type != NULL){
        ast_type_t *args = malloc(sizeof(ast_type_t));
        args[0] = ast_type_clone(match_arg);

        ast_type_t *void_ast_type = malloc(sizeof(ast_type_t));
        *void_ast_type = ast_type_make_base(strclone("void"));

        *out_expr_type = ast_type_make_func_ptr(source_on_error, args, 1, void_ast_type, TRAIT_NONE, true);
    }

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

    // Result type is the dereferenced type, if not requested, we'll dispose of it
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

        ir_type_t *casted_ir_type = ir_type_make_pointer_to(builder->pool, ((ir_type_extra_fixed_array_t*) ((ir_type_t*) array_value->type->extra)->extra)->subtype);
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
        *ir_value = handle_access_management(builder, array_value, index_value, &array_type, &index_type, &element_pointer_type);
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

errorcode_t ir_gen_expr_sizeof(ir_builder_t *builder, ast_expr_sizeof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_type_t *of_type;

    // Resolve AST type to IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->type, &of_type)) return FAILURE;

    // Get size of IR type
    *ir_value = build_const_sizeof(builder->pool, ir_builder_usize(builder), of_type);

    // Return type is always usize
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("usize"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_sizeof_value(ir_builder_t *builder, ast_expr_sizeof_value_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_type_t *of_type;
    ast_type_t ast_type;

    // Take snapshot of IR pool in order to restore it later
    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);

    // DANGEROUS: Manually storing state of builder
    length_t basicblocks_length = builder->basicblocks.length;
    length_t current_block_id = builder->current_block_id;
    length_t instructions_length = builder->basicblocks.blocks[current_block_id].instructions.length;

    // Generate the expression to get back it's result type
    if(ir_gen_expr(builder, expr->value, NULL, true, &ast_type)) return FAILURE;

    // DANGEROUS: Manually restoring state of builder
    builder->basicblocks.length = basicblocks_length;
    build_using_basicblock(builder, builder->current_block_id);
    builder->basicblocks.blocks[current_block_id].instructions.length = instructions_length;
    
    // Restore IR pool from previous snapshot
    ir_pool_snapshot_restore(builder->pool, &snapshot);

    // Resolve AST type to IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &ast_type, &of_type)){
        ast_type_free(&ast_type);
        return FAILURE;
    }

    // Dispose of temporary AST type
    ast_type_free(&ast_type);

    // Get size of IR type
    *ir_value = build_const_sizeof(builder->pool, ir_builder_usize(builder), of_type);

    // Return type is always usize
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("usize"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_phantom(ast_expr_phantom_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // Value is phantom IR value
    *ir_value = (ir_value_t*) expr->ir_value;

    // Result type is as specified
    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&expr->type);
    return SUCCESS;
}

errorcode_t ir_gen_expr_call_method(ir_builder_t *builder, ast_expr_call_method_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_pool_snapshot_t pool_snapshot;
    instructions_snapshot_t instructions_snapshot;

    // Take snapshot of construction state,
    // so that if this call ends up to be a no-op,
    // we can reset back as if nothing happened
    ir_pool_snapshot_capture(builder->pool, &pool_snapshot);
    instructions_snapshot_capture(builder, &instructions_snapshot);

    // Setup for generating method call
    ast_t *ast = &builder->object->ast;
    length_t arg_arity = expr->arity + 1;
    ir_value_t **arg_values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * arg_arity);
    ast_type_t *arg_types = malloc(sizeof(ast_type_t) * arg_arity);
    ir_value_t *stack_pointer = NULL;
    bool used_temporary_subject = false;

    // Generate IR value for subject
    if(ir_gen_expr(builder, expr->value, &arg_values[0], true, &arg_types[0])){
        free(arg_types);
        return FAILURE;
    }

    // If subject value isn't mutable, make it mutable by creating a
    // temporary variable on the stack
    if(ast_type_is_base_like(&arg_types[0])){
        if(!expr_is_mutable(expr->value)){
            // Save the stack pointer
            stack_pointer = build_stack_save(builder);
            
            // Allocate space for the value on the stack
            ir_value_t *temporary_mutable = build_alloc(builder, arg_values[0]->type);

            // Copy the value onto the stack
            build_store(builder, arg_values[0], temporary_mutable, expr->source);

            // Replace the immutable argument value with the mutable reference
            arg_values[0] = temporary_mutable;

            // Remember that we used a temporary variable for the subject value
            used_temporary_subject = true;
        }

        // Force AST type into a '*SubjectType'
        ast_type_prepend_ptr(&arg_types[0]);
    } else if(ast_type_is_pointer_to_base_like(arg_types)){
        // If subject value is a mutable pointer value, turn it into an immutable pointer value
        if(expr_is_mutable(expr->value)){
            arg_values[0] = build_load(builder, arg_values[0], expr->source);
        }
    } else {
        // Otherwise, we don't know how to use the given subject type to call a method

        if(expr->is_tentative){
            if(out_expr_type != NULL){
                *out_expr_type = ast_type_make_base(strclone("void"));
            }

            ast_types_free_fully(arg_types, 1);
            return SUCCESS;
        }
        
        strong_cstr_t typename = ast_type_str(&arg_types[0]);
        compiler_panicf(builder->compiler, expr->source, "Can't call methods on type '%s'", typename);
        ast_types_free_fully(arg_types, 1);
        free(typename);
        return FAILURE;
    }

    // Generate non-subject argument values
    for(length_t a = 0; a != expr->arity; a++){
        if(ir_gen_expr(builder, expr->args[a], &arg_values[a + 1], false, &arg_types[a + 1])){
            ast_types_free_fully(arg_types, a + 1);
            return FAILURE;
        }

        if(ast_type_is_void(&arg_types[a + 1])){
            compiler_panicf(builder->compiler, expr->args[a]->source, "Cannot pass 'void' to method");
            ast_types_free_fully(arg_types, a + 1);
            return FAILURE;
        }
    }

    // Find the appropriate method to call
    optional_func_pair_t result;
    errorcode_t error = ir_gen_expr_call_method_find_appropriate_method(builder, expr, &arg_values, &arg_types, &arg_arity, &expr->gives, &result);
    
    // If we couldn't find a suitable method, handle the failure
    if(error != SUCCESS){
        ast_types_free_fully(arg_types, arg_arity);
        if(error == FAILURE) return FAILURE;

        // The method call is tentative, so ignore the failure
        if(out_expr_type != NULL){
            *out_expr_type = ast_type_make_base(strclone("void"));
        }

        return SUCCESS;
    }

    if(!result.has){
        // This method call is a no-op
        instructions_snapshot_restore(builder, &instructions_snapshot);
        ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
        ast_types_free_fully(arg_types, arg_arity);

        if(out_expr_type != NULL){
            *out_expr_type = ast_type_make_base(strclone("void"));
        }

        return SUCCESS;
    }

    func_pair_t pair = result.value;

    if(ensure_not_violating_disallow(builder->compiler, expr->source, &ast->funcs[pair.ast_func_id])
    || ensure_not_violating_no_discard(builder->compiler, expr->no_discard, expr->source, &ast->funcs[pair.ast_func_id])){
        ast_types_free_fully(arg_types, arg_arity);
        return ALT_FAILURE;
    }

    {
        ast_func_t *ast_func = &ast->funcs[pair.ast_func_id];

        if(ir_gen_expr_call_procedure_handle_pass_management(builder, arg_arity, arg_values, arg_types, ast_func->traits, ast_func->arg_type_traits, ast_func->arity)){
            ast_types_free_fully(arg_types, arg_arity);
            return FAILURE;
        }
    }

    // Remember arity before variadic argument packing
    length_t unpacked_arity = arg_arity;

    // Handle variadic argument packing if applicable
    if(ir_gen_expr_call_procedure_handle_variadic_packing(builder, &arg_values, arg_types, &arg_arity, &pair, &stack_pointer, expr->source)){
        ast_types_free(arg_types, arg_arity);
        return FAILURE;
    }

    ir_type_t *ir_return_type = builder->object->ir_module.funcs.funcs[pair.ir_func_id].return_type;

    // Don't even bother with result unless we care about the it
    if(ir_value){
        *ir_value = build_call(builder, pair.ir_func_id, ir_return_type, arg_values, arg_arity);
    } else {
        build_call_ignore_result(builder, pair.ir_func_id, ir_return_type, arg_values, arg_arity);
    }
    
    if(used_temporary_subject && !expr->allow_drop){
        // Dereference pointer to subject AST type to get just the subject AST type
        ast_type_dereference(&arg_types[0]);
        
        // Call __defer__ on temporary mutable value
        if(handle_single_deference(builder, &arg_types[0], arg_values[0]) == ALT_FAILURE){
            ast_types_free_fully(arg_types, unpacked_arity);
            return FAILURE;
        }
    }

    // Restore stack pointer if we allocated memory on the stack
    if(stack_pointer) build_stack_restore(builder, stack_pointer);

    // Dispose of AST argument types
    ast_types_free_fully(arg_types, unpacked_arity);

    // Result type is the return type of the method
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_clone(&ast->funcs[pair.ast_func_id].return_type);
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_call_method_find_appropriate_method(ir_builder_t *builder, ast_expr_call_method_t *expr, ir_value_t ***arg_values,
        ast_type_t **arg_types, length_t *arg_arity, ast_type_t *gives, optional_func_pair_t *result){
    
    weak_cstr_t subject;
    ast_type_t *subject_type = &(*arg_types)[0];
    
    // Obtain the name of the subject and find the appropriate method
    if(ast_type_is_pointer_to_base(subject_type)){
        subject = ((ast_elem_base_t*) subject_type->elements[1])->base;
    } else if(ast_type_is_pointer_to_generic_base(subject_type)){
        subject = ((ast_elem_generic_base_t*) subject_type->elements[1])->name;
    } else {
        internalerrorprintf("ir_gen_expr_call_method_find_appropriate_method() - Bad subject type\n");
        return FAILURE;
    }

    if(ir_gen_find_method_conforming(builder, subject, expr->name, arg_values, arg_types, arg_arity, gives, expr->source, result)){
        if(expr->is_tentative) return ALT_FAILURE;
        compiler_undeclared_function(builder->compiler, builder->object, expr->source, expr->name, *arg_types, expr->arity + 1, NULL, true);
        return FAILURE;
    }
    
    return SUCCESS;
}

errorcode_t ir_gen_expr_unary(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ast_type_t expr_type;
    ir_value_t *expr_value;
    strong_cstr_t error_typename;

    if(ir_gen_expr(builder, expr->value, &expr_value, false, &expr_type)) return FAILURE;

    unsigned int category = ir_type_get_category(expr_value->type);
    
    ir_instr_unary_t *instruction = (ir_instr_unary_t*) build_instruction(builder, sizeof(ir_instr_unary_t));
    instruction->value = expr_value;

    if(expr->id == EXPR_NOT){
        // Build '!'

        // Must be either integer or float like
        if(category == PRIMITIVE_NA) goto cant_use_operator_on_that_type;

        instruction->id = INSTRUCTION_ISZERO;
        instruction->result_type = ir_builder_bool(builder);

        // Result type is bool
        if(out_expr_type != NULL){
            *out_expr_type = ast_type_make_base(strclone("bool"));
        }
    } else {
        // Build '-' or '~'
        instruction->result_type = expr_value->type;

        // Determine instruction
        switch(category){
        case PRIMITIVE_UI:
        case PRIMITIVE_SI:
            instruction->id = expr->id == EXPR_NEGATE ? INSTRUCTION_NEGATE : INSTRUCTION_BIT_COMPLEMENT;
            break;
        case PRIMITIVE_FP:
            if(expr->id == EXPR_BIT_COMPLEMENT) goto cant_use_operator_on_that_type;        
            instruction->id = INSTRUCTION_FNEGATE;
            break;
        default:
            goto cant_use_operator_on_that_type;
        }

        // Result type is the same as the value type
        if(out_expr_type) *out_expr_type = ast_type_clone(&expr_type);
    }

    // We successfully generated the value for the unary expression
    *ir_value = build_value_from_prev_instruction(builder);
    ast_type_free(&expr_type);
    return SUCCESS;

cant_use_operator_on_that_type:
    error_typename = ast_type_str(&expr_type);

    compiler_panicf(builder->compiler, expr->source, "Can't use '%c' operator on type '%s'",
        expr->id == EXPR_NOT ? '!' : (expr->id == EXPR_NEGATE ? '-' : '~'), error_typename);
    
    free(error_typename);
    ast_type_free(&expr_type);
    return FAILURE;
}

errorcode_t ir_gen_expr_new(ir_builder_t *builder, ast_expr_new_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_type_t *ir_type;
    ast_type_t multiplier_type;
    ir_value_t *amount = NULL;

    // If a count is given, validate it and generate an IR value
    if(expr->amount != NULL){
        // Generate IR value for multiplier
        if(ir_gen_expr(builder, ((ast_expr_new_t*) expr)->amount, &amount, false, &multiplier_type)) return FAILURE;

        // Ensure the type of the count is an integer
        unsigned int multiplier_typekind = amount->type->kind;

        if(multiplier_typekind < TYPE_KIND_S8 || multiplier_typekind > TYPE_KIND_U64){
            char *typename = ast_type_str(&multiplier_type);
            compiler_panicf(builder->compiler, expr->source, "Can't specify length using non-integer type '%s'", typename);
            ast_type_free(&multiplier_type);
            free(typename);
            return FAILURE;
        }

        // Dispose of multiplier type
        ast_type_free(&multiplier_type);
    }

    // Resolve the target AST type to an IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->type, &ir_type)) return FAILURE;

    // Build heap allocation instruction
    *ir_value = build_malloc(builder, ir_type, amount, expr->is_undef, NULL);

    if(expr->inputs.has){
        if(expr->amount != NULL){
            char *typename = ast_type_str(&expr->type);
            compiler_panicf(builder->compiler, expr->source, "Can't specify count when trying to construct new '%s'", typename);
            free(typename);
            return FAILURE;
        }

        if(ir_gen_do_construct(builder, *ir_value, &expr->type, &expr->inputs.value, expr->source)){
            return FAILURE;
        }
    }

    // Result type is pointer to requested type
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_clone(&expr->type);
        ast_type_prepend_ptr(out_expr_type);
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_new_cstring(ir_builder_t *builder, ast_expr_new_cstring_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_shared_common_t *common = &builder->object->ir_module.common;

    // Get length of C-string
    length_t value_length = strlen(expr->value);

    // Get IR value for bytes necessary
    ir_value_t *bytes_value = build_literal_usize(builder->pool, value_length + 1);

    // Build heap allocation instruction
    *ir_value = build_malloc(builder, common->ir_ubyte, bytes_value, true, common->ir_ptr);

    // Generate a C-string constant
    ir_value_t *cstring_value = build_literal_cstr(builder, expr->value);

    // Generate instruction to copy string constant to allocated heap memory
    ir_instr_memcpy_t *memcpy_instruction = (ir_instr_memcpy_t*) build_instruction(builder, sizeof(ir_instr_memcpy_t));
    memcpy_instruction->id = INSTRUCTION_MEMCPY;
    memcpy_instruction->result_type = NULL;
    memcpy_instruction->destination = *ir_value;
    memcpy_instruction->value = cstring_value;
    memcpy_instruction->bytes = bytes_value;
    memcpy_instruction->is_volatile = false;

    // Result type is *ubyte
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base_ptr(strclone("ubyte"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_enum_value(ir_builder_t *builder, ast_expr_enum_value_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    length_t enum_kind_id;

    // Find referenced enum
    maybe_index_t enum_index = ast_find_enum(builder->object->ast.enums, builder->object->ast.enums_length, expr->enum_name);

    // Fail if we couldn't find the enum
    if(enum_index == -1){
        compiler_panicf(builder->compiler, expr->source, "Failed to enum '%s'", expr->enum_name);
        return FAILURE;
    }

    // Get enum information
    ast_enum_t *enum_definition = &builder->object->ast.enums[enum_index];

    // Find the ID of the field of the enum
    if(!ast_enum_find_kind(enum_definition, expr->kind_name, &enum_kind_id)){
        compiler_panicf(builder->compiler, expr->source, "Failed to find member '%s' of enum '%s'", expr->kind_name, expr->enum_name);
        return FAILURE;
    }

    // The IR value for the enum value is just its ID as an u64
    *ir_value = build_literal_usize(builder->pool, enum_kind_id);

    // Result type is the enum
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone(expr->enum_name));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_static_array(ir_builder_t *builder, ast_expr_static_data_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * expr->length);
    length_t length = expr->length;

    // Resolve IR value for target element type
    ir_type_t *type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->type, &type)){
        return FAILURE;
    }

    // Generate IR value for each element
    for(length_t i = 0; i != expr->length; i++){
        ast_type_t member_type;

        // Generate IR value for element
        if(ir_gen_expr(builder, expr->values[i], &values[i], false, &member_type)) return FAILURE;

        // Conform element to array element type
        if(!ast_types_conform(builder, &values[i], &member_type, &expr->type, CONFORM_MODE_ASSIGNING)){
            char *a_type_str = ast_type_str(&member_type);
            char *b_type_str = ast_type_str(&expr->type);
            compiler_panicf(builder->compiler, expr->type.source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&member_type);
            return FAILURE;
        }

        // Ensure the value given is constant
        if(!VALUE_TYPE_IS_CONSTANT(values[i]->value_type)){
            compiler_panicf(builder->compiler, expr->values[i]->source, "Can't put non-constant value into constant array");
            ast_type_free(&member_type);
            return FAILURE;
        }

        // Dispose of temporary AST member type
        ast_type_free(&member_type);
    }

    // Build static array value
    *ir_value = build_static_array(builder->pool, type, values, length);

    // Result type is pointer to element type
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_clone(&expr->type);
        ast_type_prepend_ptr(out_expr_type);
    }
    return SUCCESS;
}

errorcode_t ir_gen_expr_static_struct(ir_builder_t *builder, ast_expr_static_data_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // Generate IR type for struct type
    ir_type_t *type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->type, &type)){
        return FAILURE;
    }

    // Find the AST structure
    const char *base = ((ast_elem_base_t*) expr->type.elements[0])->base;
    ast_composite_t *structure = ast_composite_find_exact(&builder->object->ast, base);
    if(structure == NULL) return FAILURE;

    if(!ast_layout_is_simple_struct(&structure->layout)){
        compiler_panicf(builder->compiler, expr->source, "Cannot create struct literal for complex composite type '%s'", base);
        return FAILURE;
    }

    length_t field_count = ast_simple_field_map_get_count(&structure->layout.field_map);

    // Ensure the number of fields given is correct
    if(field_count != expr->length){
        compiler_panicf(builder->compiler, expr->source, "Incorrect number of fields given in struct literal for struct '%s'", base);
        return FAILURE;
    }

    // Generate space for field IR values
    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * expr->length);
    length_t length = expr->length;

    // Generate IR values for struct fields
    for(length_t i = 0; i != expr->length; i++){
        ast_type_t member_type;

        // Generate IR value for field
        if(ir_gen_expr(builder, expr->values[i], &values[i], false, &member_type)){
            return FAILURE;
        }

        ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&structure->layout.skeleton, i);

        // Conform the field IR value to the expected AST type
        if(!ast_types_conform(builder, &values[i], &member_type, field_type, CONFORM_MODE_STANDARD)){
            char *a_type_str = ast_type_str(&member_type);
            char *b_type_str = ast_type_str(&expr->type);
            compiler_panicf(builder->compiler, expr->values[i]->source, "Can't cast type '%s' to type '%s'", a_type_str, b_type_str);
            free(a_type_str);
            free(b_type_str);
            ast_type_free(&member_type);
            return FAILURE;
        }

        // Ensure the value given in constant
        if(!VALUE_TYPE_IS_CONSTANT(values[i]->value_type)){
            compiler_panicf(builder->compiler, expr->values[i]->source, "Can't put non-constant value into constant array");
            ast_type_free(&member_type);
            return FAILURE;
        }

        // Dispose of temporary member AST type
        ast_type_free(&member_type);
    }

    // Build struct literal IR value
    *ir_value = build_static_struct(&builder->object->ir_module, type, values, length, true);

    // Result type is pointer to struct
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_clone(&expr->type);
        ast_type_prepend_ptr(out_expr_type);
    }
    return SUCCESS;
}

errorcode_t ir_gen_expr_typeinfo(ir_builder_t *builder, ast_expr_typeinfo_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // Ensure runtime type information is enabled
    if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
        compiler_panic(builder->compiler, expr->source, "Unable to use runtime type info when runtime type information is disabled");
        return FAILURE;
    }

    // Get IR value for pointer to AnyType struct
    *ir_value = rtti_for(builder, &expr->type, expr->source);
    if(*ir_value == NULL) return FAILURE;

    // Result type is *AnyType
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base_ptr(strclone("AnyType"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_ternary(ir_builder_t *builder, ast_expr_ternary_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_value_t *condition, *if_true, *if_false;
    ast_type_t condition_type, if_true_type, if_false_type;

    // Generate IR value for condition
    if(ir_gen_expr(builder, expr->condition, &condition, false, &condition_type)) return FAILURE;

    // Conform IR value to be a boolean
    if(!ast_types_conform(builder, &condition, &condition_type, &builder->static_bool, CONFORM_MODE_CALCULATION)){
        char *condition_type_str = ast_type_str(&condition_type);
        compiler_panicf(builder->compiler, expr->condition->source, "Received type '%s' when conditional expects type 'bool'", condition_type_str);
        free(condition_type_str);
        ast_type_free(&condition_type);
        return FAILURE;
    }

    // Dispose of temporary condition AST type
    ast_type_free(&condition_type);

    // Build blocks to jump to depending on condition
    length_t when_true_block_id = build_basicblock(builder);
    length_t when_false_block_id = build_basicblock(builder);

    // Jump to appropriate block based on condition
    build_cond_break(builder, condition, when_true_block_id, when_false_block_id);

    // Generate instructions for when condition is true
    build_using_basicblock(builder, when_true_block_id);
    if(ir_gen_expr(builder, expr->if_true, &if_true, false, &if_true_type)) return FAILURE;

    // Remember where true branch landed
    length_t when_true_landing = builder->current_block_id;

    // Generate instructions for when condition is false
    build_using_basicblock(builder, when_false_block_id);
    if(ir_gen_expr(builder, expr->if_false, &if_false, false, &if_false_type)){
        ast_type_free(&if_true_type);
        return FAILURE;
    }

    // Remember where false branch landed
    length_t when_false_landing = builder->current_block_id;
    
    // Ensure the resulting types of the two branches is the same
    if(!ast_types_identical(&if_true_type, &if_false_type)){
        // Try to autocast to larger type of the two if there is one
        bool conflict_resolved = ir_gen_resolve_ternary_conflict(builder, &if_true, &if_false, &if_true_type, &if_false_type, &when_true_landing, &when_false_landing);
        
        if(!conflict_resolved){
            char *if_true_typename = ast_type_str(&if_true_type);
            char *if_false_typename = ast_type_str(&if_false_type);
            compiler_panicf(builder->compiler, expr->source, "Ternary operator could result in different types '%s' and '%s'", if_true_typename, if_false_typename);
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

    // Resolve IR type for finalized AST type
    ir_type_t *result_ir_type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, &if_true_type, &result_ir_type)){
        ast_type_free(&if_true_type);
        ast_type_free(&if_false_type);
        return FAILURE;
    }

    // Result type is the AST type if the 'true' branch
    if(out_expr_type){
        *out_expr_type = if_true_type;
    } else {
        // If we don't use the result type, we can dispose if it
        ast_type_free(&if_true_type);
    }

    // Dispose of temporary AST type of the 'false' branch
    ast_type_free(&if_false_type);

    // Merge the two branch values into a single value
    *ir_value = build_phi2(builder, result_ir_type, if_true, if_false, when_true_landing, when_false_landing);
    return SUCCESS;
}

errorcode_t ir_gen_expr_crement(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ir_value_t *before_value;
    ast_type_t before_ast_type;

    // Ensure mutable value given
    if(!expr_is_mutable(expr->value)){
        compiler_panic(builder->compiler, expr->value->source, "INTERNAL ERROR: xCREMENT expression expected mutable value");
        ast_type_free(&before_ast_type);
        return FAILURE;
    }

    // Generate IR value for initial value
    if(ir_gen_expr(builder, expr->value, &before_value, true, &before_ast_type)) return FAILURE;
    
    // Ensure IR type of IR value of initial value is a pointer
    if(before_value->type->kind != TYPE_KIND_POINTER){
        compiler_panic(builder->compiler, expr->value->source, "INTERNAL ERROR: ir_gen_expr() EXPR_xCREMENT expected mutable value");
        ast_type_free(&before_ast_type);
        return FAILURE;
    }

    // Generate an IR value of '1' for the type we're working with
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
        one->extra = ir_pool_alloc(builder->pool, sizeof(adept_short));
        *((adept_short*) one->extra) = 1;
        break;
    case TYPE_KIND_U16:
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
        *((adept_double*) one->extra) = 1.0;
        break;
    }

    // If we couldn't create a value for 'one' of that type, so
    // we can't perform the -crement instruction on this value
    if(one->extra == NULL){
        char *typename = ast_type_str(&before_ast_type);
        compiler_panicf(builder->compiler, expr->source, "Can't %s type '%s'",
            expr->id == EXPR_PREINCREMENT || expr->id == EXPR_POSTINCREMENT ? "increment" : "decrement", typename);
        ast_type_free(&before_ast_type);
        free(typename);
        return FAILURE;
    }

    ir_value_t *before_value_immutable = build_load(builder, before_value, expr->source);

    // Build template for math instruction
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    instruction->id = INSTRUCTION_NONE; // Will be determined
    instruction->a = before_value_immutable;
    instruction->b = one;
    instruction->result_type = before_value->type; 

    // Generate respective instruction
    if(expr->id == EXPR_PREINCREMENT || expr->id == EXPR_POSTINCREMENT){
        // For '++', do add instruction
        if(i_vs_f_instruction(instruction, INSTRUCTION_ADD, INSTRUCTION_FADD)){
            char *typename = ast_type_str(&before_ast_type);
            compiler_panicf(builder->compiler, expr->source, "Can't increment type '%s'", typename);
            ast_type_free(&before_ast_type);
            free(typename);
            return FAILURE;
        }
    } else {
        // For '--', do subtract instruction
        if(i_vs_f_instruction(instruction, INSTRUCTION_SUBTRACT, INSTRUCTION_FSUBTRACT)){
            char *typename = ast_type_str(&before_ast_type);
            compiler_panicf(builder->compiler, expr->source, "Can't decrement type '%s'", typename);
            ast_type_free(&before_ast_type);
            free(typename);
            return FAILURE;
        }
    }

    // Store modified value
    ir_value_t *modified = build_value_from_prev_instruction(builder);
    build_store(builder, modified, before_value, expr->source);

    // Result type is the type of the initial expression
    if(out_expr_type){
        *out_expr_type = before_ast_type;
    } else {
        // Dispose of the temporary type of the initial expression
        // if we're not going to use it as the result type
        ast_type_free(&before_ast_type);
    }

    // Don't even bother with result unless we care about the it
    if(ir_value){
        if(expr->id == EXPR_PREINCREMENT || expr->id == EXPR_PREDECREMENT){
            // For pre-crement instructions, work with the post value
            *ir_value = leave_mutable ? before_value : build_load(builder, before_value, expr->source);
        } else {
            // For post-crement instructions, work with the pre value
            *ir_value = leave_mutable ? before_value : before_value_immutable;
        }
    }

    // We successfully generate instructions for the xCREMENT instruction
    return SUCCESS;
}

errorcode_t ir_gen_expr_toggle(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type){
    ast_type_t ast_type;
    ir_value_t *mutable_value;

    // Generate mutable IR value from AST expression
    if(ir_gen_expr(builder, expr->value, &mutable_value, true, &ast_type)) return FAILURE;

    // Get IR type of dereferenced mutable IR value
    ir_type_t *dereferenced_ir_type = ir_type_dereference(mutable_value->type);

    // Ensure the value given is a boolean value
    if(!ast_type_is_base_of(&ast_type, "bool") || dereferenced_ir_type == NULL || dereferenced_ir_type->kind != TYPE_KIND_BOOLEAN){
        char *t = ast_type_str(&ast_type);
        compiler_panicf(builder->compiler, expr->source, "Cannot toggle non-boolean type '%s'", t);
        ast_type_free(&ast_type);
        free(t);
        return FAILURE;
    }

    // Load and not the boolean value
    ir_value_t *loaded = build_load(builder, mutable_value, expr->source);
    
    // Perform 'not' operation
    ir_instr_unary_t *isz_instr = (ir_instr_unary_t*) build_instruction(builder, sizeof(ir_instr_unary_t));
    isz_instr->id = INSTRUCTION_ISZERO;
    isz_instr->result_type = ir_builder_bool(builder);
    isz_instr->value = loaded;
    ir_value_t *notted = build_value_from_prev_instruction(builder);
    
    // Store it back into memory
    build_store(builder, notted, mutable_value, expr->source);

    // Result type is the AST type of the initial value
    if(out_expr_type){
        *out_expr_type = ast_type;
    } else {
        // Dispose of AST type of initial value if we're not
        // going to use it as the result type
        ast_type_free(&ast_type);
    }

    // Don't even bother with expression result unless we care about the it
    if(ir_value) *ir_value = leave_mutable ? mutable_value : notted;
    return SUCCESS;
}

errorcode_t ir_gen_expr_inline_declare(ir_builder_t *builder, ast_expr_inline_declare_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ast_expr_inline_declare_t *def = ((ast_expr_inline_declare_t*) expr);

    bool is_pod = def->traits & AST_EXPR_DECLARATION_POD;
    bool is_assign_pod = def->traits & AST_EXPR_DECLARATION_ASSIGN_POD;

    // Ensure no variable with the same name already exists in this scope
    if(bridge_scope_var_already_in_list(builder->scope, def->name)){
        compiler_panicf(builder->compiler, def->source, "Variable '%s' already declared", def->name);
        return FAILURE;
    }

    // Allocate space in pool to hold IR type of declaration
    ir_type_t *ir_decl_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));

    // Resolve AST declaration type to IR declaration type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &def->type, &ir_decl_type)) return FAILURE;

    // Get IR type of mutable declaration value
    ir_type_t *var_pointer_type = ir_type_make_pointer_to(builder->pool, ir_decl_type);

    // Determine how to create inline variable, and do so
    if(def->value != NULL){
        // Inline declaration statement with initial value

        // Generate IR value for initial value
        ir_value_t *initial;
        ast_type_t initial_value_ast_type;
        if(ir_gen_expr(builder, def->value, &initial, false, &initial_value_ast_type)) return FAILURE;

        // Add the variable
        ir_value_t *destination = build_lvarptr(builder, var_pointer_type, builder->next_var_id);
        add_variable(builder, def->name, &def->type, ir_decl_type, is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

        errorcode_t errorcode = ir_gen_assign(builder, initial, &initial_value_ast_type, destination, &def->type, is_assign_pod, def->source);
        ast_type_free(&initial_value_ast_type);
        if(errorcode != SUCCESS) return errorcode;

        // Result is pointer to variable on stack
        *ir_value = destination;
    } else if(def->id == EXPR_ILDECLAREUNDEF && !(builder->compiler->traits & COMPILER_NO_UNDEF)){
        // Mark the variable as undefined memory so it isn't auto-initialized later on
        add_variable(builder, def->name, &def->type, ir_decl_type, is_pod ? BRIDGE_VAR_UNDEF | BRIDGE_VAR_POD : BRIDGE_VAR_UNDEF);

        // Result is pointer to variable on stack
        *ir_value = build_lvarptr(builder, var_pointer_type, builder->next_var_id - 1);
    } else /* plain ILDECLARE or --no-undef ILDECLAREUNDEF */ {
        // Variable declaration without initial value
        
        add_variable(builder, def->name, &def->type, ir_decl_type, is_pod ? BRIDGE_VAR_POD : TRAIT_NONE);

        // Zero initialize the variable
        ir_value_t *destination = build_lvarptr(builder, var_pointer_type, builder->next_var_id - 1);
        build_zeroinit(builder, destination);
        
        // Result is pointer to variable on stack
        *ir_value = destination;
    }

    // Result type is pointer to AST declaration type
    if(out_expr_type){
        ast_type_t ast_pointer = ast_type_clone(&def->type);
        ast_type_prepend_ptr(&ast_pointer);
        *out_expr_type = ast_pointer;
    }
    return SUCCESS;
}

errorcode_t ir_gen_expr_typenameof(ir_builder_t *builder, ast_expr_typenameof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    strong_cstr_t on_heap = ast_type_str(&expr->type);
    length_t size = strlen(on_heap) + 1;

    weak_cstr_t name = ir_pool_alloc(builder->pool, size);
    memcpy(name, on_heap, size);
    free(on_heap);
    
    *ir_value = build_literal_cstr_of_length(builder, name, size);

    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base_ptr(strclone("ubyte"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_embed(ir_builder_t *builder, ast_expr_embed_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    char *array;
    length_t length;

    if(file_binary_contents(expr->filename, &array, &length) == false){
        compiler_panicf(builder->compiler, expr->source, "Failed to read file '%s'", expr->filename);
        return FAILURE;
    }
    
    *ir_value = build_literal_str(builder, array, length);
    ir_module_defer_free(&builder->object->ir_module, array);

    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("String"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_alignof(ir_builder_t *builder, ast_expr_alignof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    ir_type_t *of_type;

    // Resolve AST type to IR type
    if(ir_gen_resolve_type(builder->compiler, builder->object, &expr->type, &of_type)) return FAILURE;

    // Get alignment of IR type
    *ir_value = build_const_alignof(builder->pool, ir_builder_usize(builder), of_type);

    // Return type is always usize
    if(out_expr_type != NULL){
        *out_expr_type = ast_type_make_base(strclone("usize"));
    }

    return SUCCESS;
}

errorcode_t ir_gen_expr_math(ir_builder_t *builder, ast_expr_math_t *math_expr, ir_value_t **ir_value, ast_type_t *out_expr_type,
        unsigned int instr1, unsigned int instr2, unsigned int instr3, const char *op_verb, weak_cstr_t overload, bool result_is_boolean){

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
            *ir_value = handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, math_expr->source, out_expr_type, overload);

            // We successfully used the overload function
            if(*ir_value != NULL){
                ast_type_free(&ast_type_a);
                ast_type_free(&ast_type_b);
                return SUCCESS;
            }
        }

        // Failed to conform the values if we had an overload function, we'll try to use the arguments in reverse order now
        if(overload){
            *ir_value = handle_math_management(builder, rhs, lhs, &ast_type_b, &ast_type_a, math_expr->source, out_expr_type, overload);

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
        builder->current_block->instructions.length--;

        // Try to use the overload function if it exists
        *ir_value = overload ? handle_math_management(builder, lhs, rhs, &ast_type_a, &ast_type_b, math_expr->source, out_expr_type, overload) : NULL;
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
        *out_expr_type = result_is_boolean ? ast_type_make_base(strclone("bool")) : ast_type_clone(&ast_type_a);
    }

    ast_type_free(&ast_type_a);
    ast_type_free(&ast_type_b);
    return SUCCESS;
}

ir_instr_math_t* ir_gen_math_operands(ir_builder_t *builder, ast_expr_math_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type){
    // ir_gen the operands of an expression into instructions and gives back an instruction with unknown id

    // NOTE: Returns the generated instruction
    // NOTE: The instruction id still must be specified after this call
    // NOTE: Returns NULL if an error occurred
    // NOTE:'op_res' should be either MATH_OP_RESULT_MATCH

    ir_value_t *a, *b;
    ast_type_t ast_type_a, ast_type_b;

    if(ir_gen_expr(builder, expr->a, &a, false, &ast_type_a)) return NULL;
    if(ir_gen_expr(builder, expr->b, &b, false, &ast_type_b)){
        ast_type_free(&ast_type_a);
        return NULL;
    }

    if(!ast_types_conform(builder, &b, &ast_type_b, &ast_type_a, CONFORM_MODE_CALCULATION)){
        char *a_type_str = ast_type_str(&ast_type_a);
        char *b_type_str = ast_type_str(&ast_type_b);
        compiler_panicf(builder->compiler, expr->source, "Incompatible types '%s' and '%s'", a_type_str, b_type_str);
        free(a_type_str);
        free(b_type_str);

        ast_type_free(&ast_type_a);
        ast_type_free(&ast_type_b);
        return NULL;
    }

    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    instruction->id = INSTRUCTION_NONE; // To be filled in by the caller
    instruction->a = a;
    instruction->b = b;
    instruction->result_type = a->type;
    *ir_value = build_value_from_prev_instruction(builder);

    // Result type is the AST type of expression 'A'
    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(&ast_type_a);
    
    ast_type_free(&ast_type_a);
    ast_type_free(&ast_type_b);
    return instruction;
}

errorcode_t ir_gen_call_function_value(ir_builder_t *builder, ast_type_t *tmp_ast_variable_type,
        ast_expr_call_t *call, ir_value_t **arg_values, ast_type_t *arg_types,
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

    // Generate the actual call address instruction
    *inout_ir_value = build_call_address(builder, ir_return_type, *inout_ir_value, arg_values, call->arity);

    if(out_expr_type != NULL) *out_expr_type = ast_type_clone(function_elem->return_type);
    return SUCCESS;
}

successful_t ir_gen_resolve_ternary_conflict(ir_builder_t *builder, ir_value_t **a, ir_value_t **b, ast_type_t *a_type, ast_type_t *b_type,
        length_t *inout_a_basicblock, length_t *inout_b_basicblock){
    
    if(!ast_type_is_base(a_type) || !ast_type_is_base(b_type)) return UNSUCCESSFUL;
    if(!typename_is_extended_builtin_type( ((ast_elem_base_t*) a_type->elements[0])->base )) return UNSUCCESSFUL;
    if(!typename_is_extended_builtin_type( ((ast_elem_base_t*) b_type->elements[0])->base )) return UNSUCCESSFUL;
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

    switch(ir_type_get_category(instruction->a->type)){
    case PRIMITIVE_UI:
    case PRIMITIVE_SI:
        instruction->id = i_instr;
        break;
    case PRIMITIVE_FP:
        instruction->id = f_instr;
        break;
    default:
        return FAILURE;
    }

    return instruction->id != INSTRUCTION_NONE ? SUCCESS : FAILURE;
}

errorcode_t u_vs_s_vs_float_instruction(ir_instr_math_t *instruction, unsigned int u_instr, unsigned int s_instr, unsigned int f_instr){
    // NOTE: Sets the instruction id to 'u_instr' if operating on unsigned intergers
    //       Sets the instruction id to 's_instr' if operating on signed intergers
    //       Sets the instruction id to 'f_instr' if operating on floats
    // NOTE: If target instruction id is INSTRUCTION_NONE, then 1 is returned
    // NOTE: Returns 1 if unsupported type

    switch(ir_type_get_category(instruction->a->type)){
    case PRIMITIVE_UI:
        instruction->id = u_instr;
        break;
    case PRIMITIVE_SI:
        instruction->id = s_instr;
        break;
    case PRIMITIVE_FP:
        instruction->id = f_instr;
        break;
    default:
        return FAILURE;
    }

    return instruction->id != INSTRUCTION_NONE ? SUCCESS : FAILURE;
}

char ir_type_get_category(ir_type_t *type){
    switch(type->kind){
    case TYPE_KIND_POINTER:
    case TYPE_KIND_BOOLEAN:
    case TYPE_KIND_FUNCPTR:
    case TYPE_KIND_U8:
    case TYPE_KIND_U16:
    case TYPE_KIND_U32:
    case TYPE_KIND_U64:
        // Unsigned integer like values
        return PRIMITIVE_UI;
    case TYPE_KIND_S8:
    case TYPE_KIND_S16:
    case TYPE_KIND_S32:
    case TYPE_KIND_S64:
        // Signed integer like values
        return PRIMITIVE_SI; 
    case TYPE_KIND_HALF:
    case TYPE_KIND_FLOAT:
    case TYPE_KIND_DOUBLE:
        // Floating point like values
        return PRIMITIVE_FP;
    }

    // Otherwise, no category
    return PRIMITIVE_NA;
}
