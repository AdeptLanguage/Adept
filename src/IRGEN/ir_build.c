
#include "IRGEN/ir_build.h"
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
        .result_type = ir_type_make_pointer_to(builder->pool, type),
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

ir_value_t *build_call_address(ir_builder_t *builder, ir_type_t *return_type, ir_value_t *address, ir_value_t **arg_values, length_t arity){
    return BUILD_VALUE(ir_instr_call_address_t, {
        .id = INSTRUCTION_CALL_ADDRESS,
        .result_type = return_type,
        .address = address,
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

ir_value_t *build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable){
    ir_value_t *value = ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_LITERAL,
        .type = type,
        .extra = ir_pool_alloc_init(&module->pool, ir_value_struct_literal_t, {
            .values = values,
            .length = length,
        })
    });

    // If mutability is required, then create an anonymous global for the structure data
    if(make_mutable){
        // Create anonymous global
        ir_value_t *anonymous_global_variable = build_anon_global(module, type, true);

        // Set initializer for anonymous global
        build_anon_global_initializer(module, anonymous_global_variable, value);

        // Use mutable anonymous global instead of constant
        value = anonymous_global_variable;
    }

    return value;
}

ir_value_t *build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_CONSTRUCTION,
        .type = type,
        .extra = ir_pool_alloc_init(pool, ir_value_struct_construction_t, {
            .values = values,
            .length = length,
        })
    });
}

ir_value_t *build_offsetof(ir_builder_t *builder, ir_type_t *type, length_t index){
    return build_offsetof_ex(builder->pool, ir_builder_usize(builder), type, index);
}

ir_value_t *build_offsetof_ex(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type, length_t index){
    if(type->kind != TYPE_KIND_STRUCTURE){
        die("build_offsetof() - Cannot get offset of field for non-struct type\n");
    }

    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_OFFSETOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_offsetof_t, {
            .type = type,
            .index = index,
        })
    });
}

ir_value_t *build_const_sizeof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_SIZEOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_sizeof_t, {
            .type = type,
        })
    });
}

ir_value_t *build_const_alignof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_ALIGNOF,
        .type = usize_type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_alignof_t, {
            .type = type,
        })
    });
}

ir_value_t *build_const_add(ir_pool_t *pool, ir_value_t *a, ir_value_t *b){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_ADD,
        .type = a->type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_math_t, {
            .a = a,
            .b = b,
        })
    });
}

ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *item_type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_ARRAY_LITERAL,
        .type = ir_type_make_pointer_to(pool, item_type),
        .extra = ir_pool_alloc_init(pool, ir_value_array_literal_t, {
            .values = values,
            .length = length,
        })
    });
}

ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant){
    unsigned int value_type;
    trait_t traits;

    if(is_constant){
        value_type = VALUE_TYPE_CONST_ANON_GLOBAL;
        traits = IR_ANON_GLOBAL_CONSTANT;
    } else {
        value_type = VALUE_TYPE_ANON_GLOBAL;
        traits = TRAIT_NONE;
    }

    ir_anon_globals_append(&module->anon_globals, (
        (ir_anon_global_t){
            .type = type,
            .traits = traits,
            .initializer = NULL,
        }
    ));

    length_t anon_global_id = module->anon_globals.length - 1;

    return ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = value_type,
        .type = ir_type_make_pointer_to(&module->pool, type),
        .extra = ir_pool_alloc_init(&module->pool, ir_value_anon_global_t, {
            .anon_global_id = anon_global_id,
        })
    });
}

void build_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer){
    switch(anon_global->value_type){
    case VALUE_TYPE_ANON_GLOBAL:
    case VALUE_TYPE_CONST_ANON_GLOBAL: {
            ir_value_anon_global_t *extra = (ir_value_anon_global_t*) anon_global->extra;
            module->anon_globals.globals[extra->anon_global_id].initializer = initializer;
        }
        break;
    default:
        internalerrorprintf("build_anon_global_initializer() - Cannot set initializer on a value that isn't a reference to an anonymous global variable\n");
    }
}
