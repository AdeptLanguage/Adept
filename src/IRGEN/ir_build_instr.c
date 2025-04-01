
#include "IRGEN/ir_build_instr.h"
#include "IRGEN/ir_build_literal.h"
#include "LEX/lex.h"

#define BUILD_VALUE(TYPE, ...) ( \
    *((TYPE*) build_instruction(builder, sizeof(TYPE))) = ((TYPE[]){ __VA_ARGS__ })[0], \
    build_value_from_prev_instruction(builder)\
)

#define BUILD_INSTR(TYPE, ...) \
    *((TYPE*) build_instruction(builder, sizeof(TYPE))) = ((TYPE[]){ __VA_ARGS__ })[0]


ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, bridge_var_t *var){
    return (var->traits & BRIDGE_VAR_STATIC)
        ? build_svarptr(builder, ptr_type, var->static_id)
        : build_lvarptr(builder, ptr_type, var->id);
}

ir_value_t *build_lvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    return BUILD_VALUE(ir_instr_varptr_t, {
        .id = INSTRUCTION_VARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    });
}

ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    return BUILD_VALUE(ir_instr_varptr_t, {
        .id = INSTRUCTION_GLOBALVARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    });
}

ir_value_t *build_svarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    return BUILD_VALUE(ir_instr_varptr_t, {
        .id = INSTRUCTION_STATICVARPTR,
        .result_type = ptr_type,
        .index = variable_id,
    });
}

ir_value_t *build_malloc(ir_builder_t *builder, ir_type_t *type, ir_value_t *amount, bool is_undef){
    return BUILD_VALUE(ir_instr_malloc_t, {
        .id = INSTRUCTION_MALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type, false),
        .type = type,
        .amount = amount,
        .is_undef = is_undef,
    });
}

void build_zeroinit(ir_builder_t *builder, ir_value_t *destination){
    BUILD_INSTR(ir_instr_zeroinit_t, {
        .id = INSTRUCTION_ZEROINIT,
        .destination = destination,
    });
}

void build_memcpy(ir_builder_t *builder, ir_value_t *destination, ir_value_t *value, ir_value_t *num_bytes, bool is_volatile){
    BUILD_INSTR(ir_instr_memcpy_t, {
        .id = INSTRUCTION_MEMCPY,
        .result_type = NULL,
        .destination = destination,
        .value = value,
        .bytes = num_bytes,
        .is_volatile = is_volatile,
    });
}

ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source){
    int line = -1, column = -1;
    ir_type_t *dereferenced_type = ir_type_dereference(value->type);

    // Ensure type is dereferencable
    if(dereferenced_type == NULL){
        return NULL;
    }

    // If null checks enabled, remember origin line/column
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &line, &column);
    }

    return BUILD_VALUE(ir_instr_load_t, {
        .id = INSTRUCTION_LOAD,
        .result_type = dereferenced_type,
        .value = value,
        .maybe_line_number = line,
        .maybe_column_number = column,
    });
}

ir_instr_store_t *build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source){
    int line = -1, column = -1;

    // If null checks enabled, remember origin line/column
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &line, &column);
    }

    BUILD_INSTR(ir_instr_store_t, {
        .id = INSTRUCTION_STORE,
        .result_type = NULL,
        .value = value,
        .destination = destination,
        .maybe_line_number = line,
        .maybe_column_number = column,
    });
    
    return (ir_instr_store_t*) ir_instrs_last_unchecked(&builder->current_block->instructions);
}

ir_value_t *build_call(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length, source_t code_source){
    // Build raw call
    build_call_ignore_result(builder, ir_func_id, result_type, arguments, arguments_length, code_source);

    // Build result value
    return build_value_from_prev_instruction(builder);
}

void build_call_ignore_result(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length, source_t code_source){
    int line = -1, column = -1;

    // If vtable validation is enabled, remember origin line/column
    if(builder->object->ir_module.funcs.funcs[ir_func_id].traits & IR_FUNC_VALIDATE_VTABLE){
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &line, &column);
    }

    BUILD_INSTR(ir_instr_call_t, {
        .id = INSTRUCTION_CALL,
        .result_type = result_type,
        .values = arguments,
        .values_length = arguments_length,
        .ir_func_id = ir_func_id,
        .maybe_line_number = line,
        .maybe_column_number = column,
    });
}

ir_value_t *build_call_address(ir_builder_t *builder, ir_type_t *return_type, ir_value_t *address, ir_value_t **arg_values, length_t arity, ir_type_t **param_types, length_t param_types_length, bool is_vararg){
    return BUILD_VALUE(ir_instr_call_address_t, {
        .id = INSTRUCTION_CALL_ADDRESS,
        .result_type = return_type,
        .function_address = address,
        .function_arg_types = param_types,
        .function_arg_types_length = param_types_length,
        .function_is_vararg = is_vararg,
        .values = arg_values,
        .values_length = arity,
    });
}

void build_break(ir_builder_t *builder, length_t basicblock_id){
    BUILD_INSTR(ir_instr_break_t, {
        .id = INSTRUCTION_BREAK,
        .result_type = NULL,
        .block_id = basicblock_id,
    });
}

void build_cond_break(ir_builder_t *builder, ir_value_t *condition, length_t true_block_id, length_t false_block_id){
    BUILD_INSTR(ir_instr_cond_break_t, {
        .id = INSTRUCTION_CONDBREAK,
        .result_type = NULL,
        .value = condition,
        .true_block_id = true_block_id,
        .false_block_id = false_block_id,
    });
}

ir_value_t *build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b){
    return BUILD_VALUE(ir_instr_math_t, {
        .id = INSTRUCTION_EQUALS,
        .a = a,
        .b = b,
        .result_type = ir_builder_bool(builder),
    });
}

ir_value_t *build_array_access(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, source_t code_source){
    if(value->type->kind != TYPE_KIND_POINTER){
        internalerrorprintf("build_array_access() - Cannot perform array access on non-pointer value\n");
        redprintf("                (value left unmodified)\n");
        return value;
    }

    return build_array_access_ex(builder, value, index, value->type, code_source);
}

ir_value_t *build_array_access_ex(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, ir_type_t *result_type, source_t code_source){
    int line = -1, column = -1;

    // If null checks enabled, remember origin line/column
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &line, &column);
    }

    return BUILD_VALUE(ir_instr_array_access_t, {
        .id = INSTRUCTION_ARRAY_ACCESS,
        .result_type = result_type,
        .value = value,
        .index = index,
        .maybe_line_number = line,
        .maybe_column_number = column,
    });
}

ir_value_t *build_member(ir_builder_t *builder, ir_value_t *value, length_t member, ir_type_t *result_type, source_t code_source){
    int line = -1, column = -1;

    // If null checks enabled, remember origin line/column
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &line, &column);
    }

    return BUILD_VALUE(ir_instr_member_t, {
        .id = INSTRUCTION_MEMBER,
        .result_type = result_type,
        .value = value,
        .member = member,
        .maybe_line_number = line,
        .maybe_column_number = column,
    });
}

void build_return(ir_builder_t *builder, ir_value_t *value){
    BUILD_INSTR(ir_instr_ret_t, {
        .id = INSTRUCTION_RET,
        .result_type = NULL,
        .value = value,
    });
}

ir_value_t *build_cast(ir_builder_t *builder, unsigned int const_cast_value_type, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t *to){
    if(VALUE_TYPE_IS_CONSTANT(from->value_type)){
        return build_const_cast(builder->pool, const_cast_value_type, from, to);
    } else {
        return build_nonconst_cast(builder, nonconst_cast_instr_type, from, to);
    }
}

ir_value_t *build_nonconst_cast(ir_builder_t *builder, unsigned int cast_instr_id, ir_value_t *from, ir_type_t* to){
    ir_instr_cast_t *instruction = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));

    *instruction = (ir_instr_cast_t){
        .id = cast_instr_id,
        .result_type = to,
        .value = from,
    };

    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type){
    return BUILD_VALUE(ir_instr_alloc_t, {
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type, false),
        .alignment = 0,
        .count = NULL,
    });
}

ir_value_t *build_alloc_array(ir_builder_t *builder, ir_type_t *type, ir_value_t *count){
    return BUILD_VALUE(ir_instr_alloc_t, {
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type, false),
        .alignment = 0,
        .count = count,
    });
}

ir_value_t *build_alloc_aligned(ir_builder_t *builder, ir_type_t *type, unsigned int alignment){
    return BUILD_VALUE(ir_instr_alloc_t, {
        .id = INSTRUCTION_ALLOC,
        .result_type = ir_type_make_pointer_to(builder->pool, type, false),
        .alignment = alignment,
        .count = NULL,
    });
}

ir_value_t *build_stack_save(ir_builder_t *builder){
    return BUILD_VALUE(ir_instr_t, {
        .id = INSTRUCTION_STACK_SAVE,
        .result_type = builder->object->ir_module.common.ir_ptr,
    });
}

void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer){
    BUILD_INSTR(ir_instr_unary_t, {
        .id = INSTRUCTION_STACK_RESTORE,
        .result_type = NULL,
        .value = stack_pointer,
    });
}

ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result){
    return BUILD_VALUE(ir_instr_math_t, {
        .id = instr_id,
        .a = a,
        .b = b,
        .result_type = result,
    });
}

ir_value_t *build_phi2(ir_builder_t *builder, ir_type_t *result_type, ir_value_t *a, ir_value_t *b, length_t landing_a_block_id, length_t landing_b_block_id){
    return BUILD_VALUE(ir_instr_phi2_t, {
        .id = INSTRUCTION_PHI2,
        .result_type = result_type,
        .a = a,
        .b = b,
        .block_id_a = landing_a_block_id,
        .block_id_b = landing_b_block_id,
    });
}

void build_llvm_asm(ir_builder_t *builder, bool is_intel, weak_cstr_t assembly, weak_cstr_t constraints, ir_value_t **args, length_t arity, bool has_side_effects, bool is_stack_align){
    BUILD_INSTR(ir_instr_asm_t, {
        .id = INSTRUCTION_ASM,
        .result_type = NULL,
        .assembly = assembly,
        .constraints = constraints,
        .args = args,
        .arity = arity,
        .is_intel = is_intel,
        .has_side_effects = has_side_effects,
        .is_stack_align = is_stack_align,
    });
}

void build_deinit_svars(ir_builder_t *builder){
    BUILD_INSTR(ir_instr_t, {
        .id = INSTRUCTION_DEINIT_SVARS,
        .result_type = NULL,
    });
}

void build_unreachable(ir_builder_t *builder){
    BUILD_INSTR(ir_instr_t, {
        .id = INSTRUCTION_UNREACHABLE,
        .result_type = NULL,
    });
}

void build_global_cleanup(ir_builder_t *builder){
    handle_deference_for_globals(builder);
    build_deinit_svars(builder);
}
