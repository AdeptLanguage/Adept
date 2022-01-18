
#include "IR/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IR/ir_lowering.h"
#include "IR/ir_value_str.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/datatypes.h"

successful_t ir_type_map_find(ir_type_map_t *type_map, char *name, ir_type_t **type_ptr){
    // Does a binary search on the type map to find the requested type by name

    ir_type_mapping_t *mappings = type_map->mappings;
    length_t mappings_length = type_map->mappings_length;
    int first = 0, middle, last = mappings_length - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            *type_ptr = &mappings[middle].type;
            return true;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return false;
}

void ir_basicblock_new_instructions(ir_basicblock_t *block, length_t amount){
    // NOTE: Ensures that there is enough room for 'amount' more instructions
    // NOTE: If there isn't, more memory will be allocated so they can be generated
    if(block->instructions.length + amount >= block->instructions.capacity){
        ir_instr_t **new_instructions = malloc(sizeof(ir_instr_t*) * block->instructions.capacity * 2);
        memcpy(new_instructions, block->instructions.instructions, sizeof(ir_instr_t*) * block->instructions.length);
        block->instructions.capacity *= 2;
        free(block->instructions.instructions);
        block->instructions.instructions = new_instructions;
    }
}

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
    ir_gen_sf_cache_init(&ir_module->sf_cache);
    ir_module->rtti_relocations = (rtti_relocations_t){0};
    ir_module->init_builder = NULL;
    ir_module->deinit_builder = NULL;
    ir_module->static_variables = (ir_static_variables_t){0};
    ir_module->job_list = (ir_job_list_t){0};
    ir_module->defer_free = (free_list_t){0};

    // Initialize common data
    ir_shared_common_t *common = &ir_module->common;
    common->ir_ubyte = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_ubyte->kind = TYPE_KIND_U8;
    common->ir_usize = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_usize->kind = TYPE_KIND_U64;
    common->ir_usize_ptr = ir_type_pointer_to(&ir_module->pool, common->ir_usize);
    common->ir_ptr = ir_type_pointer_to(&ir_module->pool, common->ir_ubyte);
    common->ir_bool = ir_pool_alloc(&ir_module->pool, sizeof(ir_type_t));
    common->ir_bool->kind = TYPE_KIND_BOOLEAN;
    common->rtti_array_index = 0;
    common->has_rtti_array = TROOLEAN_UNKNOWN;
    common->has_main = false;
    common->ast_main_id = 0;
    common->ir_main_id = 0;
}

void ir_basicblocks_free(ir_basicblocks_t *basicblocks){
    for(length_t i = 0; i < basicblocks->length; i++){
        ir_basicblock_free(&basicblocks->blocks[i]);
    }
    free(basicblocks->blocks);
}

void ir_module_free(ir_module_t *ir_module){
    ir_module_free_funcs(ir_module->funcs);
    free(ir_module->funcs.funcs);
    ir_proc_map_free(&ir_module->func_map);
    ir_proc_map_free(&ir_module->method_map);
    free(ir_module->type_map.mappings);
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
}

void ir_static_variables_free(ir_static_variables_t *static_variables){
    free(static_variables->variables);
}

void rtti_relocations_free(rtti_relocations_t *relocations){
    for(length_t i = 0; i != relocations->length; i++){
        free(relocations->relocations[i].human_notation);
    }
    free(relocations->relocations);
}

void free_list_free(free_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        free(list->pointers[i]);
    }
    free(list->pointers);
}

void ir_module_free_funcs(ir_funcs_t funcs){
    for(length_t f = 0; f != funcs.length; f++){
        ir_func_t *func = &funcs.funcs[f];

        ir_basicblocks_free(&func->basicblocks);
        free(func->argument_types);

        if(func->scope != NULL){
            bridge_scope_free(func->scope);
            free(func->scope);
        }
    }
}

void ir_basicblock_free(ir_basicblock_t *basicblock){
    free(basicblock->instructions.instructions);
}

char *ir_implementation_encoding = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
length_t ir_implementation_encoding_base;

void ir_implementation_setup(){
    ir_implementation_encoding_base = strlen(ir_implementation_encoding);
}

void ir_implementation(length_t id, char prefix, char *output_buffer){
    // NOTE: output_buffer is assumed to be able to hold 32 characters
    length_t length = 0;

    if(prefix != 0x00){
        output_buffer[length++] = prefix;
    }
    
    do {
        length_t digit = id % ir_implementation_encoding_base;
        output_buffer[length++] = ir_implementation_encoding[digit];
        id /= ir_implementation_encoding_base;

        if(length == 31){
            printf("INTERNAL ERROR: ir_implementation received too large of an id\n");
            break;
        }
    } while(id != 0);

    output_buffer[length] = 0x00;
}

unsigned long long ir_value_uniqueness_value(ir_pool_t *pool, ir_value_t **value){
    if(ir_lower_const_cast(pool, value) || (*value)->value_type != VALUE_TYPE_LITERAL){
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    };
    
    switch((*value)->type->kind){
    case TYPE_KIND_BOOLEAN: return *((adept_bool*) (*value)->extra);
    case TYPE_KIND_S8:      return *((adept_byte*) (*value)->extra);
    case TYPE_KIND_U8:      return *((adept_ubyte*) (*value)->extra);
    case TYPE_KIND_S16:     return *((adept_short*) (*value)->extra);
    case TYPE_KIND_U16:     return *((adept_ushort*) (*value)->extra);
    case TYPE_KIND_S32:     return *((adept_int*) (*value)->extra);
    case TYPE_KIND_U32:     return *((adept_uint*) (*value)->extra);
    case TYPE_KIND_S64:     return *((adept_long*) (*value)->extra);
    case TYPE_KIND_U64:     return *((adept_ulong*) (*value)->extra);
    default:
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    }
}

void ir_print_value(ir_value_t *value){
    strong_cstr_t s = ir_value_str(value);
    printf("%s\n", s);
    free(s);
}

void ir_print_type(ir_type_t *type){
    strong_cstr_t s = ir_type_str(type);
    printf("%s\n", s);
    free(s);
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

void ir_job_list_free(ir_job_list_t *job_list){
    free(job_list->jobs);
}

void ir_module_defer_free(ir_module_t *module, void *pointer){
    free_list_append(&module->defer_free, pointer);
}
