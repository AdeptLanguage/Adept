
#include "IR/ir.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "IR/ir_lowering.h"
#include "IR/ir_value_str.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

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

void ir_module_init(ir_module_t *ir_module, length_t funcs_capacity, length_t globals_length, length_t func_mappings_length_guess){
    ir_pool_init(&ir_module->pool);

    ir_module->funcs = (ir_funcs_t){
        .funcs = malloc(sizeof(ir_func_t) * funcs_capacity),
        .length = 0,
        .capacity = funcs_capacity,
    };

    ir_module->func_mappings = (ir_func_mappings_t){
        .mappings = malloc(sizeof(ir_func_mapping_t) * func_mappings_length_guess),
        .length = 0,
        .capacity = func_mappings_length_guess,
    };

    ir_module->methods = (ir_methods_t){0};
    ir_module->poly_methods = (ir_poly_methods_t){0};
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
    free(ir_module->func_mappings.mappings);
    free(ir_module->methods.methods);
    free(ir_module->poly_methods.methods);
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

void ir_module_insert_method(ir_module_t *module, weak_cstr_t name, weak_cstr_t struct_name, funcid_t ir_func_id, funcid_t ast_func_id, bool preserve_sortedness){
    ir_method_t method;
    method.name = name;
    method.struct_name = struct_name;
    method.ir_func_id = ir_func_id;
    method.ast_func_id = ast_func_id;
    method.is_beginning_of_group = -1;

    // Make room for the additional method
    expand((void**) &module->methods, sizeof(ir_method_t), module->methods.length, &module->methods.capacity, 1, 4);

    if(preserve_sortedness){
        // Find where to insert into the method list
        length_t insert_position = ir_module_find_insert_method_position(module, &method);
        
        // Move other methods over
        memmove(&module->methods.methods[insert_position + 1], &module->methods.methods[insert_position], sizeof(ir_method_t) * (module->methods.length - insert_position));

        // Invalidate whether method after is beginning of group
        if(insert_position != module->methods.length++)
            module->methods.methods[insert_position + 1].is_beginning_of_group = -1;

        // New method available
        memcpy(&module->methods.methods[insert_position], &method, sizeof(ir_method_t));
    } else {
        // New method available
        memcpy(&module->methods.methods[module->methods.length++], &method, sizeof(ir_method_t));
    }
}

void ir_module_insert_poly_method(ir_module_t *module, 
    weak_cstr_t name,
    weak_cstr_t struct_name,
    ast_type_t *weak_generics,
    length_t generics_length,
    funcid_t ir_func_id,
    funcid_t ast_func_id,
bool preserve_sortedness){
    
    ir_poly_method_t method;
    method.struct_name = struct_name;
    method.name = name;
    method.generics = weak_generics;
    method.generics_length = generics_length;
    method.ir_func_id = ir_func_id;
    method.ast_func_id = ast_func_id;
    method.is_beginning_of_group = -1;

    // Make room for the additional method
    expand((void**) &module->poly_methods, sizeof(ir_poly_method_t), module->poly_methods.length, &module->poly_methods.capacity, 1, 4);

    if(preserve_sortedness){
        // Find where to insert into the method list
        length_t insert_position = ir_module_find_insert_poly_method_position(module, &method);
        
        // Move other methods over
        memmove(
            &module->poly_methods.methods[insert_position + 1],
            &module->poly_methods.methods[insert_position],
            sizeof(ir_poly_method_t) * (module->poly_methods.length - insert_position)    
        );

        // Invalidate whether method after is beginning of group
        if(insert_position != module->poly_methods.length++)
            module->poly_methods.methods[insert_position + 1].is_beginning_of_group = -1;

        // New method available
        memcpy(&module->poly_methods.methods[insert_position], &method, sizeof(ir_poly_method_t));
    } else {
        // New method available
        memcpy(&module->poly_methods.methods[module->poly_methods.length++], &method, sizeof(ir_poly_method_t));
    }
}

ir_func_mapping_t *ir_module_insert_func_mapping(ir_module_t *module, weak_cstr_t name, funcid_t ir_func_id, funcid_t ast_func_id, bool preserve_sortedness){
    ir_func_mapping_t mapping, *result;
    mapping.name = name;
    mapping.ir_func_id = ir_func_id;
    mapping.ast_func_id = ast_func_id;
    mapping.is_beginning_of_group = -1;

    // Make room for the additional function mapping
    expand((void**) &module->func_mappings, sizeof(ir_func_mapping_t), module->func_mappings.length, &module->func_mappings.capacity, 1, 32);

    if(preserve_sortedness){
        // Find where to insert into the mappings list
        length_t insert_position = ir_module_find_insert_mapping_position(module, &mapping);
        
        // Move other mappings over
        memmove(
            &module->func_mappings.mappings[insert_position + 1],
            &module->func_mappings.mappings[insert_position],
            sizeof(ir_func_mapping_t) * (module->func_mappings.length - insert_position)    
        );
        
        // Invalidate whether mapping after is beginning of group
        if(insert_position != module->func_mappings.length++)
            module->func_mappings.mappings[insert_position + 1].is_beginning_of_group = -1;
        
        // New mapping available
        result = &module->func_mappings.mappings[insert_position];
    } else {
        // New mapping available
        result = &module->func_mappings.mappings[module->func_mappings.length++];
    }

    memcpy(result, &mapping, sizeof(ir_func_mapping_t));
    return result;
}

int ir_func_mapping_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_func_mapping_t*) a)->name, ((ir_func_mapping_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_func_mapping_t*) a)->ast_func_id - (int) ((ir_func_mapping_t*) b)->ast_func_id;
}

int ir_method_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_method_t*) a)->struct_name, ((ir_method_t*) b)->struct_name);
    if(diff != 0) return diff;
    diff = strcmp(((ir_method_t*) a)->name, ((ir_method_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_method_t*) a)->ast_func_id - (int) ((ir_method_t*) b)->ast_func_id;
}

int ir_poly_method_cmp(const void *a, const void *b){
    int diff = strcmp(((ir_poly_method_t*) a)->struct_name, ((ir_poly_method_t*) b)->struct_name);
    if(diff != 0) return diff;
    diff = strcmp(((ir_poly_method_t*) a)->name, ((ir_poly_method_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ir_poly_method_t*) a)->ast_func_id - (int) ((ir_poly_method_t*) b)->ast_func_id;
}

void ir_job_list_free(ir_job_list_t *job_list){
    free(job_list->jobs);
}

void ir_module_defer_free(ir_module_t *module, void *pointer){
    free_list_append(&module->defer_free, pointer);
}
