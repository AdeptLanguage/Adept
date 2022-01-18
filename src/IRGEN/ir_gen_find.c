
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
#include "IR/ir_func_endpoint.h"
#include "IR/ir_pool.h"
#include "IR/ir_proc_map.h"
#include "IR/ir_proc_query.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_cache.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/builtin_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/trait.h"

static errorcode_t try_to_autogen_proc_to_fill_query(ir_proc_query_t *query, optional_funcpair_t *result);
static errorcode_t ir_gen_find_proc_sweep(ir_proc_query_t *query, optional_funcpair_t *result, unsigned int conform_mode_if_applicable);

errorcode_t ir_gen_find_proc(ir_proc_query_t *query, optional_funcpair_t *result){
    // Allow using empty type for 'optional_gives' instead of NULL
    if(query->optional_gives && query->optional_gives->elements_length == 0){
        query->optional_gives = NULL;
    }

    if(query->conform){
        const unsigned int strict_mode = CONFORM_MODE_CALL_ARGUMENTS;
        const unsigned int loose_mode = query->conform_params.no_user_casts ? CONFORM_MODE_CALL_ARGUMENTS_LOOSE_NOUSER : CONFORM_MODE_CALL_ARGUMENTS_LOOSE;

        errorcode_t res = ir_gen_find_proc_sweep(query, result, strict_mode);
        if(res != FAILURE) return res;

        return ir_gen_find_proc_sweep(query, result, loose_mode);
    } else {
        return ir_gen_find_proc_sweep(query, result, CONFORM_MODE_NOT_APPLICABLE);
    }
}

static errorcode_t ir_gen_fill_in_default_arguments(ir_proc_query_t *query, ast_func_t *ast_func, ast_poly_catalog_t *optional_catalog){
    // Don't fill in default values if not allowed or don't have enough information
    if(!query->conform || !query->allow_default_values) return SUCCESS;
    
    ast_expr_t **target_defaults = ast_func->arg_defaults;
    length_t target_arity = ast_func->arity;
    length_t provided_arity = *query->with_defaults.inout_length;
    
    // No need to fill in missing arguments if we already have them
    if(target_defaults == NULL || provided_arity >= target_arity) return SUCCESS;

    ir_builder_t *builder = query->conform_params.builder;
    ir_value_t ***arg_values = query->with_defaults.inout_arg_values;
    ast_type_t **arg_types = query->with_defaults.inout_arg_types;
    ast_type_t *all_expected_arg_types = ast_func->arg_types;

    // ------------------------------------------------
    // | 0 | 1 | 2 |                    Supplied
    // | 0 | 1 | 2 | 3 | 4 |            Required
    // |   | 1 |   | 3 | 4 |            Defaults
    // ------------------------------------------------
    
    // Allocate memory to hold full version of argument list
    ir_value_t **new_args = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * target_arity);
    ast_type_t *new_arg_types = malloc(sizeof(ast_type_t) * target_arity);

    // Copy given argument values into new array
    memcpy(new_args, *arg_values, sizeof(ir_value_t*) * provided_arity);

    // Copy AST types of given arguments into new array
    memcpy(new_arg_types, *arg_types, sizeof(ast_type_t) * provided_arity);

    // Attempt to fill in missing values
    for(length_t i = provided_arity; i < target_arity; i++){
        // We should have never received a function that can't be completed using its default values
        if(target_defaults[i] == NULL){
            compiler_panicf(builder->compiler, ast_func->source, "INTERNAL ERROR: Failed to fill in default value for argument %d", i);
            ast_types_free(&new_arg_types[provided_arity], i - provided_arity);
            return ALT_FAILURE;
        }
        
        // Generate IR value for given default expression
        ast_type_t *expected_type = &all_expected_arg_types[i];

        ir_value_t *ir_value;
        ast_type_t ast_type;
        if(ir_gen_expr(builder, target_defaults[i], &ir_value, false, &ast_type)){
            ast_types_free(&new_arg_types[provided_arity], i - provided_arity);
            return ALT_FAILURE;
        }

        // If polymorphism and compatible, don't conform
        if(optional_catalog && ast_type_has_polymorph(expected_type)){
            compiler_t *compiler = ir_proc_query_getter_compiler(query);
            object_t *object = ir_proc_query_getter_object(query);

            // Polymorphism for argument
            errorcode_t res = arg_type_polymorphable(compiler, object, expected_type, &ast_type, optional_catalog);
            if(res == ALT_FAILURE){
                ast_type_free(&ast_type);
                ast_types_free(&new_arg_types[provided_arity], i - provided_arity);
                return ALT_FAILURE;
            }

            if(res == FAILURE){
                strong_cstr_t a_type_str = ast_type_str(&ast_type);
                strong_cstr_t b_type_str = ast_type_str(expected_type);
                const char *error_format = "Received value of type '%s' for default argument which expects type '%s'";
                compiler_panicf(builder->compiler, expected_type->source, error_format, a_type_str, b_type_str);
                free(a_type_str);
                free(b_type_str);
                ast_type_free(&ast_type);
                ast_types_free(&new_arg_types[provided_arity], i - provided_arity);
                return ALT_FAILURE;
            }

            new_arg_types[i] = ast_type;
        } else {
            // No polymorphism for argument

            if(!ast_types_conform(builder, &ir_value, &ast_type, expected_type, CONFORM_MODE_CALL_ARGUMENTS_LOOSE)){
                strong_cstr_t a_type_str = ast_type_str(&ast_type);
                strong_cstr_t b_type_str = ast_type_str(expected_type);
                const char *error_format = "Received value of type '%s' for default argument which expects type '%s'";
                compiler_panicf(builder->compiler, expected_type->source, error_format, a_type_str, b_type_str);
                free(a_type_str);
                free(b_type_str);
                ast_type_free(&ast_type);
                ast_types_free(&new_arg_types[provided_arity], i - provided_arity);
                return ALT_FAILURE;
            }

            new_arg_types[i] = ast_type_clone(&all_expected_arg_types[i]);
            ast_type_free(&ast_type);
        }

        new_args[i] = ir_value;
    }

    // Swap out partial argument list for full argument list.
    // NOTE: We are abandoning the memory held by 'arg_values'
    //       Since it is a part of the pool, it'll just remain stagnant until
    //       the pool is freed
    *arg_values = new_args;
    
    // Replace argument types array
    free(*arg_types);
    *arg_types = new_arg_types;

    // Update arity
    *query->with_defaults.inout_length = target_arity;

    // We've successfully filled in the missing arguments
    return SUCCESS;
}

static errorcode_t ir_gen_find_proc_sweep_partial(ir_proc_query_t *query, optional_funcpair_t *result, unsigned int conform_mode_if_applicable, ir_func_endpoint_t endpoint){
    compiler_t *compiler = ir_proc_query_getter_compiler(query);
    object_t *object = ir_proc_query_getter_object(query);
    ast_func_t *ast_func = &object->ast.funcs[endpoint.ast_func_id];

    // Do function trait restrictions
    if((ast_func->traits & query->traits_mask) != query->traits_match){
        return FAILURE;
    }

    // If we are looking for a method and the potential match is not a method,
    // then don't match against it
    if(ir_proc_query_is_method(query) && !ast_func_is_method(ast_func)){
        return FAILURE;
    }

    ir_value_t **arg_values = ir_proc_query_getter_arg_values(query);
    ast_type_t *arg_types = ir_proc_query_getter_arg_types(query);
    length_t arg_types_length = ir_proc_query_getter_length(query);

    if(ast_func->traits & AST_FUNC_POLYMORPHIC){
        // Polymorphism

        // Catalog to remember polymophic parameter solution
        // 'func_args_polymorphable' will set this to the chosen solution on SUCCESS
        ast_poly_catalog_t catalog; 

        errorcode_t res;

        if(query->conform){
            res = func_args_polymorphable(query->conform_params.builder, ast_func, arg_values, arg_types, arg_types_length, &catalog, query->optional_gives, conform_mode_if_applicable);
        } else {
            res = func_args_polymorphable_no_conform(compiler, object, ast_func, arg_types, arg_types_length, &catalog);
        }

        if(res == SUCCESS){
            // (NOTE: invalidates pointer 'ast_func' and a lot of other stuff)
            if(ir_gen_fill_in_default_arguments(query, ast_func, &catalog)){
                return ALT_FAILURE;
            }

            ast_type_t *arg_types = ir_proc_query_getter_arg_types(query);
            length_t arg_types_length = ir_proc_query_getter_length(query);
            
            ir_func_endpoint_t instance;
            if(instantiate_poly_func(compiler, object, query->from_source, endpoint.ast_func_id, arg_types, arg_types_length, &catalog, &instance)){
                ast_poly_catalog_free(&catalog);
                return ALT_FAILURE;
            }

            ast_poly_catalog_free(&catalog);
            optional_funcpair_set(result, true, instance.ast_func_id, instance.ir_func_id, object);
            return SUCCESS;
        } else if(res == ALT_FAILURE){
            return res;
        }
    } else {
        // No polymorphism

        successful_t was_successful;

        if(query->conform){
            was_successful = func_args_conform(query->conform_params.builder, ast_func, arg_values, arg_types, arg_types_length, query->optional_gives, conform_mode_if_applicable);
        } else {
            was_successful = func_args_match(ast_func, arg_types, arg_types_length);
        }

        if(was_successful){
            // (NOTE: invalidates pointer 'ast_func' and a lot of other stuff)
            if(ir_gen_fill_in_default_arguments(query, ast_func, NULL)){
                return ALT_FAILURE;
            }

            optional_funcpair_set(result, true, endpoint.ast_func_id, endpoint.ir_func_id, object);
            return SUCCESS;
        }
    }

    return FAILURE;
}

static errorcode_t ir_gen_find_proc_sweep_endpoint_list(
    ir_proc_query_t *query,
    optional_funcpair_t *result,
    unsigned int conform_mode_if_applicable,
    ir_func_endpoint_list_t *endpoint_list
){
    if(endpoint_list == NULL) return FAILURE;

    for(length_t i = 0; i != endpoint_list->length; i++){
        ir_func_endpoint_t endpoint = endpoint_list->endpoints[i];

        errorcode_t res = ir_gen_find_proc_sweep_partial(query, result, conform_mode_if_applicable, endpoint);
        if(res != FAILURE) return res;
    }

    return FAILURE;
}

static errorcode_t ir_gen_find_proc_sweep_proc_map(
    ir_proc_query_t *query,
    optional_funcpair_t *result,
    unsigned int conform_mode_if_applicable,
    ir_proc_map_t *proc_map,
    void *key,
    length_t sizeof_key,
    int (*compare)(const void*, const void*)
){
    ir_func_endpoint_list_t *endpoint_list = ir_proc_map_find(proc_map, key, sizeof_key, compare);
    
    return ir_gen_find_proc_sweep_endpoint_list(query, result, conform_mode_if_applicable, endpoint_list);
}

static errorcode_t ir_gen_find_proc_sweep(ir_proc_query_t *query, optional_funcpair_t *result, unsigned int conform_mode_if_applicable){
    errorcode_t res;
    ir_module_t *ir_module = &ir_proc_query_getter_object(query)->ir_module;

    if(ir_proc_query_is_method(query)){
        // If we are trying to find a method, then search
        // the method procedure map first.

        res = ir_gen_find_proc_sweep_proc_map(
            query,
            result,
            conform_mode_if_applicable,
            &ir_module->method_map,
            &(ir_method_key_t){
                .method_name = query->proc_name,
                .struct_name = query->struct_name,
            },
            sizeof(ir_method_key_t),
            &compare_ir_method_key
        );

        if(res != FAILURE) return res;

        // It doesn't exist in the methods procedure map,
        // so that means that the subject type
        // of the method is unconventional, which
        // requires us to search for the method
        // in the full list of functions.
        // ...
    }

    // Search the function procedure map
    res = ir_gen_find_proc_sweep_proc_map(
        query,
        result,
        conform_mode_if_applicable,
        &ir_module->func_map,
        &(ir_func_key_t){
            .name = query->proc_name
        },
        sizeof(ir_func_key_t),
        &compare_ir_func_key
    );

    if(res != FAILURE) return res;

    return try_to_autogen_proc_to_fill_query(query, result);
}

static errorcode_t try_to_autogen_proc_to_fill_query(ir_proc_query_t *query, optional_funcpair_t *result){
    // Attempt to auto-generate a procedure in order to fill a query

    ast_type_t *types = ir_proc_query_getter_arg_types(query);
    length_t length = ir_proc_query_getter_length(query);

    // Get query-agnostic handles to required compiler components
    compiler_t *compiler = ir_proc_query_getter_compiler(query);
    object_t *object = ir_proc_query_getter_object(query);

    if(streq(query->proc_name, "__defer__")){
        return attempt_autogen___defer__(compiler, object, types, length, result);
    }

    if(streq(query->proc_name, "__assign__")){
        return attempt_autogen___assign__(compiler, object, types, length, result);
    }

    if(streq(query->proc_name, "__pass__") && ir_proc_query_is_function(query)){
        return attempt_autogen___pass__(compiler, object, types, length, result);
    }

    return FAILURE;
}

errorcode_t ir_gen_find_func_named(object_t *object, weak_cstr_t name, bool *out_is_unique, funcpair_t *result){
    // Find list of function endpoints for the given name
    ir_func_endpoint_list_t *endpoint_list = ir_proc_map_find(
        &object->ir_module.func_map,
        &(ir_func_key_t){ .name = name },
        sizeof(ir_func_key_t),
        &compare_ir_func_key
    );

    // Return the first of them if it exists
    if(endpoint_list){
        ir_func_endpoint_t endpoint = endpoint_list->endpoints[0];

        *result = (funcpair_t){
            .ast_func = &object->ast.funcs[endpoint.ast_func_id],
            .ir_func = &object->ir_module.funcs.funcs[endpoint.ir_func_id],
            .ast_func_id = endpoint.ast_func_id,
            .ir_func_id = endpoint.ir_func_id,
        };

        if(out_is_unique){
            *out_is_unique = endpoint_list->length == 1;
        }

        return SUCCESS;
    } else {
        return FAILURE;
    }
}

errorcode_t ir_gen_find_func_regular(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t function_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    trait_t traits_mask,
    trait_t traits_match,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_regular(&query, compiler, object, function_name, arg_types, arg_types_length, traits_mask, traits_match, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_func_conforming(
    ir_builder_t *builder,
    weak_cstr_t function_name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_conforming(&query, builder, function_name, inout_arg_values, inout_arg_types, inout_length, optional_gives, no_user_casts, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_func_conforming_without_defaults(
    ir_builder_t *builder,
    weak_cstr_t function_name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_conforming_without_defaults(&query, builder, function_name, arg_values, arg_types, length, optional_gives, no_user_casts, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_method(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t method_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_regular(&query, compiler, object, struct_name, method_name, arg_types, arg_types_length, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_method_conforming(
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *gives,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_conforming(&query, builder, struct_name, name, inout_arg_values, inout_arg_types, inout_length, gives, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_method_conforming_without_defaults(
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *gives,
    source_t from_source,
    optional_funcpair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_conforming_without_defaults(&query, builder, struct_name, name, arg_values, arg_types, length, gives, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_pass_func(ir_builder_t *builder, ir_value_t **argument, ast_type_t *arg_type, optional_funcpair_t *result){
    // Finds the correct __pass__ function for a type
    // NOTE: Returns SUCCESS when a function was found,
    //               FAILURE when a function wasn't found and
    //               ALT_FAILURE when something goes wrong

    ir_gen_sf_cache_entry_t *cache_entry = ir_gen_sf_cache_locate_or_insert(&builder->object->ir_module.sf_cache, arg_type);

    if(cache_entry->has_pass == TROOLEAN_TRUE){
        optional_funcpair_set(result, true, cache_entry->pass_ast_func_id, cache_entry->pass_ir_func_id, builder->object);
        return SUCCESS;
    } else if(cache_entry->has_pass == TROOLEAN_FALSE){
        result->has = false;
        return SUCCESS;
    }

    // Whether we have a __pass__ function is unknown, so lets try to see if we have one
    return ir_gen_find_func_conforming_without_defaults(builder, "__pass__", argument, arg_type, 1, NULL, true, NULL_SOURCE, result);
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

    weak_cstr_t struct_name;

    switch(arg_type->elements[0]->id){
    case AST_ELEM_BASE:
        struct_name = ((ast_elem_base_t*) arg_type->elements[0])->base;
        break;
    case AST_ELEM_GENERIC_BASE:
        struct_name = ((ast_elem_generic_base_t*) arg_type->elements[0])->name;
        break;
    default:
        struct_name = NULL;
    }

    // Try to find '__defer__' method
    errorcode_t errorcode;

    if(struct_name){
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__defer__", &ast_type_ptr, 1, NULL_SOURCE, result);
    } else {
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
        errorcode = ir_gen_find_method(compiler, object, struct_name, "__assign__", args, 2, NULL_SOURCE, result);
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

successful_t func_args_match(ast_func_t *func, ast_type_t *type_list, length_t type_list_length){
    ast_type_t *arg_types = func->arg_types;
    length_t arity = func->arity;

    if(func->traits & AST_FUNC_VARARG){
        if(type_list_length < arity) return false;
    } else {
        if(type_list_length != arity) return false;
    }

    return ast_type_lists_identical(arg_types, type_list, arity);
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

    // We will make weak copy of fields of 'poly_template' we will need to access,
    // since it the location of the function in memory may move during conformation process
    ast_type_t poly_template_return_type = poly_template->return_type;
    ast_type_t *poly_template_arg_types = poly_template->arg_types;

    // Number of polymorphic paramater types that have been processed (used for cleanup)
    length_t i;

    for(i = 0; i != type_list_length; i++){
        if(ast_type_has_polymorph(&poly_template_arg_types[i]))
            res = arg_type_polymorphable(builder->compiler, builder->object, &poly_template_arg_types[i], &arg_types[i], &catalog);
        else
            res = ast_types_conform(builder, &arg_value_list[i], &arg_types[i], &poly_template_arg_types[i], conform_mode) ? SUCCESS : FAILURE;

        if(res != SUCCESS){
            i++;
            goto polymorphic_failure;
        }
    }

    // Ensure return type matches if provided
    if(gives && gives->elements_length != 0){
        res = arg_type_polymorphable(builder->compiler, builder->object, &poly_template_return_type, gives, &catalog);

        if(res != SUCCESS){
            goto polymorphic_failure;
        }

        ast_type_t concrete_return_type;
        if(resolve_type_polymorphics(builder->compiler, builder->type_table, &catalog, &poly_template_return_type, &concrete_return_type)){
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

    bool is_unique;
    funcpair_t result;

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
