
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast.h"
#include "AST/ast_expr_lean.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir_pool.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_args.h"
#include "IRGEN/ir_gen_polymorphable.h"
#include "IRGEN/ir_gen_type.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

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
    if(required_arity < type_list_length){
        if(!(poly_template->traits & AST_FUNC_VARARG)){
            if(!(conform_mode & CONFORM_MODE_VARIADIC) || !(poly_template->traits & AST_FUNC_VARIADIC)){
                return FAILURE;
            }
        }
    }

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
    ir_value_t **arg_value_list_unmodified = memclone(arg_value_list, sizeof(ir_value_t*) * type_list_length);

    // We will make weak copy of fields of 'poly_template' we will need to access,
    // since it the location of the function may move in memory during the conformation process
    ast_type_t poly_template_return_type = poly_template->return_type;
    ast_type_t *poly_template_arg_types = poly_template->arg_types;

    // Number of polymorphic paramater types that have been processed (used for cleanup)
    length_t i;

    length_t num_conforms = length_min(type_list_length, required_arity);

    for(i = 0; i != num_conforms; i++){
        if(ast_type_has_polymorph(&poly_template_arg_types[i]))
            res = ir_gen_polymorphable(builder->compiler, builder->object, &poly_template_arg_types[i], &arg_types[i], &catalog, true);
        else
            res = ast_types_conform(builder, &arg_value_list[i], &arg_types[i], &poly_template_arg_types[i], conform_mode) ? SUCCESS : FAILURE;

        if(res != SUCCESS){
            i++;
            goto polymorphic_failure;
        }
    }

    // Ensure return type matches if provided
    if(gives && gives->elements_length != 0){
        res = ir_gen_polymorphable(builder->compiler, builder->object, &poly_template_return_type, gives, &catalog, false);

        if(res != SUCCESS){
            goto polymorphic_failure;
        }

        ast_type_t concrete_return_type;
        if(ast_resolve_type_polymorphs(builder->compiler, builder->type_table, &catalog, &poly_template_return_type, &concrete_return_type)){
            res = FAILURE;
            goto polymorphic_failure;
        }

        bool matches_return_type = ast_types_identical(gives, &concrete_return_type);
        ast_type_free(&concrete_return_type);

        if(!matches_return_type){
            compiler_panicf(builder->compiler, gives->source, "Unable to match requested return type with callee's return type");
            res = FAILURE;
            goto polymorphic_failure;
        }
    }

    if(out_catalog){
        *out_catalog = catalog;
    } else {
        ast_poly_catalog_free(&catalog);
    }

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
            res = ir_gen_polymorphable(compiler, object, &poly_template->arg_types[i], &arg_types[i], &catalog, false);
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
