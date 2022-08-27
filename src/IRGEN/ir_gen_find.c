
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "UTIL/func_pair.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_module.h"
#include "IR/ir_pool.h"
#include "IR/ir_proc_map.h"
#include "IR/ir_proc_query.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_args.h"
#include "IRGEN/ir_gen_expr.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_polymorphable.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

static const trait_t normal_forbidden_traits = AST_FUNC_VIRTUAL | AST_FUNC_OVERRIDE;

static errorcode_t try_to_autogen_proc_to_fill_query(ir_proc_query_t *query, optional_func_pair_t *result);
static errorcode_t ir_gen_find_proc_sweep(ir_proc_query_t *query, optional_func_pair_t *result, unsigned int conform_mode_if_applicable);

errorcode_t ir_gen_find_proc(ir_proc_query_t *query, optional_func_pair_t *result){
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
            errorcode_t res = ir_gen_polymorphable(compiler, object, expected_type, &ast_type, optional_catalog, true);
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

static errorcode_t actualize_suitable_polymorphic(ir_proc_query_t *query, optional_func_pair_t *result, ast_poly_catalog_t *catalog, ir_func_endpoint_t endpoint){
    const func_id_t ast_func_id = endpoint.ast_func_id;

    compiler_t *compiler = ir_proc_query_getter_compiler(query);
    object_t *object = ir_proc_query_getter_object(query);

    // Validate and fill in argument gaps
    {
        ast_func_t *ast_func = &object->ast.funcs[ast_func_id];

        if(ast_func->traits & AST_FUNC_DISALLOW){
            strong_cstr_t display = ast_func_head_str(ast_func);
            compiler_panicf(compiler, query->from_source, "Cannot call disallowed '%s'", display);
            free(display);
            return ALT_FAILURE;
        }

        // DANGEROUS: invalidates pointer 'ast_func'
        if(ir_gen_fill_in_default_arguments(query, ast_func, catalog)){
            return ALT_FAILURE;
        }
    }

    // Instantiate polymorphic function
    ir_func_endpoint_t instance;

    ast_type_t *arg_types = ir_proc_query_getter_arg_types(query);
    length_t arg_types_length = ir_proc_query_getter_length(query);

    if(instantiate_poly_func(compiler, object, query->from_source, endpoint.ast_func_id, arg_types, arg_types_length, catalog, &instance)){
        ast_func_t *poly_func = &object->ast.funcs[ast_func_id];

        strong_cstr_t display = ast_func_head_str(poly_func);
        compiler_panicf(compiler, poly_func->source, "Cannot instantiate polymorphic function with given types", display);

        if(SOURCE_IS_NULL(query->from_source)){
            errorprintf("Could not instantiate `%s` due to errors\n", display);
        } else {
            compiler_panicf(compiler, query->from_source, "Could not instantiate `%s` due to errors", display);
        }

        free(display);
        return ALT_FAILURE;
    }

    optional_func_pair_set(result, true, instance.ast_func_id, instance.ir_func_id);
    return SUCCESS;
}

static errorcode_t actualize_suitable_nonpolymorphic(ir_proc_query_t *query, optional_func_pair_t *result, ir_func_endpoint_t endpoint){
    object_t *object = ir_proc_query_getter_object(query);

    if(ir_gen_fill_in_default_arguments(query, &object->ast.funcs[endpoint.ast_func_id], NULL)){
        return ALT_FAILURE;
    }

    optional_func_pair_set(result, true, endpoint.ast_func_id, endpoint.ir_func_id);
    return SUCCESS;
}

static errorcode_t ir_gen_find_proc_sweep_partial(ir_proc_query_t *query, optional_func_pair_t *result, unsigned int conform_mode_if_applicable, ir_func_endpoint_t endpoint){
    compiler_t *compiler = ir_proc_query_getter_compiler(query);
    object_t *object = ir_proc_query_getter_object(query);
    ast_func_t *ast_func = &object->ast.funcs[endpoint.ast_func_id];

    // Do function trait restrictions
    if((ast_func->traits & query->traits_mask) != query->traits_match || (ast_func->traits & query->forbid_traits)){
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

        // Catalog to remember polymorphic parameter solution
        // 'func_args_polymorphable' will set this to the chosen solution on SUCCESS
        ast_poly_catalog_t catalog; 

        errorcode_t res;

        if(query->conform){
            res = func_args_polymorphable(query->conform_params.builder, ast_func, arg_values, arg_types, arg_types_length, &catalog, query->optional_gives, conform_mode_if_applicable);
        } else {
            res = func_args_polymorphable_no_conform(compiler, object, ast_func, arg_types, arg_types_length, &catalog);
        }

        if(res == SUCCESS){
            res = actualize_suitable_polymorphic(query, result, &catalog, endpoint);
            ast_poly_catalog_free(&catalog);
            return res;
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
            return actualize_suitable_nonpolymorphic(query, result, endpoint);
        }
    }

    return FAILURE;
}

static errorcode_t ir_gen_find_proc_sweep_endpoint_list(
    ir_proc_query_t *query,
    optional_func_pair_t *result,
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
    optional_func_pair_t *result,
    unsigned int conform_mode_if_applicable,
    ir_proc_map_t *proc_map,
    void *key,
    length_t sizeof_key,
    int (*compare)(const void*, const void*)
){
    ir_func_endpoint_list_t *endpoint_list = ir_proc_map_find(proc_map, key, sizeof_key, compare);
    
    return ir_gen_find_proc_sweep_endpoint_list(query, result, conform_mode_if_applicable, endpoint_list);
}

static errorcode_t ir_gen_find_proc_sweep(ir_proc_query_t *query, optional_func_pair_t *result, unsigned int conform_mode_if_applicable){
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

static errorcode_t try_to_autogen_proc_to_fill_query(ir_proc_query_t *query, optional_func_pair_t *result){
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

errorcode_t ir_gen_find_func_named(object_t *object, weak_cstr_t name, bool *out_is_unique, func_pair_t *result, bool allow_polymorphic){
    // Find list of function endpoints for the given name
    ir_func_endpoint_list_t *endpoint_list = ir_proc_map_find(
        &object->ir_module.func_map,
        &(ir_func_key_t){ .name = name },
        sizeof(ir_func_key_t),
        &compare_ir_func_key
    );

    if(endpoint_list == NULL) return FAILURE;

    ir_func_endpoint_t endpoint;

    if(allow_polymorphic){
        assert(endpoint_list->length > 0);
        endpoint = endpoint_list->endpoints[0];
        goto found;
    } else {
        ast_func_t *funcs = object->ast.funcs;

        // Find first endpoint that isn't polymorphic
        for(length_t i = 0; i != endpoint_list->length; i++){
            endpoint = endpoint_list->endpoints[i];

            if(!(funcs[endpoint.ast_func_id].traits & AST_FUNC_POLYMORPHIC)){
                goto found;
            }
        }
    }

    return FAILURE;

found:
    *result = (func_pair_t){
        .ast_func_id = endpoint.ast_func_id,
        .ir_func_id = endpoint.ir_func_id,
    };

    if(out_is_unique){
        *out_is_unique = endpoint_list->length == 1;
    }

    return SUCCESS;
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_regular(&query, compiler, object, function_name, arg_types, arg_types_length, traits_mask, traits_match, TRAIT_NONE, from_source);
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_conforming(&query, builder, function_name, inout_arg_values, inout_arg_types, inout_length, optional_gives, no_user_casts, normal_forbidden_traits, from_source);
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_func_conforming_without_defaults(&query, builder, function_name, arg_values, arg_types, length, optional_gives, no_user_casts, normal_forbidden_traits, from_source);
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_regular(&query, compiler, object, struct_name, method_name, arg_types, arg_types_length, normal_forbidden_traits, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_dispatchee(
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t method_name,
    ast_type_t *arg_types,
    length_t arg_types_length,
    source_t from_source,
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    trait_t forbidden = AST_FUNC_DISPATCHER;
    ir_proc_query_init_find_method_regular(&query, compiler, object, struct_name, method_name, arg_types, arg_types_length, forbidden, from_source);
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_conforming(&query, builder, struct_name, name, inout_arg_values, inout_arg_types, inout_length, gives, normal_forbidden_traits, from_source);
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
    optional_func_pair_t *out_result
){
    ir_proc_query_t query;
    ir_proc_query_init_find_method_conforming_without_defaults(&query, builder, struct_name, name, arg_values, arg_types, length, gives, normal_forbidden_traits, from_source);
    return ir_gen_find_proc(&query, out_result);
}

errorcode_t ir_gen_find_singular_special_func(compiler_t *compiler, object_t *object, weak_cstr_t func_name, func_id_t *out_ir_func_id){
    // Finds a special function (such as __variadic_array__)
    // Sets 'out_ir_func_id' ONLY IF the IR function was found.
    // Returns SUCCESS if found
    // Returns FAILURE if not found
    // Returns ALT_FAILURE if something went wrong

    bool is_unique;
    func_pair_t result;

    if(ir_gen_find_func_named(object, func_name, &is_unique, &result, false) == SUCCESS){
        // Found special function

        source_t source = object->ast.funcs[result.ast_func_id].source;
        
        if(!is_unique && compiler_warnf(compiler, source, "Using this definition of %s, but there are multiple possibilities", func_name)){
            return ALT_FAILURE;
        }

        *out_ir_func_id = result.ir_func_id;
        return SUCCESS;
    }
    
    return FAILURE;
}
