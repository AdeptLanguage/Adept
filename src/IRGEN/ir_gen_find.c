
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGE/funcpair.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_pool.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/trait.h"

errorcode_t ir_gen_find_func(compiler_t *compiler, object_t *object, const char *name,
        ast_type_t *arg_types, length_t arg_types_length, trait_t mask, trait_t req_traits, optional_funcpair_t *result){
    
    ir_module_t *ir_module = &object->ir_module;
    maybe_index_t index = find_beginning_of_func_group(&ir_module->func_mappings, name);

    if(index != -1){
        ir_func_mapping_t *mapping = &ir_module->func_mappings.mappings[index];
        ast_func_t *ast_func = &object->ast.funcs[mapping->ast_func_id];
    
        while(true){
            if((ast_func->traits & mask) == req_traits && func_args_match(ast_func, arg_types, arg_types_length)){
                optional_funcpair_set(result, true, mapping->ast_func_id, mapping->ir_func_id, object);
                return SUCCESS;
            }
    
            if((length_t) ++index >= ir_module->func_mappings.length) break;
    
            mapping = &ir_module->func_mappings.mappings[index];
            ast_func = &object->ast.funcs[mapping->ast_func_id];
    
            if(mapping->is_beginning_of_group == -1){
                mapping->is_beginning_of_group = !streq(mapping->name, ir_module->func_mappings.mappings[index - 1].name);
            }
    
            if(mapping->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__pass__")){
        return attempt_autogen___pass__(compiler, object, arg_types, arg_types_length, result);
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(compiler, object, arg_types, arg_types_length, result);
    }

    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(compiler, object, arg_types, arg_types_length, result);
    }

    return FAILURE; // No function with that definition found
}

errorcode_t ir_gen_find_func_named(object_t *object, const char *name, bool *out_is_unique, funcpair_t *result){
    ir_module_t *ir_module = &object->ir_module;

    maybe_index_t index = find_beginning_of_func_group(&ir_module->func_mappings, name);
    if(index == -1) return FAILURE;

    // TODO: CLEANUP: Cleanup this code

    if(out_is_unique){
        *out_is_unique = true;

        if((length_t) index + 1 != ir_module->func_mappings.length){
            ir_func_mapping_t *mapping = &ir_module->func_mappings.mappings[index + 1];
            if(mapping->is_beginning_of_group == -1){
                mapping->is_beginning_of_group = index == 0 ? 1 : !streq(mapping->name, ir_module->func_mappings.mappings[index].name);
            }
            if(mapping->is_beginning_of_group == 0) *out_is_unique = false;
        }
    }

    ir_func_mapping_t *mapping = &ir_module->func_mappings.mappings[index];
    result->ast_func = &object->ast.funcs[mapping->ast_func_id];
    result->ir_func = &object->ir_module.funcs.funcs[mapping->ir_func_id];
    result->ast_func_id = mapping->ast_func_id;
    result->ir_func_id = mapping->ir_func_id;
    return SUCCESS;
}

errorcode_t ir_gen_find_func_conforming(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, ast_type_t *gives, bool no_user_casts, source_t from_source, optional_funcpair_t *result){
    
    // Do strict argument type conforming rules first
    errorcode_t strict_errorcode = ir_gen_find_func_conforming_to(builder, name, arg_values, arg_types, type_list_length, gives, from_source, result, CONFORM_MODE_CALL_ARGUMENTS);
    if(strict_errorcode == SUCCESS || strict_errorcode == ALT_FAILURE) return strict_errorcode;

    // If no strict match was found, try a looser match
    unsigned int loose_mode = no_user_casts ? CONFORM_MODE_CALL_ARGUMENTS_LOOSE_NOUSER : CONFORM_MODE_CALL_ARGUMENTS_LOOSE; 
    return ir_gen_find_func_conforming_to(builder, name, arg_values, arg_types, type_list_length, gives, from_source, result, loose_mode);
}

errorcode_t ir_gen_find_func_conforming_to(ir_builder_t *builder, const char *name, ir_value_t **arg_values,
        ast_type_t *arg_types, length_t type_list_length, ast_type_t *gives, source_t from_source, optional_funcpair_t *result, trait_t conform_mode){
    
    ir_module_t *ir_module = &builder->object->ir_module;
    maybe_index_t index = find_beginning_of_func_group(&ir_module->func_mappings, name);

    // Allow for '.elements_length' to be zero to indicate no return matching
    if(gives && gives->elements_length == 0) gives = NULL;

    if(index != -1){
        // DANGEROUS: We must not store a pointer to 'ir_module->func_mappings', since the mappings may be relocated/adjusted
        // by 'funcs_arg_conform'
        // By copying it onto the stack, we avoid having to constantly refresh the pointer to the mapping (or forgetting to do so)
        ir_func_mapping_t mapping = ir_module->func_mappings.mappings[index];
        ast_func_t *ast_func = &builder->object->ast.funcs[mapping.ast_func_id];

        while(true){
            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length, gives, conform_mode)){
                optional_funcpair_set(result, true, mapping.ast_func_id, mapping.ir_func_id, builder->object);
                return SUCCESS;
            }

            if((length_t) ++index >= ir_module->func_mappings.length) break;

            mapping = ir_module->func_mappings.mappings[index];
            ast_func = &builder->object->ast.funcs[mapping.ast_func_id];

            if(mapping.is_beginning_of_group == -1){
                mapping.is_beginning_of_group = !streq(mapping.name, ir_module->func_mappings.mappings[index - 1].name);
            }

            if(mapping.is_beginning_of_group == 1) break;
        }
    }

    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->poly_funcs, ast->poly_funcs_length, name);

    if(poly_index != -1){
        ast_poly_func_t *poly_func = &ast->poly_funcs[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        ast_poly_catalog_t using_catalog;

        while(true){
            errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog, gives, conform_mode);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS){
                ir_func_mapping_t instance;
                
                if(instantiate_poly_func(builder->compiler, builder->object, from_source, poly_func->ast_func_id, arg_types, type_list_length, &using_catalog, &instance)){
                    ast_poly_catalog_free(&using_catalog);
                    return ALT_FAILURE;
                }

                ast_poly_catalog_free(&using_catalog);
                optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, builder->object);
                return SUCCESS;
            }

            if((length_t) ++poly_index >= ast->poly_funcs_length) break;

            poly_func = &ast->poly_funcs[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = !streq(poly_func->name, ast->poly_funcs[poly_index - 1].name);
            }

            if(poly_func->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__pass__")){
        return attempt_autogen___pass__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    return FAILURE; // No function with that definition found
}

errorcode_t ir_gen_find_pass_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, optional_funcpair_t *result){
    // Finds the correct __pass__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&builder->object->ir_module.sf_cache, arg_type);

    if(cache_entry->has_pass == TROOLEAN_TRUE){
        funcpair_t *out_result = &result->value;
        out_result->ir_func_id = cache_entry->pass_ir_func_id;
        out_result->ast_func_id = cache_entry->pass_ast_func_id;
        out_result->ir_func = &builder->object->ir_module.funcs.funcs[out_result->ir_func_id];
        out_result->ast_func = &builder->object->ast.funcs[out_result->ast_func_id];
        result->has = true;
        return SUCCESS;
    } else if(cache_entry->has_pass == TROOLEAN_FALSE){
        result->has = false;
        return SUCCESS;
    }

    // Whether we have a __pass__ function is unknown, so lets try to see if we have one
    return ir_gen_find_func_conforming(builder, "__pass__", argument, arg_type, 1, NULL, true, NULL_SOURCE, result);
}

errorcode_t ir_gen_find_defer_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_funcpair_t *result){
    // Finds the correct __defer__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&object->ir_module.sf_cache, arg_type);

    // If result is cached, use the cached version
    if(cache_entry->has_defer == TROOLEAN_TRUE){
        optional_funcpair_set(result, true, cache_entry->defer_ast_func_id, cache_entry->defer_ir_func_id, object);
        return SUCCESS;
    } else if(cache_entry->has_defer == TROOLEAN_FALSE){
        return FAILURE;
    }

    // Create temporary AST pointer type without allocating on the heap
    // Will be used as AST type for subject of method during lookup
    ast_type_t ast_type_ptr;
    ast_elem_t *ast_type_ptr_elems[2];
    ast_elem_t ast_type_ptr_elem;
    ast_type_ptr_elem.id = AST_ELEM_POINTER;
    ast_type_ptr_elem.source = arg_type->source;
    ast_type_ptr_elems[0] = &ast_type_ptr_elem;
    ast_type_ptr_elems[1] = arg_type->elements[0];
    ast_type_ptr.elements = ast_type_ptr_elems;
    ast_type_ptr.elements_length = 2;
    ast_type_ptr.source = arg_type->source;

    // Try to find '__defer__' method
    errorcode_t errorcode;
    weak_cstr_t struct_name;

    switch(arg_type->elements[0]->id){
    case AST_ELEM_BASE:
        struct_name = ((ast_elem_base_t*) arg_type->elements[0])->base;
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__defer__", &ast_type_ptr, 1, NULL_SOURCE, result);
        break;
    case AST_ELEM_GENERIC_BASE:
        struct_name = ((ast_elem_generic_base_t*) arg_type->elements[0])->name;
        errorcode = ir_gen_find_poly_method(compiler, object, struct_name, "__defer__", &ast_type_ptr, 1, NULL_SOURCE, result);
        break;
    default:
        errorcode = FAILURE;
    }

    // Cache result
    if(errorcode == SUCCESS && result->has){
        cache_entry->defer_ast_func_id = result->value.ast_func_id;
        cache_entry->defer_ir_func_id = result->value.ir_func_id;
        cache_entry->has_defer = TROOLEAN_TRUE;
    } else {
        cache_entry->has_defer = TROOLEAN_FALSE;
    }

    return errorcode;
}

errorcode_t ir_gen_find_assign_func(compiler_t *compiler, object_t *object, ast_type_t *arg_type, optional_funcpair_t *result){
    // Finds the correct __assign__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&object->ir_module.sf_cache, arg_type);

    // If result is cached, use the cached version
    if(cache_entry->has_assign == TROOLEAN_TRUE){
        optional_funcpair_set(result, true, cache_entry->assign_ast_func_id, cache_entry->assign_ir_func_id, object);
        return SUCCESS;
    } else if(cache_entry->has_assign == TROOLEAN_FALSE){
        return FAILURE;
    }

    // Create temporary AST pointer type without allocating on the heap
    // Will be used as AST type for subject of method during lookup
    ast_type_t ast_type_ptr;
    ast_elem_t *ast_type_ptr_elems[2];
    ast_elem_t ast_type_ptr_elem;
    ast_type_ptr_elem.id = AST_ELEM_POINTER;
    ast_type_ptr_elem.source = arg_type->source;
    ast_type_ptr_elems[0] = &ast_type_ptr_elem;
    ast_type_ptr_elems[1] = arg_type->elements[0];
    ast_type_ptr.elements = ast_type_ptr_elems;
    ast_type_ptr.elements_length = 2;
    ast_type_ptr.source = arg_type->source;

    ast_type_t args[2];
    args[0] = ast_type_ptr;
    args[1] = *arg_type;

    // Try to find '__assign__' method
    errorcode_t errorcode;
    weak_cstr_t struct_name;

    switch(arg_type->elements[0]->id){
    case AST_ELEM_BASE:
        struct_name = ((ast_elem_base_t*) arg_type->elements[0])->base;
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__assign__", args, 2, NULL_SOURCE, result);
        break;
    case AST_ELEM_GENERIC_BASE:
        struct_name = ((ast_elem_generic_base_t*) arg_type->elements[0])->name;
        errorcode = ir_gen_find_poly_method(compiler, object, struct_name, "__assign__", args, 2, NULL_SOURCE, result);
        break;
    default:
        errorcode = FAILURE;
    }

    // Cache result
    if(errorcode == SUCCESS && result->has){
        cache_entry->assign_ast_func_id = result->value.ast_func_id;
        cache_entry->assign_ir_func_id = result->value.ir_func_id;
        cache_entry->has_assign = TROOLEAN_TRUE;
    } else {
        cache_entry->has_assign = TROOLEAN_FALSE;
    }

    return errorcode;
}

errorcode_t ir_gen_find_method_conforming(ir_builder_t *builder, const char *struct_name,
        const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
        length_t type_list_length, ast_type_t *gives, source_t from_source, optional_funcpair_t *result){
    
    // Do strict argument type conforming rules first
    errorcode_t strict_errorcode = ir_gen_find_method_conforming_to(builder, struct_name, name, arg_values, arg_types, type_list_length, gives, from_source, result, CONFORM_MODE_CALL_ARGUMENTS);
    if(strict_errorcode == SUCCESS || strict_errorcode == ALT_FAILURE) return strict_errorcode;

    // If no strict match was found, try a looser match
    return ir_gen_find_method_conforming_to(builder, struct_name, name, arg_values, arg_types, type_list_length, gives, from_source, result, CONFORM_MODE_CALL_ARGUMENTS_LOOSE);
}

errorcode_t ir_gen_find_method_conforming_to(ir_builder_t *builder, const char *struct_name,
        const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
        length_t type_list_length, ast_type_t *gives, source_t from_source, optional_funcpair_t *result, trait_t conform_mode){

    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &builder->object->ir_module;

    // Allow for '.elements_length' to be zero to indicate no return matching
    if(gives && gives->elements_length == 0) gives = NULL;

    maybe_index_t index = find_beginning_of_method_group(&ir_module->methods, struct_name, name);

    if(index != -1){
        ir_method_t *method = &ir_module->methods.methods[index];
        ast_func_t *ast_func = &builder->object->ast.funcs[method->ast_func_id];

        while(true){
            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length, gives, conform_mode)){
                optional_funcpair_set(result, true, method->ast_func_id, method->ir_func_id, builder->object);
                return SUCCESS;
            }

            if((length_t) ++index >= ir_module->methods.length) break;

            method = &ir_module->methods.methods[index];
            ast_func = &builder->object->ast.funcs[method->ast_func_id];

            if(method->is_beginning_of_group == -1){
                method->is_beginning_of_group = !streq(method->name, ir_module->methods.methods[index - 1].name) || !streq(method->struct_name, ir_module->methods.methods[index - 1].struct_name);
            }

            if(method->is_beginning_of_group == 1) break;
        }
    }

    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_poly_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        ast_poly_catalog_t using_catalog;

        while(true){
            errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog, gives, conform_mode);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS && poly_template->arity != 0 && streq(poly_template->arg_names[0], "this")){
                ir_func_mapping_t instance;
    
                if(instantiate_poly_func(builder->compiler, builder->object, from_source, poly_func->ast_func_id, arg_types, type_list_length, &using_catalog, &instance)){
                    ast_poly_catalog_free(&using_catalog);
                    return ALT_FAILURE;
                }

                ast_poly_catalog_free(&using_catalog);
    
                optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, builder->object);
                return SUCCESS;
            }

            if((length_t) ++poly_index >= ast->polymorphic_methods_length) break;

            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = !streq(poly_func->name, ast->polymorphic_methods[poly_index - 1].name);
            }

            if(poly_func->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }
    
    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    return FAILURE; // No method with that definition found
}

errorcode_t ir_gen_find_poly_method_conforming(ir_builder_t *builder, const char *struct_name,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, ast_type_t *gives, source_t from_source, optional_funcpair_t *result){
    
    // Do strict argument type conforming rules first
    errorcode_t strict_errorcode = ir_gen_find_poly_method_conforming_to(builder, struct_name, name, arg_values, arg_types, type_list_length, gives, from_source, result, CONFORM_MODE_CALL_ARGUMENTS);
    if(strict_errorcode == SUCCESS || strict_errorcode == ALT_FAILURE) return strict_errorcode;

    // If no strict match was found, try a looser match
    return ir_gen_find_poly_method_conforming_to(builder, struct_name, name, arg_values, arg_types, type_list_length, gives, from_source, result, CONFORM_MODE_CALL_ARGUMENTS_LOOSE);
}

errorcode_t ir_gen_find_poly_method_conforming_to(ir_builder_t *builder, const char *struct_name,
    const char *name, ir_value_t **arg_values, ast_type_t *arg_types,
    length_t type_list_length, ast_type_t *gives, source_t from_source, optional_funcpair_t *result, trait_t conform_mode){
    
    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &builder->object->ir_module;

    // Allow for '.elements_length' to be zero to indicate no return matching
    if(gives && gives->elements_length == 0) gives = NULL;

    // TODO: CLEANUP: Cleanup this code

    maybe_index_t index = find_beginning_of_poly_method_group(&ir_module->poly_methods, struct_name, name);

    if(index != -1){
        ir_poly_method_t *method = &ir_module->poly_methods.methods[index];
        ast_func_t *ast_func = &builder->object->ast.funcs[method->ast_func_id];

        while(true){
            if(func_args_conform(builder, ast_func, arg_values, arg_types, type_list_length, gives, conform_mode)){
                optional_funcpair_set(result, true, method->ast_func_id, method->ir_func_id, builder->object);
                return SUCCESS;
            }

            if((length_t) ++index >= ir_module->poly_methods.length) break;

            method = &ir_module->poly_methods.methods[index];
            ast_func = &builder->object->ast.funcs[method->ast_func_id];

            if(method->is_beginning_of_group == -1){
                method->is_beginning_of_group = index == 0 ? 1 : (!streq(method->name, ir_module->poly_methods.methods[index - 1].name) || !streq(method->struct_name, ir_module->poly_methods.methods[index - 1].struct_name));
            }

            if(method->is_beginning_of_group == 1) break;
        }
    }
    
    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &builder->object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_poly_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        ast_poly_catalog_t using_catalog;

        while(true){
            errorcode_t res = func_args_polymorphable(builder, poly_template, arg_values, arg_types, type_list_length, &using_catalog, gives, conform_mode);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS && poly_template->arity != 0 && streq(poly_template->arg_names[0], "this")){
                ir_func_mapping_t instance;
    
                if(instantiate_poly_func(builder->compiler, builder->object, from_source, poly_func->ast_func_id, arg_types, type_list_length, &using_catalog, &instance)){
                    ast_poly_catalog_free(&using_catalog);
                    return ALT_FAILURE;
                }
                
                ast_poly_catalog_free(&using_catalog);
    
                optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, builder->object);
                return SUCCESS;
            }
            
            if((length_t) ++poly_index >= ast->polymorphic_methods_length) break;

            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = poly_index == 0 ? 1 : !streq(poly_func->name, ast->polymorphic_methods[poly_index - 1].name);
            }

            if(poly_func->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(builder->compiler, builder->object, arg_types, type_list_length, result);
    }

    return FAILURE; // No method with that definition found
}

errorcode_t ir_gen_find_method(compiler_t *compiler, object_t *object, const char *struct_name, 
        const char *name, ast_type_t *arg_types, length_t type_list_length,
        source_t from_source, optional_funcpair_t *result){

    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &object->ir_module;

    maybe_index_t index = find_beginning_of_method_group(&ir_module->methods, struct_name, name);

    if(index != -1){
        ir_method_t *method = &ir_module->methods.methods[index];
        ast_func_t *ast_func = &object->ast.funcs[method->ast_func_id];

        while(true){
            if(func_args_match(ast_func, arg_types, type_list_length)){
                funcpair_t *out_result = &result->value;
                out_result->ast_func = ast_func;
                out_result->ir_func = &object->ir_module.funcs.funcs[method->ir_func_id];
                out_result->ast_func_id = method->ast_func_id;
                out_result->ir_func_id = method->ir_func_id;
                result->has = true;
                return SUCCESS;
            }

            if(((length_t) ++index >= ir_module->methods.length)) break;

            method = &ir_module->methods.methods[index];
            ast_func = &object->ast.funcs[method->ast_func_id];

            if(method->is_beginning_of_group == -1){
                method->is_beginning_of_group = index == 0 ? 1 : (!streq(method->name, ir_module->methods.methods[index - 1].name) || !streq(method->struct_name, ir_module->methods.methods[index - 1].struct_name));
            }

            if(method->is_beginning_of_group == 1) break;
        }
    }

    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_poly_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        ast_poly_catalog_t using_catalog;

        while(true){
            errorcode_t res = func_args_polymorphable_no_conform(compiler, object, poly_template, arg_types, type_list_length, &using_catalog);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS && poly_template->arity != 0 && streq(poly_template->arg_names[0], "this")){
                ir_func_mapping_t instance;

                if(instantiate_poly_func(compiler, object, from_source, poly_func->ast_func_id, arg_types, type_list_length, &using_catalog, &instance)) return ALT_FAILURE;
                ast_poly_catalog_free(&using_catalog);

                optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, object); 
                return SUCCESS;
            }

            if((length_t) ++poly_index >= ast->polymorphic_methods_length) break;

            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = !streq(poly_func->name, ast->polymorphic_methods[poly_index - 1].name);
            }

            if(poly_func->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(compiler, object, arg_types, type_list_length, result);
    }

    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(compiler, object, arg_types, type_list_length, result);
    }

    return FAILURE; // No method with that definition found
}

errorcode_t ir_gen_find_poly_method(compiler_t *compiler, object_t *object, const char *struct_name,
    const char *name, ast_type_t *arg_types,
    length_t type_list_length, source_t from_source, optional_funcpair_t *result){
    
    // NOTE: arg_values, arg_types, etc. must contain an additional beginning
    //           argument for the object being called on
    ir_module_t *ir_module = &object->ir_module;

    // TODO: CLEANUP: Cleanup this messy code

    maybe_index_t index = find_beginning_of_poly_method_group(&ir_module->poly_methods, struct_name, name);

    if(index != -1){
        ir_poly_method_t *method = &ir_module->poly_methods.methods[index];
        ast_func_t *ast_func = &object->ast.funcs[method->ast_func_id];

        while(true){
            if(func_args_match(ast_func, arg_types, type_list_length)){
                optional_funcpair_set(result, true, method->ast_func_id, method->ir_func_id, object);
                return SUCCESS;
            }

            if((length_t) ++index >= ir_module->poly_methods.length) break;
            
            method = &ir_module->poly_methods.methods[index];
            ast_func = &object->ast.funcs[method->ast_func_id];

            if(method->is_beginning_of_group == -1){
                method->is_beginning_of_group = !streq(method->name, ir_module->poly_methods.methods[index - 1].name) || !streq(method->struct_name, ir_module->poly_methods.methods[index - 1].struct_name);
            }

            if(method->is_beginning_of_group == 1) break;
        }
    }
    
    // Attempt to create a conforming function from available polymorphic functions
    ast_t *ast = &object->ast;
    maybe_index_t poly_index = find_beginning_of_poly_func_group(ast->polymorphic_methods, ast->polymorphic_methods_length, name);

    if(poly_index != -1){
        ast_poly_func_t *poly_func = &ast->polymorphic_methods[poly_index];
        ast_func_t *poly_template = &ast->funcs[poly_func->ast_func_id];
        ast_poly_catalog_t using_catalog;

        while(true){
            errorcode_t res = func_args_polymorphable_no_conform(compiler, object, poly_template, arg_types, type_list_length, &using_catalog);
            if(res == ALT_FAILURE) return ALT_FAILURE; // An error occurred

            if(poly_template->traits & AST_FUNC_POLYMORPHIC && res == SUCCESS && poly_template->arity != 0 && streq(poly_template->arg_names[0], "this")){
                ir_func_mapping_t instance;
    
                if(instantiate_poly_func(compiler, object, from_source, poly_func->ast_func_id, arg_types, type_list_length, &using_catalog, &instance)){
                    ast_poly_catalog_free(&using_catalog);
                    return ALT_FAILURE;
                }

                ast_poly_catalog_free(&using_catalog);
                optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, object);
                return SUCCESS;
            }
            
            if((length_t) ++poly_index >= ast->polymorphic_methods_length) break;

            poly_func = &ast->polymorphic_methods[poly_index];
            poly_template = &ast->funcs[poly_func->ast_func_id];

            if(poly_func->is_beginning_of_group == -1){
                poly_func->is_beginning_of_group = !streq(poly_func->name, ast->polymorphic_methods[poly_index - 1].name);
            }

            if(poly_func->is_beginning_of_group == 1) break;
        }
    }

    if(streq(name, "__defer__")){
        return attempt_autogen___defer__(compiler, object, arg_types, type_list_length, result);
    }

    if(streq(name, "__assign__")){
        return attempt_autogen___assign__(compiler, object, arg_types, type_list_length, result);
    }

    return FAILURE; // No method with that definition found
}

maybe_index_t find_beginning_of_func_group(ir_func_mappings_t *func_mappings, const char *name){
    // Searches for beginning of function group in a list of mappings
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    ir_func_mapping_t *mappings = func_mappings->mappings;
    maybe_index_t first, middle, last, comparison;
    first = 0; last = func_mappings->length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            while(true){
                if(mappings[middle].is_beginning_of_group == -1){
                    // It is uncalculated, so we must calculate it
                    bool prev_different_name = middle == 0 ? true : !streq(mappings[middle - 1].name, name);
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

maybe_index_t find_beginning_of_method_group(ir_methods_t *ir_methods, const char *struct_name, const char *name){
    // Searches for beginning of method group in a list of methods
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    // TODO: CLEANUP: Cleanup this code

    ir_method_t *methods = ir_methods->methods;
    maybe_index_t first, middle, last, comparison;
    first = 0; last = ir_methods->length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(methods[middle].struct_name, struct_name);

        if(comparison == 0){
            comparison = strcmp(methods[middle].name, name);

            if(comparison == 0){
                while(true){
                    if(methods[middle].is_beginning_of_group == -1){
                        // It is uncalculated, so we must calculate it
                        bool prev_different_name = middle == 0 ? true : !streq(methods[middle - 1].name, name) || !streq(methods[middle - 1].struct_name, struct_name);
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

maybe_index_t find_beginning_of_poly_method_group(ir_poly_methods_t *ir_methods, const char *generic_base, const char *name){
    // Searches for beginning of method group in a list of methods
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    // TODO: CLEANUP: Cleanup this code

    ir_poly_method_t *methods = ir_methods->methods;
    maybe_index_t first, middle, last, comparison;
    first = 0; last = ir_methods->length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(methods[middle].struct_name, generic_base);

        if(comparison == 0){
            comparison = strcmp(methods[middle].name, name);

            if(comparison == 0){
                while(true){
                    if(methods[middle].is_beginning_of_group == -1){
                        // It is uncalculated, so we must calculate it
                        bool prev_different_name = middle == 0 ? true : !streq(methods[middle - 1].name, name) || !streq(methods[middle - 1].struct_name, generic_base);
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

maybe_index_t find_beginning_of_poly_func_group(ast_poly_func_t *poly_funcs, length_t poly_funcs_length, const char *name){
    // Searches for beginning of function group in a list of polymorphic function mappings
    // If not found returns -1 else returns mapping index
    // (Where a function group is a group of all the functions with the same name)

    maybe_index_t first, middle, last, comparison;
    first = 0; last = poly_funcs_length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(poly_funcs[middle].name, name);

        if(comparison == 0){
            while(true){
                if(poly_funcs[middle].is_beginning_of_group == -1){
                    // It is uncalculated, so we must calculate it
                    bool prev_different_name = middle == 0 ? true : !streq(poly_funcs[middle - 1].name, name);
                    poly_funcs[middle].is_beginning_of_group = prev_different_name;
                }
                if(poly_funcs[middle].is_beginning_of_group == 1) return middle;
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
    length_t arity = func->arity;

    if(func->traits & AST_FUNC_VARARG){
        if(func->arity > type_list_length) return false;
    } else {
        if(arity != type_list_length) return false;
    }

    for(length_t a = 0; a != arity; a++){
        if(!ast_types_identical(&arg_types[a], &type_list[a])) return false;
    }

    return true;
}

successful_t func_args_conform(ir_builder_t *builder, ast_func_t *func, ir_value_t **arg_value_list,
            ast_type_t *arg_type_list, length_t type_list_length, ast_type_t *gives, trait_t conform_mode){

    ast_type_t *arg_types = func->arg_types;
    length_t required_arity = func->arity;

    // In no world is more arguments than allowed for non-vararg/non-variadic functions valid Adept code
    if(
        required_arity < type_list_length &&
        !(func->traits & AST_FUNC_VARARG) &&
        (conform_mode & CONFORM_MODE_VARIADIC ? !(func->traits & AST_FUNC_VARIADIC) : true)
    ) return false;
    
    // Ensure return type matches if provided
    if(gives && !ast_types_identical(gives, &func->return_type)) return false;

    // Determine whether we are missing arguments
    bool requires_use_of_defaults = required_arity > type_list_length;

    if(requires_use_of_defaults){
        // Check to make sure that we have the necessary default values available to later
        // fill in the missing arguments
        
        ast_expr_t **arg_defaults = func->arg_defaults;

        // No default arguments are available to use to attempt to meet the arity requirement
        if(arg_defaults == NULL) return false;

        for(length_t i = type_list_length; i != required_arity; i++){
            // We are missing a necessary default argument value
            if(arg_defaults[i] == NULL) return false;
        }

        // Otherwise, we met the arity requirement.
        // We will then only process the argument values we already have
        // and leave processing and conforming the default arguments to higher level functions
    }

    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);

    // Store a copy of the unmodified function argument values
    ir_value_t **arg_value_list_unmodified = malloc(sizeof(ir_value_t*) * type_list_length);
    memcpy(arg_value_list_unmodified, arg_value_list, sizeof(ir_value_t*) * type_list_length);

    // If the number of arguments supplied exceeds the amount that's required (only the case if varargs), then
    // only conform the non-varargs arguments.
    length_t min_arity = func->arity < type_list_length ? func->arity : type_list_length;

    for(length_t a = 0; a != min_arity; a++){
        if(!ast_types_conform(builder, &arg_value_list[a], &arg_type_list[a], &arg_types[a], conform_mode)){
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
        length_t type_list_length, ast_poly_catalog_t *out_catalog, ast_type_t *gives, trait_t conform_mode){

    errorcode_t res;
    length_t required_arity = poly_template->arity;

    // Ensure argument supplied meet length requirements
    if(
        required_arity < type_list_length &&
        !(poly_template->traits & AST_FUNC_VARARG) &&
        (conform_mode & CONFORM_MODE_VARIADIC ? !(poly_template->traits & AST_FUNC_VARIADIC) : true)
    ) return FAILURE;

    // Determine whether we are missing arguments
    bool requires_use_of_defaults = required_arity > type_list_length;

    if(requires_use_of_defaults){
        // Check to make sure that we have the necessary default values available to later
        // fill in the missing arguments
        
        ast_expr_t **arg_defaults = poly_template->arg_defaults;

        // No default arguments are available to use to attempt to meet the arity requirement
        if(arg_defaults == NULL) return FAILURE;

        for(length_t i = type_list_length; i != required_arity; i++){
            // We are missing a necessary default argument value
            if(arg_defaults[i] == NULL) return FAILURE;
        }

        // Otherwise, we met the arity requirement.
        // We will then only process the argument values we already have
        // and leave processing and conforming the default arguments to higher level functions
    }
    
    ast_poly_catalog_t catalog;
    ast_poly_catalog_init(&catalog);

    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);

    // Store a copy of the unmodified function argument values
    ir_value_t **arg_value_list_unmodified = malloc(sizeof(ir_value_t*) * type_list_length);
    memcpy(arg_value_list_unmodified, arg_value_list, sizeof(ir_value_t*) * type_list_length);

    // Number of polymorphic paramater types that have been processed (used for cleanup)
    length_t i;

    for(i = 0; i != type_list_length; i++){
        if(ast_type_has_polymorph(&poly_template->arg_types[i]))
            res = arg_type_polymorphable(builder->compiler, builder->object, &poly_template->arg_types[i], &arg_types[i], &catalog);
        else
            res = ast_types_conform(builder, &arg_value_list[i], &arg_types[i], &poly_template->arg_types[i], conform_mode) ? SUCCESS : FAILURE;

        if(res != SUCCESS){
            i++;
            goto polymorphic_failure;
        }
    }

    // Ensure return type matches if provided
    if(gives && gives->elements_length != 0){
        res = arg_type_polymorphable(builder->compiler, builder->object, &poly_template->return_type, gives, &catalog);

        if(res != SUCCESS){
            goto polymorphic_failure;
        }

        ast_type_t concrete_return_type;
        if(resolve_type_polymorphics(builder->compiler, builder->type_table, &catalog, &poly_template->return_type, &concrete_return_type)){
            res = FAILURE;
            goto polymorphic_failure;
        }

        bool meets_return_matching_requirement = ast_types_identical(gives, &concrete_return_type);
        ast_type_free(&concrete_return_type);

        if(!meets_return_matching_requirement){
            res = FAILURE;
            goto polymorphic_failure;
        }
    }

    if(out_catalog) *out_catalog = catalog;
    else ast_poly_catalog_free(&catalog);
    free(arg_value_list_unmodified);
    return SUCCESS;

polymorphic_failure:
    // Restore pool snapshot
    ir_pool_snapshot_restore(builder->pool, &snapshot);

    // Undo any modifications to the function arguments
    memcpy(arg_value_list, arg_value_list_unmodified, sizeof(ir_value_t*) * i);
    free(arg_value_list_unmodified);
    ast_poly_catalog_free(&catalog);
    return res;
}

errorcode_t func_args_polymorphable_no_conform(compiler_t *compiler, object_t *object, ast_func_t *poly_template, ast_type_t *arg_types, length_t type_list_length, ast_poly_catalog_t *out_catalog){
    if(type_list_length != poly_template->arity) return FAILURE;

    errorcode_t res;

    ast_poly_catalog_t catalog;
    ast_poly_catalog_init(&catalog);

    for(length_t i = 0; i != type_list_length; i++){
        if(ast_type_has_polymorph(&poly_template->arg_types[i]))
            res = arg_type_polymorphable(compiler, object, &poly_template->arg_types[i], &arg_types[i], &catalog);
        else
            res = ast_types_identical(&arg_types[i], &poly_template->arg_types[i]) ? SUCCESS : FAILURE;

        if(res != SUCCESS){
            i++;
            goto polymorphic_failure;
        }
    }

    if(out_catalog){
        *out_catalog = catalog;
    } else {
        ast_poly_catalog_free(&catalog);
    }

    return SUCCESS;

polymorphic_failure:
    ast_poly_catalog_free(&catalog);
    return res;
}

errorcode_t arg_type_polymorphable(compiler_t *compiler, object_t *object, ast_type_t *polymorphic_type, ast_type_t *concrete_type, ast_poly_catalog_t *catalog){
    if(polymorphic_type->elements_length > concrete_type->elements_length) return FAILURE;

    // TODO: CLEANUP: Cleanup this dirty code

    for(length_t i = 0; i != concrete_type->elements_length; i++){
        length_t poly_elem_id = polymorphic_type->elements[i]->id;

        // Convert polymorphs with prerequisites to plain old polymorphic variables
        if(poly_elem_id == AST_ELEM_POLYMORPH_PREREQ){
            ast_elem_polymorph_prereq_t *prereq = (ast_elem_polymorph_prereq_t*) polymorphic_type->elements[i];

            // Verify that we are at the final element of the concrete type, otherwise it
            // isn't possible for the prerequisite to be fulfilled
            if(i + 1 != concrete_type->elements_length) return FAILURE;

            // NOTE: Must be presorted
            const char *reserved_prereqs[] = {
                "__assign__", "__number__", "__pod__", "__primitive__", "__signed__", "__struct__", "__unsigned__"
            };
            const length_t reserved_prereqs_length = sizeof(reserved_prereqs) / sizeof(weak_cstr_t*);
            maybe_index_t special_prereq = binary_string_search(reserved_prereqs, reserved_prereqs_length, prereq->similarity_prerequisite);
            bool meets_special_prereq = false;

            switch(special_prereq){
            case 0: { // __assign__
                    optional_funcpair_t result;
                    errorcode_t errorcode = ir_gen_find_assign_func(compiler, object, concrete_type, &result);
                    if(errorcode == ALT_FAILURE) return errorcode;
                    meets_special_prereq = errorcode == SUCCESS && result.has;
                }
                break;
            case 1: // __number__
                meets_special_prereq = concrete_type->elements[i]->id == AST_ELEM_BASE && typename_builtin_type(((ast_elem_base_t*) concrete_type->elements[i])->base) != BUILTIN_TYPE_NONE;
                break;
            case 2: { // __pod__
                    optional_funcpair_t result;
                    errorcode_t errorcode = ir_gen_find_assign_func(compiler, object, concrete_type, &result);
                    if(errorcode == ALT_FAILURE) return errorcode;
                    meets_special_prereq = !(errorcode == SUCCESS && result.has);
                }
                break;
            case 3: // __primitive__
                meets_special_prereq = concrete_type->elements[i]->id == AST_ELEM_BASE && typename_is_extended_builtin_type(((ast_elem_base_t*) concrete_type->elements[i])->base);
                break;
            case 4: { // __signed__
                    if(concrete_type->elements[i]->id != AST_ELEM_BASE) break;
                    weak_cstr_t base = ((ast_elem_base_t*) concrete_type->elements[i])->base;

                    if(streq(base, "byte") || streq(base, "short") || streq(base, "int") || streq(base, "long")){
                        meets_special_prereq = true;
                        break;
                    }
                }
                break;
            case 5: // __struct__
                meets_special_prereq = concrete_type->elements[i]->id != AST_ELEM_BASE || !typename_is_extended_builtin_type(((ast_elem_base_t*) concrete_type->elements[i])->base);
                break;
            case 6: { // __unsigned__
                    if(concrete_type->elements[i]->id != AST_ELEM_BASE) break;
                    weak_cstr_t base = ((ast_elem_base_t*) concrete_type->elements[i])->base;

                    if(streq(base, "ubyte") || streq(base, "ushort") || streq(base, "uint") || streq(base, "ulong") || streq(base, "usize")){
                        meets_special_prereq = true;
                        break;
                    }
                }
                break;
            }

            if(!meets_special_prereq && prereq->allow_auto_conversion){
                ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, prereq->name);

                // Determine special allowed auto conversions
                meets_special_prereq = type_var ? is_allowed_auto_conversion(compiler, object, &type_var->binding, concrete_type) : false;
            }

            if(!meets_special_prereq){
                // If we failed a special prereq, then return false
                if(special_prereq != - 1) return FAILURE;

                ast_composite_t *similar = ast_composite_find_exact(&object->ast, prereq->similarity_prerequisite);

                if(similar == NULL){
                    compiler_panicf(compiler, prereq->source, "Undeclared struct '%s'", prereq->similarity_prerequisite);
                    return ALT_FAILURE;
                }

                if(!ast_layout_is_simple_struct(&similar->layout)){
                    compiler_panicf(compiler, prereq->source, "Cannot use complex composite type '%s' as struct prerequisite", prereq->similarity_prerequisite);
                    return ALT_FAILURE;
                }

                length_t field_count = ast_simple_field_map_get_count(&similar->layout.field_map);

                if(concrete_type->elements[i]->id == AST_ELEM_BASE){
                    char *given_name = ((ast_elem_base_t*) concrete_type->elements[i])->base;
                    ast_composite_t *given = ast_composite_find_exact(&object->ast, given_name);

                    if(given == NULL){
                        // Undeclared struct given, no error should be necessary
                        return FAILURE;
                    }

                    // Ensure polymorphic prerequisite met
                    ast_layout_endpoint_t ignore_endpoint;
                    for(length_t f = 0; f != field_count; f++){
                        weak_cstr_t field_name = ast_simple_field_map_get_name_at_index(&similar->layout.field_map, f);

                        // Ensure an endpoint with the same name exists
                        if(!ast_field_map_find(&given->layout.field_map, field_name, &ignore_endpoint)) return FAILURE;
                    }

                    // All fields of struct 'similar' exist in struct 'given'
                    // therefore 'given' is similar to 'similar' and the prerequisite is met
                } else if(concrete_type->elements[i]->id != AST_ELEM_GENERIC_BASE){
                    if(((ast_elem_generic_base_t*) concrete_type->elements[i])->name_is_polymorphic){
                        internalerrorprintf("arg_type_polymorphable() - Encountered polymorphic generic struct name in middle of AST type\n");
                        return ALT_FAILURE;
                    }

                    char *given_name = ((ast_elem_generic_base_t*) concrete_type->elements[i])->name;
                    ast_polymorphic_composite_t *given = ast_polymorphic_composite_find_exact(&object->ast, given_name);

                    if(given == NULL){
                        internalerrorprintf("arg_type_polymorphable() - Failed to find polymophic struct '%s', which should exist\n", given_name);
                        return FAILURE;
                    }

                    // Ensure polymorphic prerequisite met
                    ast_layout_endpoint_t ignore_endpoint;
                    for(length_t f = 0; f != field_count; f++){
                        weak_cstr_t field_name = ast_simple_field_map_get_name_at_index(&similar->layout.field_map, f);

                        // Ensure an endpoint with the same name exists
                        if(!ast_field_map_find(&given->layout.field_map, field_name, &ignore_endpoint)) return FAILURE;
                    }

                    // All fields of struct 'similar' exist in struct 'given'
                    // therefore 'given' is similar to 'similar' and the prerequisite is met
                } else {
                    return FAILURE;
                }
            }
            
            // Ensure there aren't other elements following the polymorphic element
            if(i + 1 != polymorphic_type->elements_length){
                internalerrorprintf("arg_type_polymorphable() - Encountered polymorphic element in middle of AST type\n");
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
            ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, prereq->name);

            if(type_var){
                if(!ast_types_identical(&replacement, &type_var->binding)){
                    // Given arguments don't meet consistency requirements of type variables
                    free(replacement.elements);
                    return FAILURE;
                }
            } else {
                // Add to catalog since it's not already present
                ast_poly_catalog_add_type(catalog, prereq->name, &replacement);
            }

            i = concrete_type->elements_length - 1;
            free(replacement.elements);
            continue;
        }

        // Check whether element is a polymorphic element
        if(poly_elem_id == AST_ELEM_POLYMORPH){
            // Ensure there aren't other elements following the polymorphic element
            if(i + 1 != polymorphic_type->elements_length){
                internalerrorprintf("arg_type_polymorphable() - Encountered polymorphic element in middle of AST type\n");
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
            ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, polymorphic_elem->name);

            if(type_var){
                if(!ast_types_identical(&replacement, &type_var->binding)){
                    if(!polymorphic_elem->allow_auto_conversion || !is_allowed_auto_conversion(compiler, object, &replacement, &type_var->binding)){
                        // Given arguments don't meet consistency requirements of type variables
                        free(replacement.elements);
                        return FAILURE;
                    }
                }
            } else {
                // Add to catalog since it's not already present
                ast_poly_catalog_add_type(catalog, polymorphic_elem->name, &replacement);
            }

            i = concrete_type->elements_length - 1;
            free(replacement.elements);
            continue;
        }

        // Check whether element is a polymorphic count
        if(poly_elem_id == AST_ELEM_POLYCOUNT){
            if(concrete_type->elements[i]->id != AST_ELEM_FIXED_ARRAY){
                // Concrete type must have a fixed array element here
                return FAILURE;
            }

            length_t required_binding = ((ast_elem_fixed_array_t*) concrete_type->elements[i])->length;
            
            // Ensure consistency with catalog
            ast_elem_polycount_t *polycount_elem = (ast_elem_polycount_t*) polymorphic_type->elements[i];
            ast_poly_catalog_count_t *count_var = ast_poly_catalog_find_count(catalog, polycount_elem->name);

            if(count_var){
                // Can't polycount if existing count don't match
                if(count_var->binding != required_binding) return FAILURE;
            } else {
                // Add to catalog since it's not already present
                ast_poly_catalog_add_count(catalog, polycount_elem->name, required_binding);
            }

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
            if(!streq(((ast_elem_base_t*) polymorphic_type->elements[i])->base, ((ast_elem_base_t*) concrete_type->elements[i])->base))
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

                res = arg_type_polymorphable(compiler, object, func_elem_a->return_type, func_elem_b->return_type, catalog);
                if(res != SUCCESS) return res;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    res = arg_type_polymorphable(compiler, object, &func_elem_a->arg_types[a], &func_elem_b->arg_types[a], catalog);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_a = (ast_elem_generic_base_t*) polymorphic_type->elements[i];
                ast_elem_generic_base_t *generic_base_b = (ast_elem_generic_base_t*) concrete_type->elements[i];

                if(generic_base_a->name_is_polymorphic || generic_base_b->name_is_polymorphic){
                    internalerrorprintf("arg_type_polymorphable() - Polymorphic names for generic structs is unimplemented\n");
                    return ALT_FAILURE;
                }

                if(generic_base_a->generics_length != generic_base_b->generics_length) return FAILURE;
                if(!streq(generic_base_a->name, generic_base_b->name)) return FAILURE;

                for(length_t i = 0; i != generic_base_a->generics_length; i++){
                    res = arg_type_polymorphable(compiler, object, &generic_base_a->generics[i], &generic_base_b->generics[i], catalog);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_POLYMORPH:
        case AST_ELEM_POLYMORPH_PREREQ:
            internalerrorprintf("arg_type_polymorphable() - Encountered uncaught polymorphic type element\n");
            return ALT_FAILURE;
        default:
            internalerrorprintf("arg_type_polymorphable() - Encountered unrecognized element ID 0x%8X\n", polymorphic_type->elements[i]->id);
            return ALT_FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t ir_gen_find_singular_special_func(compiler_t *compiler, object_t *object, weak_cstr_t func_name, funcid_t *out_ir_func_id){
    // Finds a special function (such as __variadic_array__)
    // Sets 'out_ir_func_id' ONLY IF the IR function was found.
    // Returns SUCCESS if found
    // Returns FAILURE if not found
    // Returns ALT_FAILURE if something went wrong

    funcpair_t result;
    bool is_unique;
    if(ir_gen_find_func_named(object, func_name, &is_unique, &result) == SUCCESS){
        // Found special function
        
        if(!is_unique && compiler_warnf(compiler, result.ast_func->source, "Using this definition of %s, but there are multiple possibilities", func_name)){
            return ALT_FAILURE;
        }

        *out_ir_func_id = result.ir_func_id;
        return SUCCESS;
    }
    
    return FAILURE;
}
