
#include "BRIDGE/rtti.h"

ir_value_t* rtti_for(ir_builder_t *builder, ast_type_t *target, source_t sourceOnFailure){
    // Kinda hacky way of doing it but whatever
    strong_cstr_t readable_name = ast_type_str(target);
    maybe_index_t found_type_index = type_table_find(builder->object->ast.type_table, readable_name);
    if(found_type_index == -1){
        compiler_panicf(builder->compiler, sourceOnFailure, "INTERNAL ERROR: typeinfo failed to find info for type '%s', which should exist", readable_name);
        free(readable_name);
        return NULL;
    }
    free(readable_name);

    // TODO: Clean up this up
    // It works, but the global variable lookup could be better
    bool found_global = false;
    length_t var_index;
    
    for(var_index = 0; var_index != builder->object->ir_module.globals_length; var_index++){
        if(strcmp("__types__", builder->object->ir_module.globals[var_index].name) == 0){
            found_global = true; break;
        }
    }
    
    if(!found_global){
        compiler_panic(builder->compiler, sourceOnFailure, "Failed to find __types__ global variable");
        return NULL;
    }

    ir_global_t *global = &builder->object->ir_module.globals[var_index];
    ir_value_t *rtti = build_load(builder, build_gvarptr(builder, ir_type_pointer_to(builder->pool, global->type), var_index));
    rtti = build_load(builder, build_array_access(builder, rtti, build_literal_usize(builder->pool, found_type_index)));
    return rtti;
}
