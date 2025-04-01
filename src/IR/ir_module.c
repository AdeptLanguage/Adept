
#include "IR/ir_module.h"

#include <stdlib.h>
#include <string.h>

#include "IR/ir_type.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/builtin_type.h"

static rtti_collector_t *create_rtti_collector(ir_pool_t *pool){
    rtti_collector_t *rtti_collector = ir_pool_alloc(pool, sizeof(rtti_collector_t));
    rtti_collector_init(rtti_collector);

    // Mention builtin primitive types to RTTI collector
    for(length_t i = 0; i < NUM_ITEMS(global_primitives_extended); i++){
        rtti_collector_mention_base(rtti_collector, global_primitives_extended[i]);
    } 

    return rtti_collector;
}

void ir_module_init(ir_module_t *ir_module, length_t funcs_capacity, length_t globals_length, length_t number_of_function_names_guess){
    ir_pool_t *pool = &ir_module->pool;
    ir_pool_init(pool);

    ir_module->funcs = (ir_funcs_t){
        .funcs = malloc(sizeof(ir_func_t) * funcs_capacity),
        .length = 0,
        .capacity = funcs_capacity,
    };

    ir_proc_map_init(&ir_module->func_map, sizeof(ir_func_key_t), number_of_function_names_guess);
    ir_proc_map_init(&ir_module->method_map, sizeof(ir_method_key_t), 0);

    ir_module->type_map.mappings = NULL;
    ir_module->globals = malloc(sizeof(ir_global_t) * globals_length);
    ir_module->globals_length = 0;
    ir_module->anon_globals = (ir_anon_globals_t){0};

    ir_gen_sf_cache_init(&ir_module->sf_cache, IR_GEN_SF_CACHE_SUGGESTED_NUM_BUCKETS);

    ir_module->rtti_collector = create_rtti_collector(pool);
    ir_module->rtti_table = NULL;

    ir_module->rtti_relocations = (rtti_relocations_t){0};
    ir_module->init_builder = NULL;
    ir_module->deinit_builder = NULL;
    ir_module->static_variables = (ir_static_variables_t){0};
    ir_module->job_list = (ir_job_list_t){0};
    ir_module->defer_free = (free_list_t){0};
    ir_module->vtable_init_list = (ir_vtable_init_list_t){0};
    ir_module->vtable_dispatch_list = (ir_vtable_dispatch_list_t){0};

    // Create shared resources
    ir_module->common = (ir_shared_common_t){
        .ir_ubyte = ir_type_make(pool, TYPE_KIND_U8, NULL),
        .ir_ubyte_ptr = ir_type_make_pointer_to(pool, ir_type_make(pool, TYPE_KIND_U8, NULL), false),
        .ir_usize = ir_type_make(pool, TYPE_KIND_U64, NULL),
        .ir_usize_ptr = ir_type_make_pointer_to(&ir_module->pool, ir_type_make(pool, TYPE_KIND_U64, NULL), false),
        .ir_ptr = ir_type_make_pointer_to(pool, ir_type_make(pool, TYPE_KIND_U8, NULL), false),
        .ir_bool = ir_type_make(pool, TYPE_KIND_BOOLEAN, NULL),
        .has_rtti_array = TROOLEAN_UNKNOWN,
        /* rest zero initialized */
    };
}

void ir_module_free(ir_module_t *ir_module){
    ir_funcs_free(ir_module->funcs);
    ir_proc_map_free(&ir_module->func_map);
    ir_proc_map_free(&ir_module->method_map);
    ir_type_map_free(&ir_module->type_map);
    free(ir_module->globals);
    free(ir_module->anon_globals.globals);
    ir_gen_sf_cache_free(&ir_module->sf_cache);

    // Free init_builder
    if(ir_module->init_builder){
        ir_basicblocks_free(&ir_module->init_builder->basicblocks);
        free(ir_module->init_builder);
    }

    // Free deinit_builder
    if(ir_module->deinit_builder){
        ir_basicblocks_free(&ir_module->deinit_builder->basicblocks);
        free(ir_module->deinit_builder);
    }

    ir_static_variables_free(&ir_module->static_variables);

    if(ir_module->rtti_collector){
        rtti_collector_free(ir_module->rtti_collector);
    }

    if(ir_module->rtti_table){
        rtti_table_free(ir_module->rtti_table);
    }

    rtti_relocations_free(&ir_module->rtti_relocations);
    ir_job_list_free(&ir_module->job_list);
    free_list_free(&ir_module->defer_free);
    ir_vtable_init_list_free(&ir_module->vtable_init_list);
    ir_vtable_dispatch_list_free(&ir_module->vtable_dispatch_list);

    ir_pool_free(&ir_module->pool);
}

void ir_module_create_func_mapping(ir_module_t *module, weak_cstr_t function_name, ir_func_endpoint_t endpoint, bool add_to_job_list){
    ir_func_key_t key = (ir_func_key_t){
        .name = function_name,
    };

    ir_proc_map_insert(&module->func_map, &key, sizeof key, endpoint, &compare_ir_func_key);

    if(add_to_job_list){
        ir_job_list_append(&module->job_list, endpoint);
    }
}

void ir_module_create_method_mapping(ir_module_t *module, weak_cstr_t struct_name, weak_cstr_t method_name, ir_func_endpoint_t endpoint){
    ir_method_key_t key = (ir_method_key_t){
        .method_name = method_name,
        .struct_name = struct_name,
    };

    ir_proc_map_insert(&module->method_map, &key, sizeof key, endpoint, &compare_ir_method_key);
}

ir_value_t *ir_module_create_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant, ir_value_t *initializer_or_null){
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
            .initializer = initializer_or_null,
        }
    ));

    length_t anon_global_id = module->anon_globals.length - 1;

    return ir_pool_alloc_init(&module->pool, ir_value_t, {
        .value_type = value_type,
        .type = ir_type_make_pointer_to(&module->pool, type, false),
        .extra = ir_pool_alloc_init(&module->pool, ir_value_anon_global_t, {
            .anon_global_id = anon_global_id,
        })
    });
}

void ir_module_set_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer){
    switch(anon_global->value_type){
    case VALUE_TYPE_ANON_GLOBAL:
    case VALUE_TYPE_CONST_ANON_GLOBAL: {
            ir_value_anon_global_t *extra = (ir_value_anon_global_t*) anon_global->extra;
            module->anon_globals.globals[extra->anon_global_id].initializer = initializer;
        }
        break;
    default:
        internalerrorprintf("ir_module_set_anon_global_initializer() - Cannot set initializer on a value that isn't a reference to an anonymous global variable\n");
    }
}

void ir_module_defer_free(ir_module_t *module, void *pointer){
    free_list_append(&module->defer_free, pointer);
}
