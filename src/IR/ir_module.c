
#include "IR/ir_module.h"
#include "IRGEN/ir_builder.h"

void ir_module_init(ir_module_t *ir_module, length_t funcs_capacity, length_t globals_length, length_t number_of_function_names_guess){
    ir_pool_init(&ir_module->pool);

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
    ir_module->rtti_relocations = (rtti_relocations_t){0};
    ir_module->init_builder = NULL;
    ir_module->deinit_builder = NULL;
    ir_module->static_variables = (ir_static_variables_t){0};
    ir_module->job_list = (ir_job_list_t){0};
    ir_module->defer_free = (free_list_t){0};
    ir_module->vtable_init_list = (ir_vtable_init_list_t){0};
    ir_module->vtable_dispatch_list = (ir_vtable_dispatch_list_t){0};

    // Initialize common data
    ir_pool_t *pool = &ir_module->pool;
    ir_shared_common_t *common = &ir_module->common;

    memset(common, 0, sizeof *common);

    common->ir_ubyte = ir_type_make(pool, TYPE_KIND_U8, NULL);
    common->ir_usize = ir_type_make(pool, TYPE_KIND_U64, NULL);
    common->ir_usize_ptr = ir_type_make_pointer_to(&ir_module->pool, common->ir_usize);
    common->ir_ptr = ir_type_make_pointer_to(pool, common->ir_ubyte);
    common->ir_bool = ir_type_make(pool, TYPE_KIND_BOOLEAN, NULL);

    common->has_rtti_array = TROOLEAN_UNKNOWN;
}

void ir_module_free(ir_module_t *ir_module){
    ir_funcs_free(ir_module->funcs);
    ir_proc_map_free(&ir_module->func_map);
    ir_proc_map_free(&ir_module->method_map);
    ir_type_map_free(&ir_module->type_map);
    free(ir_module->globals);
    free(ir_module->anon_globals.globals);
    ir_pool_free(&ir_module->pool);
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
    rtti_relocations_free(&ir_module->rtti_relocations);
    ir_job_list_free(&ir_module->job_list);
    free_list_free(&ir_module->defer_free);
    ir_vtable_init_list_free(&ir_module->vtable_init_list);
    ir_vtable_dispatch_list_free(&ir_module->vtable_dispatch_list);
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


void ir_module_defer_free(ir_module_t *module, void *pointer){
    free_list_append(&module->defer_free, pointer);
}
