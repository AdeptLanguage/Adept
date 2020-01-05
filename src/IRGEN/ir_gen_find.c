
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
        ast_func = &object->ast.funcs[mapping->ast_func_id];

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
        const char *name, bool *out_is_unique, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    maybe_index_t index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->func_mappings_length, name);
    if(index == -1) return FAILURE;

    if(out_is_unique){
        *out_is_unique = true;

        if(index + 1 != ir_module->funcs_length){
            ir_func_mapping_t *mapping = &ir_module->func_mappings[index + 1];
            if(mapping->is_beginning_of_group == -1){
                mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index].name) != 0);
            }
            if(mapping->is_beginning_of_group == 0) *out_is_unique = false;
        }
    }

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

        errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
        if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

        if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
            found_compatible = true;
        } else while(++poly_index != ast->polymorphic_funcs_length){
            poly_func = &ast->polymorphic_funcs[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = (strcmp(poly_func->name, ast->polymorphic_funcs[poly_index - 1].name) != 0);
            }
            if(poly_func->is_beginning_of_group == 1) break;

            res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
                found_compatible = true;
                break;
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

    if(index != -1){
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
                method->is_beginning_of_group = (strcmp(method->name, ir_module->methods[index - 1].name) != 0 || strcmp(method->struct_name, ir_module->methods[index - 1].struct_name) != 0);
            }
            if(method->is_beginning_of_group == 1) break;

            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
                result->ast_func = ast_func;
                result->ir_func = &builder->object->ir_module.funcs[method->ir_func_id];
                result->ast_func_id = method->ast_func_id;
                result->ir_func_id = method->ir_func_id;
                return SUCCESS;
            }
        }
    }

    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_polymorphic_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        bool found_compatible = false;
        ast_type_var_catalog_t using_catalog;

        errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
        if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

        if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
            found_compatible = poly_template->arity != 0 && strcmp(poly_template->arg_names[0], "this") == 0;
        } else while(++poly_index != ast->polymorphic_methods_length){
            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = (strcmp(poly_func->name, ast->polymorphic_methods[poly_index - 1].name) != 0);
            }
            if(poly_func->is_beginning_of_group == 1) break;

            res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res){
                found_compatible = poly_template->arity != 0 && strcmp(poly_template->arg_names[0], "this") == 0;
                if(found_compatible) break;
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

    if(strcmp(name, "__defer__") == 0
    && attempt_autogen___defer__(builder, arg_values, arg_types, type_list_length, result) == SUCCESS){
        // Auto-generate __defer__ method if possible
        return SUCCESS;
    }

    return FAILURE; // No method with that definition found
}

errorcode_t ir_gen_find_generic_base_method_conforming(ir_builder_t *builder, const char *generic_base,
    const char *name,
    ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, funcpair_t *result){
    
    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &builder->object->ir_module;

    maybe_index_t index = find_beginning_of_generic_base_method_group(ir_module->generic_base_methods, ir_module->generic_base_methods_length, generic_base, name);

    if(index != -1){
        ir_generic_base_method_t *method = &ir_module->generic_base_methods[index];
        ast_func_t *ast_func = &builder->object->ast.funcs[method->ast_func_id];

        if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
            result->ast_func = ast_func;
            result->ir_func = &builder->object->ir_module.funcs[method->ir_func_id];
            result->ast_func_id = method->ast_func_id;
            result->ir_func_id = method->ir_func_id;
            return SUCCESS;
        }

        while(++index != ir_module->generic_base_methods_length){
            method = &ir_module->generic_base_methods[index];
            ast_func = &builder->object->ast.funcs[method->ast_func_id];

            if(method->is_beginning_of_group == -1){
                method->is_beginning_of_group = (strcmp(method->name, ir_module->generic_base_methods[index - 1].name) != 0 || strcmp(method->generic_base, ir_module->generic_base_methods[index - 1].generic_base) != 0);
            }
            if(method->is_beginning_of_group == 1) break;

            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length)){
                result->ast_func = ast_func;
                result->ir_func = &builder->object->ir_module.funcs[method->ir_func_id];
                result->ast_func_id = method->ast_func_id;
                result->ir_func_id = method->ir_func_id;
                return SUCCESS;
            }
        }
    }
    
    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_polymorphic_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        bool found_compatible = false;
        ast_type_var_catalog_t using_catalog;

        errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
        if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

        if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
            found_compatible = poly_template->arity != 0 && strcmp(poly_template->arg_names[0], "this") == 0;
        } else while(++poly_index != ast->polymorphic_methods_length){
            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = (strcmp(poly_func->name, ast->polymorphic_methods[poly_index - 1].name) != 0);
            }
            if(poly_func->is_beginning_of_group == 1) break;

            res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
                found_compatible = poly_template->arity != 0 && strcmp(poly_template->arg_names[0], "this") == 0;
                if(found_compatible) break;
            }
        }

        if(found_compatible){
            ir_func_mapping_t instance;

            if(instantiate_polymorphic_func(builder, poly_template, arg_types, type_list_length, &using_catalog, &instance)){
                ast_type_var_catalog_free(&using_catalog);
                return ALT_FAILURE;
            }
            
            ast_type_var_catalog_free(&using_catalog);

            result->ast_func = &builder->object->ast.funcs[instance.ast_func_id];
            result->ir_func = &builder->object->ir_module.funcs[instance.ir_func_id];
            result->ast_func_id = instance.ast_func_id;
            result->ir_func_id = instance.ir_func_id;
            return SUCCESS;
        }
    }

    if(strcmp(name, "__defer__") == 0
    && attempt_autogen___defer__(builder, arg_values, arg_types, type_list_length, result) == SUCCESS){
        // Auto-generate __defer__ method if possible
        return SUCCESS;
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
                        bool prev_different_name = middle == 0 ? true : strcmp(methods[middle - 1].name, name) != 0 || strcmp(methods[middle - 1].struct_name, struct_name) != 0;
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

maybe_index_t find_beginning_of_generic_base_method_group(ir_generic_base_method_t *methods, length_t length,
    const char *generic_base, const char *name){
    // Searches for beginning of method group in a list of methods
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    maybe_index_t first, middle, last, comparison;
    first = 0; last = length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(methods[middle].generic_base, generic_base);

        if(comparison == 0){
            comparison = strcmp(methods[middle].name, name);

            if(comparison == 0){
                while(true){
                    if(methods[middle].is_beginning_of_group == -1){
                        // It is uncalculated, so we must calculate it
                        bool prev_different_name = middle == 0 ? true : strcmp(methods[middle - 1].name, name) != 0 || strcmp(methods[middle - 1].generic_base, generic_base) != 0;
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
        if(!ast_types_conform(builder, &arg_value_list[a], &arg_type_list[a], &arg_types[a], CONFORM_MODE_CALL_ARGUMENTS)){
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

errorcode_t func_args_polymorphable(ir_builder_t *builder, ast_func_t *poly_template, ir_value_t **arg_value_list, ast_type_t *arg_types,
        length_t type_length, ast_type_var_catalog_t *out_catalog){

    // Ensure argument supplied meet length requirements
    if(poly_template->traits & AST_FUNC_VARARG ? poly_template->arity > type_length : poly_template->arity != type_length){
        return FAILURE;
    }
    
    ast_type_var_catalog_t catalog;
    ast_type_var_catalog_init(&catalog);

    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);

    // Store a copy of the unmodifed function argument values
    ir_value_t **arg_value_list_unmodified = malloc(sizeof(ir_value_t*) * type_length);
    memcpy(arg_value_list_unmodified, arg_value_list, sizeof(ir_value_t*) * type_length);

    for(length_t i = 0; i != poly_template->arity; i++){
        errorcode_t res;

        if(ast_type_has_polymorph(&poly_template->arg_types[i]))
            res = arg_type_polymorphable(builder, &poly_template->arg_types[i], &arg_types[i], &catalog);
        else
            res = ast_types_conform(builder, &arg_value_list[i], &arg_types[i], &poly_template->arg_types[i], CONFORM_MODE_CALL_ARGUMENTS) ? SUCCESS : FAILURE;

        if(res != SUCCESS){
            // Restore pool snapshot
            ir_pool_snapshot_restore(builder->pool, &snapshot);

            // Undo any modifications to the function arguments
            memcpy(arg_value_list, arg_value_list_unmodified, sizeof(ir_value_t*) * (i + 1));
            free(arg_value_list_unmodified);
            ast_type_var_catalog_free(&catalog);
            return res;
        }
    }

    if(out_catalog) *out_catalog = catalog;
    else ast_type_var_catalog_free(&catalog);
    free(arg_value_list_unmodified);
    return SUCCESS;
}

errorcode_t arg_type_polymorphable(ir_builder_t *builder, const ast_type_t *polymorphic_type, const ast_type_t *concrete_type, ast_type_var_catalog_t *catalog){
    if(polymorphic_type->elements_length > concrete_type->elements_length) return FAILURE;

    for(length_t i = 0; i != concrete_type->elements_length; i++){
        length_t poly_elem_id = polymorphic_type->elements[i]->id;

        // Convert polymorphs with prerequisites to plain old polymorphic variables
        if(poly_elem_id == AST_ELEM_POLYMORPH_PREREQ){
            ast_elem_polymorph_prereq_t *prereq = (ast_elem_polymorph_prereq_t*) polymorphic_type->elements[i];

            ast_struct_t *similar = ast_struct_find(&builder->object->ast, prereq->similarity_prerequisite);

            if(similar == NULL){
                compiler_panicf(builder->compiler, prereq->source, "Undeclared struct '%s'", similar);
                return ALT_FAILURE;
            }

            if(i + 1 != concrete_type->elements_length) return FAILURE;

            if(concrete_type->elements[i]->id == AST_ELEM_BASE){
                char *given_name = ((ast_elem_base_t*) concrete_type->elements[i])->base;
                ast_struct_t *given = ast_struct_find(&builder->object->ast, given_name);

                if(given == NULL){
                    redprintf("INTERNAL ERROR: arg_type_polymorphable failed to find struct '%s' which should exist\n", given_name);
                    return FAILURE;
                }

                // Ensure polymorphic prerequisite met
                length_t dummy_field_index;
                for(length_t f = 0; f != similar->field_count; f++){
                    if(!ast_struct_find_field(given, similar->field_names[f], &dummy_field_index)) return FAILURE;
                }

                // All fields of struct 'similar' exist in struct 'given'
                // therefore 'given' is similar to 'similar' and the prerequisite is met
            } else if(concrete_type->elements[i]->id != AST_ELEM_GENERIC_BASE){
                if(((ast_elem_generic_base_t*) concrete_type->elements[i])->name_is_polymorphic){
                    redprintf("INTERNAL ERROR: arg_type_polymorphable encountered polymorphic generic struct name in middle of AST type\n");
                    return ALT_FAILURE;
                }

                char *given_name = ((ast_elem_generic_base_t*) concrete_type->elements[i])->name;
                ast_polymorphic_struct_t *given = ast_polymorphic_struct_find(&builder->object->ast, given_name);

                if(given == NULL){
                    redprintf("INTERNAL ERROR: arg_type_polymorphable failed to find polymophic struct '%s' which should exist\n", given_name);
                    return FAILURE;
                }

                // Ensure polymorphic prerequisite met
                length_t dummy_field_index;
                for(length_t f = 0; f != similar->field_count; f++){
                    // NOTE: Assuming 'ast_polymorphic_struct_t' overlaps with 'ast_struct_t'
                    if(!ast_struct_find_field((ast_struct_t*) given, similar->field_names[f], &dummy_field_index)) return FAILURE;
                }

                // All fields of struct 'similar' exist in struct 'given'
                // therefore 'given' is similar to 'similar' and the prerequisite is met
            } else {
                return FAILURE;
            }
            
            // Ensure there aren't other elements following the polymorphic element
            if(i + 1 != polymorphic_type->elements_length){
                redprintf("INTERNAL ERROR: arg_type_polymorphable encountered polymorphic element in middle of AST type\n");
                return ALT_FAILURE;
            }

            // DANGEROUS: Manually managed AST type with semi-ownership
            // DANGEROUS: Potentially bad memory tricks
            ast_type_t replacement;
            replacement.elements_length = concrete_type->elements_length - i;
            replacement.elements = malloc(sizeof(ast_elem_t*) * replacement.elements_length);
            memcpy(replacement.elements, &concrete_type->elements[i], sizeof(ast_elem_t*) * replacement.elements_length);
            replacement.source = replacement.elements[0]->source;

            // Ensure consistency with catalog
            ast_elem_polymorph_t *polymorphic_elem = (ast_elem_polymorph_t*) polymorphic_type->elements[i];
            ast_type_var_t *type_var = ast_type_var_catalog_find(catalog, polymorphic_elem->name);

            if(type_var){
                if(!ast_types_identical(&replacement, &type_var->binding)){
                    // Given arguments don't meet consistency requirements of type variables
                    free(replacement.elements);
                    return FAILURE;
                }
            } else {
                // Add to catalog since it's not already present
                ast_type_var_catalog_add(catalog, polymorphic_elem->name, &replacement);
            }

            i = concrete_type->elements_length - 1;
            free(replacement.elements);
            continue;
        }

        // Check whether element is a polymorphic element
        if(poly_elem_id == AST_ELEM_POLYMORPH){
            // Ensure there aren't other elements following the polymorphic element
            if(i + 1 != polymorphic_type->elements_length){
                redprintf("INTERNAL ERROR: arg_type_polymorphable encountered polymorphic element in middle of AST type\n");
                return ALT_FAILURE;
            }

            // DANGEROUS: Manually managed AST type with semi-ownership
            // DANGEROUS: Potentially bad memory tricks
            ast_type_t replacement;
            replacement.elements_length = concrete_type->elements_length - i;
            replacement.elements = malloc(sizeof(ast_elem_t*) * replacement.elements_length);
            memcpy(replacement.elements, &concrete_type->elements[i], sizeof(ast_elem_t*) * replacement.elements_length);
            replacement.source = replacement.elements[0]->source;

            // Ensure consistency with catalog
            ast_elem_polymorph_t *polymorphic_elem = (ast_elem_polymorph_t*) polymorphic_type->elements[i];
            ast_type_var_t *type_var = ast_type_var_catalog_find(catalog, polymorphic_elem->name);

            if(type_var){
                if(!ast_types_identical(&replacement, &type_var->binding)){
                    // Given arguments don't meet consistency requirements of type variables
                    free(replacement.elements);
                    return FAILURE;
                }
            } else {
                // Add to catalog since it's not already present
                ast_type_var_catalog_add(catalog, polymorphic_elem->name, &replacement);
            }

            i = concrete_type->elements_length - 1;
            free(replacement.elements);
            continue;
        }

        // If the polymorphic type doesn't have a polymorphic element for the current element,
        // then the type element ids must match
        if(poly_elem_id != concrete_type->elements[i]->id) return FAILURE;

        // To determine which of (SUCCESS, FAILURE, or ALT_FAILURE)
        errorcode_t res;
        
        // Ensure the two non-polymorphic elements match
        switch(poly_elem_id){
        case AST_ELEM_BASE:
            if(strcmp(((ast_elem_base_t*) polymorphic_type->elements[i])->base, ((ast_elem_base_t*) concrete_type->elements[i])->base) != 0)
                return FAILURE;
            break;
        case AST_ELEM_POINTER:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
            // Always considered matching
            break;
        case AST_ELEM_FIXED_ARRAY:
            if(((ast_elem_fixed_array_t*) polymorphic_type->elements[i])->length != ((ast_elem_fixed_array_t*) concrete_type->elements[i])->length) return FAILURE;
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem_a = (ast_elem_func_t*) polymorphic_type->elements[i];
                ast_elem_func_t *func_elem_b = (ast_elem_func_t*) concrete_type->elements[i];
                if((func_elem_a->traits & AST_FUNC_VARARG) != (func_elem_b->traits & AST_FUNC_VARARG)) return FAILURE;
                if((func_elem_a->traits & AST_FUNC_STDCALL) != (func_elem_b->traits & AST_FUNC_STDCALL)) return FAILURE;
                if(func_elem_a->arity != func_elem_b->arity) return FAILURE;

                res = arg_type_polymorphable(builder, func_elem_a->return_type, func_elem_b->return_type, catalog);
                if(res != SUCCESS) return res;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    res = arg_type_polymorphable(builder, &func_elem_a->arg_types[a], &func_elem_b->arg_types[a], catalog);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_a = (ast_elem_generic_base_t*) polymorphic_type->elements[i];
                ast_elem_generic_base_t *generic_base_b = (ast_elem_generic_base_t*) concrete_type->elements[i];

                if(generic_base_a->name_is_polymorphic || generic_base_b->name_is_polymorphic){
                    redprintf("INTERNAL ERROR: polymorphic names for generic structs not implemented in arg_type_polymorphable\n");
                    return FAILURE;
                }

                if(generic_base_a->generics_length != generic_base_b->generics_length) return FAILURE;
                if(strcmp(generic_base_a->name, generic_base_b->name) != 0) return FAILURE;

                for(length_t i = 0; i != generic_base_a->generics_length; i++){
                    res = arg_type_polymorphable(builder, &generic_base_a->generics[i], &generic_base_b->generics[i], catalog);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_POLYMORPH:
        case AST_ELEM_POLYMORPH_PREREQ:
            redprintf("INTERNAL ERROR: arg_type_polymorphable encountered uncaught polymorphic type element\n");
            return ALT_FAILURE;
        default:
            redprintf("INTERNAL ERROR: arg_type_polymorphable encountered unknown element id 0x%8X\n", polymorphic_type->elements[i]->id);
            return ALT_FAILURE;
        }
    }

    return SUCCESS;
}
