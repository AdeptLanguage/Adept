
#include "assemble_find.h"
#include "assemble_type.h"

int assemble_find_func(compiler_t *compiler, object_t *object, const char *name,
        ast_type_t *arg_types, length_t arg_types_length, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    int index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->funcs_length, name);
    if(index == -1) return 1;

    ir_func_mapping_t *mapping = &ir_module->func_mappings[index];

    if(func_args_match(mapping->ast_func, arg_types, arg_types_length)){
        result->ast_func = mapping->ast_func;
        result->ir_func = mapping->module_func;
        result->func_id = mapping->func_id;
        return 0;
    }

    while(++index != ir_module->funcs_length){
        mapping = &ir_module->func_mappings[index];

        if(mapping->is_beginning_of_group == -1){
            mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
        }
        if(mapping->is_beginning_of_group == 1) return 1;

        if(func_args_match(mapping->ast_func, arg_types, arg_types_length)){
            result->ast_func = mapping->ast_func;
            result->ir_func = mapping->module_func;
            result->func_id = mapping->func_id;
            return 0;
        }
    }

    return 1; // No function with that definition found
}

int assemble_find_func_named(compiler_t *compiler, object_t *object,
        const char *name, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    int index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->funcs_length, name);
    if(index == -1) return 1;

    ir_func_mapping_t *mapping = &ir_module->func_mappings[index];
    result->ast_func = mapping->ast_func;
    result->ir_func = mapping->module_func;
    result->func_id = mapping->func_id;
    return 0;
}

int assemble_find_func_conforming(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result){
    ir_module_t *ir_module = &builder->object->ir_module;

    int index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->funcs_length, name);
    if(index == -1) return 1;

    ir_func_mapping_t *mapping = &ir_module->func_mappings[index];

    if(func_args_conform(builder, mapping->ast_func, arg_values, arg_types, type_list_length)){
        result->ast_func = mapping->ast_func;
        result->ir_func = mapping->module_func;
        result->func_id = mapping->func_id;
        return 0;
    }

    while(++index != ir_module->funcs_length){
        mapping = &ir_module->func_mappings[index];

        if(mapping->is_beginning_of_group == -1){
            mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
        }
        if(mapping->is_beginning_of_group == 1) return 1;

        if(func_args_conform(builder, mapping->ast_func, arg_values, arg_types, type_list_length)){
            result->ast_func = mapping->ast_func;
            result->ir_func = mapping->module_func;
            result->func_id = mapping->func_id;
            return 0;
        }
    }

    return 1; // No function with that definition found
}

int assemble_find_method_conforming(ir_builder_t *builder, const char *struct_name,
        const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result){

    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &builder->object->ir_module;

    int index = find_beginning_of_method_group(ir_module->methods, ir_module->methods_length, struct_name, name);
    if(index == -1) return 1;

    ir_method_t *method = &ir_module->methods[index];

    if(func_args_conform(builder, method->ast_func, arg_values, arg_types, type_list_length)){
        result->ast_func = method->ast_func;
        result->ir_func = method->module_func;
        result->func_id = method->func_id;
        return 0;
    }

    while(++index != ir_module->methods_length){
        method = &ir_module->methods[index];

        if(method->is_beginning_of_group == -1){
            method->is_beginning_of_group = (strcmp(method->name, ir_module->methods[index - 1].name) != 0);
        }
        if(method->is_beginning_of_group == 1) return 1;

        if(func_args_conform(builder, method->ast_func, arg_values, arg_types, type_list_length)){
            result->ast_func = method->ast_func;
            result->ir_func = method->module_func;
            result->func_id = method->func_id;
            return 0;
        }
    }

    return 1; // No method with that definition found
}

int find_beginning_of_func_group(ir_func_mapping_t *mappings, length_t length, const char *name){
    // Searches for beginning of function group in a list of mappings
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    int first, middle, last, comparison;
    first = 0; last = length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            while(true){
                if(mappings[middle].is_beginning_of_group == -1){
                    // It is uncalculated, so we must calculate it
                    bool prev_different_name = middle == 0 ? true : strcmp(mappings[middle - 1].name, name) != 0;
                    mappings[middle].is_beginning_of_group = prev_different_name;
                }
                if(mappings[middle].is_beginning_of_group == 1) return middle;
                // Not the beginning of the group, check the previous if it is
                middle--; // DANGEROUS: Potential for negative index
            }

            return middle;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

int find_beginning_of_method_group(ir_method_t *methods, length_t length,
    const char *struct_name, const char *name){
    // Searches for beginning of method group in a list of methods
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    int first, middle, last, comparison;
    first = 0; last = length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(methods[middle].struct_name, struct_name);

        if(comparison == 0){
            comparison = strcmp(methods[middle].name, name);

            if(comparison == 0){
                while(true){
                    if(methods[middle].is_beginning_of_group == -1){
                        // It is uncalculated, so we must calculate it
                        bool prev_different_name = middle == 0 ? true : strcmp(methods[middle - 1].name, name) != 0;
                        methods[middle].is_beginning_of_group = prev_different_name;
                    }
                    if(methods[middle].is_beginning_of_group == 1){
                        return middle;
                    }
                    // Not the beginning of the group, check the previous if it is
                    middle--; // DANGEROUS: Potential for negative index
                }

                return middle;
            }
            else if(comparison > 0) last = middle - 1;
            else first = middle + 1;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

bool func_args_match(ast_func_t *func, ast_type_t *type_list, length_t type_list_length){
    ast_type_t *arg_types = func->arg_types;
    length_t args_count = func->arity;

    if(func->traits & AST_FUNC_VARARG){
        if(func->arity > type_list_length) return false;
    } else {
        if(args_count != type_list_length) return false;
    }

    for(length_t a = 0; a != args_count; a++){
        if(!ast_types_identical(&arg_types[a], &type_list[a])) return false;
    }

    return true;
}

bool func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
        ast_type_t *arg_type_list, length_t type_list_length){
    ast_type_t *arg_types = func->arg_types;
    length_t args_count = func->arity;

    if(func->traits & AST_FUNC_VARARG){
        if(func->arity > type_list_length) return false;
    } else {
        if(args_count != type_list_length) return false;
    }

    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);

    // Store a copy of the unmodifed function argument values
    ir_value_t **arg_value_list_unmodified = malloc(sizeof(ir_value_t*) * type_list_length);
    memcpy(arg_value_list_unmodified, arg_value_list, sizeof(ir_value_t*) * type_list_length);

    for(length_t a = 0; a != args_count; a++){
        if(!ast_types_conform(builder, &arg_value_list[a], &arg_type_list[a], &arg_types[a], CONFORM_MODE_PRIMITIVES)){
            // Restore pool snapshot
            ir_pool_snapshot_restore(builder->pool, &snapshot);

            // Undo any modifications to the function arguments
            memcpy(arg_value_list, arg_value_list_unmodified, sizeof(ir_value_t*) * (a + 1));
            free(arg_value_list_unmodified);
            return false;
        }
    }

    free(arg_value_list_unmodified);
    return true;
}
