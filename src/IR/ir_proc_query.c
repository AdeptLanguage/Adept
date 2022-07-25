
#include "IR/ir_proc_query.h"

#include <stddef.h>

void ir_proc_query_init_find_func_regular(
    ir_proc_query_t *out_query,
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t name,
    ast_type_t *arg_types,
    length_t length,
    trait_t traits_mask,
    trait_t traits_match,
    trait_t forbid_traits,
    source_t from_source
){
    *out_query = (ir_proc_query_t){
        .conform = false,
        .rigid_params = (ir_proc_query_rigid_params_t){
            .compiler = compiler,
            .object = object,
        },
        .allow_default_values = false,
        .without_defaults = (ir_proc_query_without_defaults_t){
            .arg_values = NULL,
            .arg_types = arg_types,
            .length = length,
        },
        .proc_name = name,
        .struct_name = NULL,
        .traits_mask = traits_mask,
        .traits_match = traits_match,
        .forbid_traits = forbid_traits,
        .optional_gives = NULL,
        .from_source = from_source,
    };
}

void ir_proc_query_init_find_method_regular(
    ir_proc_query_t *out_query,
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t name,
    ast_type_t *arg_types,
    length_t length,
    trait_t forbid_traits,
    source_t from_source
){
    ir_proc_query_init_find_func_regular(out_query, compiler, object, name, arg_types, length, TRAIT_NONE, TRAIT_NONE, forbid_traits, from_source);
    out_query->struct_name = struct_name;
}

void ir_proc_query_init_find_func_conforming(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    trait_t forbid_traits,
    source_t from_source
){
    *out_query = (ir_proc_query_t){
        .conform = true,
        .conform_params = (ir_proc_query_conform_params_t){
            .builder = builder,
            .no_user_casts = no_user_casts,
        },
        .allow_default_values = true,
        .with_defaults = (ir_proc_query_with_defaults_t){
            .inout_arg_values = inout_arg_values,
            .inout_arg_types = inout_arg_types,
            .inout_length = inout_length,
        },
        .proc_name = name,
        .struct_name = NULL,
        .traits_mask = TRAIT_NONE,
        .traits_match = TRAIT_NONE,
        .forbid_traits = forbid_traits,
        .optional_gives = optional_gives,
        .from_source = from_source,
    };
}

void ir_proc_query_init_find_method_conforming(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    trait_t forbid_traits,
    source_t from_source
){
    ir_proc_query_init_find_func_conforming(out_query, builder, name, inout_arg_values, inout_arg_types, inout_length, optional_gives, false, forbid_traits, from_source);
    out_query->struct_name = struct_name;
}

void ir_proc_query_init_find_func_conforming_without_defaults(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    trait_t forbid_traits,
    source_t from_source
){
    *out_query = (ir_proc_query_t){
        .conform = true,
        .conform_params = (ir_proc_query_conform_params_t){
            .builder = builder,
            .no_user_casts = no_user_casts,
        },
        .allow_default_values = false,
        .without_defaults = (ir_proc_query_without_defaults_t){
            .arg_values = arg_values,
            .arg_types = arg_types,
            .length = length,
        },
        .proc_name = name,
        .struct_name = NULL,
        .traits_mask = TRAIT_NONE,
        .traits_match = TRAIT_NONE,
        .forbid_traits = forbid_traits,
        .optional_gives = optional_gives,
        .from_source = from_source,
    };
}

void ir_proc_query_init_find_method_conforming_without_defaults(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    trait_t forbid_traits,
    source_t from_source
){
    ir_proc_query_init_find_func_conforming_without_defaults(out_query, builder, name, arg_values, arg_types, length, optional_gives, false, forbid_traits, from_source);
    out_query->struct_name = struct_name;
}

compiler_t *ir_proc_query_getter_compiler(ir_proc_query_t *query){
    if(query->conform){
        return query->conform_params.builder->compiler;
    } else {
        return query->rigid_params.compiler;
    }
}

object_t *ir_proc_query_getter_object(ir_proc_query_t *query){
    if(query->conform){
        return query->conform_params.builder->object;
    } else {
        return query->rigid_params.object;
    }
}

bool ir_proc_query_is_function(ir_proc_query_t *query){
    return query->struct_name == NULL;
}

bool ir_proc_query_is_method(ir_proc_query_t *query){
    return query->struct_name != NULL;
}

ir_value_t **ir_proc_query_getter_arg_values(ir_proc_query_t *query){
    if(query->allow_default_values){
        ir_value_t ***p = query->with_defaults.inout_arg_values;
        return p ? *p : NULL;
    } else {
        return query->without_defaults.arg_values;
    }
}

ast_type_t *ir_proc_query_getter_arg_types(ir_proc_query_t *query){
    if(query->allow_default_values){
        return *query->with_defaults.inout_arg_types;
    } else {
        return query->without_defaults.arg_types;
    }
}

length_t ir_proc_query_getter_length(ir_proc_query_t *query){
    if(query->allow_default_values){
        return *query->with_defaults.inout_length;
    } else {
        return query->without_defaults.length;
    }
}
