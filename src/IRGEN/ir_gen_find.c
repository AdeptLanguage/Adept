
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_type.h"

errorcode_t ir_gen_find_func(compiler_t *compiler, object_t *object, const char *name,
        ast_type_t *arg_types, length_t arg_types_length, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    maybe_index_t index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->func_mappings_length, name);
    if(index == -1) return FAILURE;

    ir_func_mapping_t *mapping = &ir_module->func_mappings[index];
    ast_func_t *ast_func = &object->ast.funcs[mapping->ast_func_id];

    if(func_args_match(ast_func, arg_types, arg_types_length)){
        result->ast_func = ast_func;
        result->ir_func = &object->ir_module.funcs[mapping->ir_func_id];
        result->ast_func_id = mapping->ast_func_id;
        result->ir_func_id = mapping->ir_func_id;
        return SUCCESS;
    }

    while(++index != ir_module->funcs_length){
        mapping = &ir_module->func_mappings[index];

        if(mapping->is_beginning_of_group == -1){
            mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
        }
        if(mapping->is_beginning_of_group == 1) return FAILURE;

        if(func_args_match(ast_func, arg_types, arg_types_length)){
            result->ast_func = ast_func;
            result->ir_func = &object->ir_module.funcs[mapping->ir_func_id];
            result->ast_func_id = mapping->ast_func_id;
            result->ir_func_id = mapping->ir_func_id;
            return SUCCESS;
        }
    }

    return FAILURE; // No function with that definition found
}

errorcode_t ir_gen_find_func_named(compiler_t *compiler, object_t *object,
        const char *name, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    maybe_index_t index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->func_mappings_length, name);
    if(index == -1) return FAILURE;

    ir_func_mapping_t *mapping = &ir_module->func_mappings[index];
    result->ast_func = &object->ast.funcs[mapping->ast_func_id];
    result->ir_func = &object->ir_module.funcs[mapping->ir_func_id];
    result->ast_func_id = mapping->ast_func_id;
    result->ir_func_id = mapping->ir_func_id;
    return SUCCESS;
}

errorcode_t ir_gen_find_func_conforming(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, funcpair_t *result){
    
    ir_module_t *ir_module = &builder->object->ir_module;
    maybe_index_t index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->func_mappings_length, name);

    if(index != -1){
        ir_func_mapping_t *mapping = &ir_module->func_mappings[index];
        ast_func_t *ast_func = &builder->object->ast.funcs[mapping->ast_func_id];

        if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
            result->ast_func = ast_func;
            result->ir_func = &builder->object->ir_module.funcs[mapping->ir_func_id];
            result->ast_func_id = mapping->ast_func_id;
            result->ir_func_id = mapping->ir_func_id;
            return SUCCESS;
        }

        while(++index != ir_module->funcs_length){
            mapping = &ir_module->func_mappings[index];
            ast_func = &builder->object->ast.funcs[mapping->ast_func_id];

            if(mapping->is_beginning_of_group == -1){
                mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
            }
            if(mapping->is_beginning_of_group == 1) break;

            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
                result->ast_func = ast_func;
                result->ir_func = &builder->object->ir_module.funcs[mapping->ir_func_id];
                result->ast_func_id = mapping->ast_func_id;
                result->ir_func_id = mapping->ir_func_id;
                return SUCCESS;
            }
        }
    }

    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_funcs, ast->polymorphic_funcs_length, name);

    if(poly_index != -1){
        ast_polymorphic_func_t *poly_func = &ast->polymorphic_funcs[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        bool found_compatible = false;
        ast_type_var_catalog_t using_catalog;

        if(poly_template->traits & AST_FUNC_POLYMORPHIC && func_args_polymorphable(poly_template, arg_types, type_list_length, &using_catalog)){
            found_compatible = true;
        } else while(++poly_index != ast->polymorphic_funcs_length){
            poly_func = &ast->polymorphic_funcs[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = (strcmp(poly_func->name, ast->polymorphic_funcs[poly_index - 1].name) != 0);
            }
            if(poly_func->is_beginning_of_group == 1) break;

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && func_args_polymorphable(poly_template, arg_types, type_list_length, &using_catalog)){
                found_compatible = true;
            }
        }

        if(found_compatible){
            ir_func_mapping_t instance;
            
            if(instantiate_polymorphic_func(builder, poly_template, arg_types, type_list_length, &using_catalog, &instance)) return ALT_FAILURE;
            ast_type_var_catalog_free(&using_catalog);

            result->ast_func = &builder->object->ast.funcs[instance.ast_func_id];
            result->ir_func = &builder->object->ir_module.funcs[instance.ir_func_id];
            result->ast_func_id = instance.ast_func_id;
            result->ir_func_id = instance.ir_func_id;
            return SUCCESS;
        }
    }

    return FAILURE; // No function with that definition found
}

errorcode_t ir_gen_find_method_conforming(ir_builder_t *builder, const char *struct_name,
        const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result){

    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &builder->object->ir_module;

    maybe_index_t index = find_beginning_of_method_group(ir_module->methods, ir_module->methods_length, struct_name, name);
    if(index == -1) return FAILURE;

    ir_method_t *method = &ir_module->methods[index];
    ast_func_t *ast_func = &builder->object->ast.funcs[method->ast_func_id];

    if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
        result->ast_func = ast_func;
        result->ir_func = &builder->object->ir_module.funcs[method->ir_func_id];
        result->ast_func_id = method->ast_func_id;
        result->ir_func_id = method->ir_func_id;
        return SUCCESS;
    }

    while(++index != ir_module->methods_length){
        method = &ir_module->methods[index];
        ast_func = &builder->object->ast.funcs[method->ast_func_id];

        if(method->is_beginning_of_group == -1){
            method->is_beginning_of_group = (strcmp(method->name, ir_module->methods[index - 1].name) != 0);
        }
        if(method->is_beginning_of_group == 1) return FAILURE;

        if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
            result->ast_func = ast_func;
            result->ir_func = &builder->object->ir_module.funcs[method->ir_func_id];
            result->ast_func_id = method->ast_func_id;
            result->ir_func_id = method->ir_func_id;
            return SUCCESS;
        }
    }

    return FAILURE; // No method with that definition found
}

maybe_index_t find_beginning_of_func_group(ir_func_mapping_t *mappings, length_t length, const char *name){
    // Searches for beginning of function group in a list of mappings
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    maybe_index_t first, middle, last, comparison;
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

maybe_index_t find_beginning_of_method_group(ir_method_t *methods, length_t length,
    const char *struct_name, const char *name){
    // Searches for beginning of method group in a list of methods
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    maybe_index_t first, middle, last, comparison;
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

maybe_index_t find_beginning_of_poly_func_group(ast_polymorphic_func_t *polymorphic_funcs, length_t polymorphic_funcs_length, const char *name){
    // Searches for beginning of function group in a list of polymorphic function mappings
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    maybe_index_t first, middle, last, comparison;
    first = 0; last = polymorphic_funcs_length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(polymorphic_funcs[middle].name, name);

        if(comparison == 0){
            while(true){
                if(polymorphic_funcs[middle].is_beginning_of_group == -1){
                    // It is uncalculated, so we must calculate it
                    bool prev_different_name = middle == 0 ? true : strcmp(polymorphic_funcs[middle - 1].name, name) != 0;
                    polymorphic_funcs[middle].is_beginning_of_group = prev_different_name;
                }
                if(polymorphic_funcs[middle].is_beginning_of_group == 1) return middle;
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

successful_t func_args_match(ast_func_t *func, ast_type_t *type_list, length_t type_list_length){
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

successful_t func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
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

bool func_args_polymorphable(ast_func_t *poly_template, ast_type_t *arg_types, length_t type_length, ast_type_var_catalog_t *out_catalog){
    ast_type_var_catalog_t catalog;
    ast_type_var_catalog_init(&catalog);

    // Ensure argument supplied meet length requirements
    if(poly_template->traits & AST_FUNC_VARARG ? poly_template->arity > type_length : poly_template->arity != type_length){
        if(out_catalog) *out_catalog = catalog;
        else ast_type_var_catalog_free(&catalog);
        return false;
    }

    for(length_t i = 0; i != type_length; i++){
        if(!ast_type_polymorphable(&poly_template->arg_types[i], &arg_types[i], &catalog)){
            if(out_catalog) *out_catalog = catalog;
            else ast_type_var_catalog_free(&catalog);
            return false;
        }
    }

    if(out_catalog) *out_catalog = catalog;
    else ast_type_var_catalog_free(&catalog);
    return true;
}
