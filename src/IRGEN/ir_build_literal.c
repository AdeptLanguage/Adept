
#include "IRGEN/ir_build_literal.h"

ir_value_t *build_struct_literal(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable){
    // Create struct literal
    ir_value_t *result = ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_LITERAL,
        .type = type,
        .extra = ir_pool_alloc_init(&module->pool, ir_value_struct_literal_t, {
            .values = values,
            .length = length,
        })
    });

    // If mutability is required, then create an anonymous global for the structure data
    if(make_mutable){
        // Create mutable anonymous global and use it instead of constant
        result = ir_module_create_anon_global(module, type, true, result);
    }

    return result;
}

ir_value_t *build_const_struct_literal(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CONST_STRUCT_LITERAL,
        .type = type,
        .extra = ir_pool_alloc_init(pool, ir_value_const_struct_literal_t, {
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

ir_value_t *build_array_literal(ir_pool_t *pool, ir_type_t *item_type, ir_value_t **values, length_t length){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_ARRAY_LITERAL,
        .type = ir_type_make_pointer_to(pool, item_type),
        .extra = ir_pool_alloc_init(pool, ir_value_array_literal_t, {
            .values = values,
            .length = length,
        })
    });
}

ir_value_t *build_literal_int(ir_pool_t *pool, adept_int value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_S32, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}

ir_value_t *build_literal_usize(ir_pool_t *pool, adept_usize value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_U64, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}

ir_value_t *build_unknown_enum_value(ir_pool_t *pool, source_t source, weak_cstr_t kind_name){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_UNKNOWN_ENUM,
        .type = ir_type_make_unknown_enum(pool, source, kind_name),
        .extra = NULL,
    });
}

ir_value_t *build_literal_str(ir_builder_t *builder, char *array, length_t length){
    ir_type_t *ir_string_type = builder->object->ir_module.common.ir_string_struct;

    if(ir_string_type == NULL){
        redprintf("Can't create string literal without String type present");
        printf("\nTry importing '%s/String.adept'\n", ADEPT_VERSION_STRING);
        return NULL;
    }

    length_t num_fields = 4;
    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * num_fields);
    values[0] = build_literal_cstr_of_size(builder, array, length);
    values[1] = build_literal_usize(builder->pool, length);
    values[2] = build_literal_usize(builder->pool, length);
    values[3] = build_literal_usize(builder->pool, 0); // DANGEROUS: This is a hack to initialize String.ownership as a reference

    return ir_pool_alloc_init(builder->pool, ir_value_t, {
        .value_type = VALUE_TYPE_STRUCT_LITERAL,
        .type = ir_string_type,
        .extra = ir_pool_alloc_init(builder->pool, ir_value_struct_literal_t, {
            .values = values,
            .length = num_fields,
        })
    });
}

ir_value_t *build_literal_cstr(ir_builder_t *builder, weak_cstr_t value){
    return build_literal_cstr_of_size(builder, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value){
    return build_literal_cstr_of_size_ex(pool, type_map, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_of_size(ir_builder_t *builder, char *array, length_t size){
    return build_literal_cstr_of_size_ex(builder->pool, builder->type_map, array, size);
}

ir_value_t *build_literal_cstr_of_size_ex(ir_pool_t *pool, ir_type_map_t *type_map, char *array, length_t size){
    ir_type_t *ir_ubyte_type;

    if(!ir_type_map_find(type_map, "ubyte", &ir_ubyte_type)){
        die("build_literal_cstr_of_size_ex() - Failed to find 'ubyte' type mapping\n");
    }

    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_CSTR_OF_LEN,
        .type = ir_type_make_pointer_to(pool, ir_ubyte_type),
        .extra = ir_pool_alloc_init(pool, ir_value_cstr_of_len_t, {
            .array = array,
            .size = size,
        })
    });
}

ir_value_t *build_null_pointer(ir_pool_t *pool){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_NULLPTR,
        .type = ir_type_make_pointer_to(pool, ir_type_make(pool, TYPE_KIND_U8, NULL)),
        .extra = NULL,
    });
}

ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_NULLPTR_OF_TYPE,
        .type = type,
        .extra = NULL,
    });
}

ir_value_t *build_func_addr(ir_pool_t *pool, ir_type_t *result_type, func_id_t ir_func_id){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_FUNC_ADDR,
        .type = result_type,
        .extra = ir_pool_alloc_init(pool, ir_value_func_addr_t, {
            .ir_func_id = ir_func_id,
        })
    });
}

ir_value_t *build_func_addr_by_name(ir_pool_t *pool, ir_type_t *result_type, const char *name){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_FUNC_ADDR_BY_NAME,
        .type = result_type,
        .extra = ir_pool_alloc_init(pool, ir_value_func_addr_by_name_t, {
            .name = name,
        })
    });
}

ir_value_t *build_const_cast(ir_pool_t *pool, unsigned int cast_value_type, ir_value_t *from, ir_type_t *to){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = cast_value_type,
        .type = to,
        .extra = from,
    });
}

ir_value_t *build_bool(ir_pool_t *pool, adept_bool value){
    return ir_pool_alloc_init(pool, ir_value_t, {
        .value_type = VALUE_TYPE_LITERAL,
        .type = ir_type_make(pool, TYPE_KIND_BOOLEAN, NULL),
        .extra = ir_pool_memclone(pool, &value, sizeof value),
    });
}
