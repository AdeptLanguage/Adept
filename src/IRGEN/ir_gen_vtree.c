
#include "AST/POLY/ast_translate.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast_poly_catalog.h"
#include "IRGEN/ir_gen_find.h"
#include "IRGEN/ir_gen_vtree.h"
#include "IRGEN/ir_vtree.h"

errorcode_t ir_gen_vtree_overrides(
    compiler_t *compiler,
    object_t *object,
    vtree_t *start,
    int depth_left
){
    // NOTE: Requires that tables for ancestors have already been
    // filled in. This will happen naturally, as we will start processing
    // from the roots

    // Enforce maximum family depth
    if(depth_left <= 0){
        strong_cstr_t typename = ast_type_str(&start->signature);
        compiler_panicf(compiler, start->signature.source, "Refusing to search for virtual function overrides in class '%s' with absurdly deeply nested descendents", typename);
        free(typename);
        return FAILURE;
    }

    // Start with existing table from parent
    if(start->parent){
        ir_func_endpoint_list_append_list(&start->table, &start->parent->table);
    }

    // Override ancestor methods when suitable
    for(length_t i = 0; i != start->table.length; i++){
        func_id_t ast_func_id = start->table.endpoints[i].ast_func_id;
        optional_func_pair_t result;

        // Attempt to find suitable method to override with
        if(ir_gen_vtree_search_for_single_override(compiler, object, &start->signature, ast_func_id, &result)){
            return FAILURE;
        }

        // If we found a suitable method to override with,
        if(result.has){
            // Replace the ancestor method's endpoint with the new method's endpoint
            // for this type's table
            start->table.endpoints[i] = (ir_func_endpoint_t){
                .ast_func_id = result.value.ast_func_id,
                .ir_func_id = result.value.ir_func_id,
            };
        }
    }

    ast_t *ast = &object->ast;

    // Add our own virtual methods to our table by their default function endpoints
    // (this may mean instantiating new functions)
    for(length_t i = 0; i != start->virtuals.length; i++){
        func_id_t ast_func_id = start->virtuals.endpoints[i].ast_func_id;
        ast_func_t *ast_func = &ast->funcs[ast_func_id];

        source_t source_on_error = start->signature.source;
        optional_func_pair_t result;

        assert(ast_func->arity >= 1 && ast_type_is_pointer(&ast_func->arg_types[0]));

        ast_type_t subject_non_pointer_type = ast_type_unwrapped_view(&ast_func->arg_types[0]);
        weak_cstr_t struct_name = ast_type_struct_name(&subject_non_pointer_type);

        // Create argument type list for virtual method using concrete subject type
        // NOTE: We shouldn't have to worry about any polymorphs in the argument types,
        //       since we are working with a concrete virtual AST method definition
        ast_type_t *arg_types = ast_types_clone(ast_func->arg_types, ast_func->arity);
        ast_type_free(&arg_types[0]);
        arg_types[0] = ast_type_pointer_to(ast_type_clone(&start->signature));

        // Find the default implementation of our own virtual method
        // This will instantiate a polymorphic function if necessary
        if(ir_gen_find_dispatchee(compiler, object, struct_name, ast_func->name, arg_types, ast_func->arity, source_on_error, &result)){
            strong_cstr_t typename = ast_type_str(&start->signature);
            errorprintf("Could not generate vtable for class '%s' due to errors\n", typename);
            free(typename);
            return FAILURE;
        }

        ast_types_free_fully(arg_types, ast_func->arity);

        // Raise an error if we couldn't find the default implementation for our own virtual method
        if(!result.has){
            strong_cstr_t typename = ast_type_str(&start->signature);
            errorprintf("Could not generate vtable for class '%s' due to errors\n", typename);
            free(typename);

            compiler_panicf(compiler, source_on_error, "Got empty function result");
            compiler_undeclared_function(compiler, object, source_on_error, ast_func->name, arg_types, ast_func->arity, NULL, true);
            return FAILURE;
        }

        // Create function endpoint
        ir_func_endpoint_t endpoint = (ir_func_endpoint_t){
            .ast_func_id = ast_func_id,
            .ir_func_id = result.value.ir_func_id,
        };

        // Append endpoint to table
        ir_func_endpoint_list_append(&start->table, endpoint);
    }

    // Recursively handle children
    for(length_t i = 0; i != start->children.length; i++){
        if(ir_gen_vtree_overrides(compiler, object, start->children.vtrees[i], depth_left - 1)){
            return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t ir_gen_vtree_search_for_single_override(
    compiler_t *compiler,
    object_t *object,
    const ast_type_t *child_subject_type,
    func_id_t ast_func_id,
    optional_func_pair_t *out_result
){
    ast_t *ast = &object->ast;
    ast_func_t *ast_func = &ast->funcs[ast_func_id];

    source_t source_on_error = child_subject_type->source;
    optional_func_pair_t result;

    assert(ast_func->arity > 0 && ast_type_is_pointer(&ast_func->arg_types[0]));

    ast_type_t subject_non_pointer_type = ast_type_unwrapped_view(&ast_func->arg_types[0]);
    weak_cstr_t struct_name = ast_type_struct_name(&subject_non_pointer_type);

    ast_type_t *arg_types = ast_types_clone(ast_func->arg_types, ast_func->arity);
    ast_type_free(&arg_types[0]);
    arg_types[0] = ast_type_pointer_to(ast_type_clone(child_subject_type));

    errorcode_t errorcode = ir_gen_find_dispatchee(compiler, object, struct_name, ast_func->name, arg_types, ast_func->arity, source_on_error, &result);

    // TODO: Clean up code so that this revalidation isn't needed
    // Revalidate AST function
    ast_func = &ast->funcs[ast_func_id];

    switch(errorcode){
    case ALT_FAILURE: {
            strong_cstr_t typename = ast_type_str(child_subject_type);
            errorprintf("Could not generate vtable for class '%s' due to errors\n", typename);
            free(typename);
            goto failure;
        }
    case SUCCESS: {
            ast_func_t *dispatchee = &ast->funcs[result.value.ast_func_id];

            if(result.has){
                if(!(dispatchee->traits & AST_FUNC_OVERRIDE)){
                    compiler_panicf(compiler, dispatchee->source, "Method is used as virtual dispatchee but is missing 'override' keyword");
                    goto failure;
                }

                if(!ast_types_identical(&ast_func->return_type, &dispatchee->return_type)){
                    strong_cstr_t typename = ast_type_str(child_subject_type);
                    strong_cstr_t should_return = ast_type_str(&ast_func->return_type);
                    compiler_panicf(compiler, dispatchee->return_type.source, "Incorrect return type for method override, expected '%s'", should_return);
                    free(should_return);
                    free(typename);
                    goto failure;
                }

                dispatchee->traits |= AST_FUNC_USED_OVERRIDE;
            }

            *out_result = result;
        }
        break;
    default:
        out_result->has = false;
    }

    ast_types_free_fully(arg_types, ast_func->arity);
    return SUCCESS;

failure:
    ast_types_free_fully(arg_types, ast_func->arity);
    return FAILURE;
}

errorcode_t ir_gen_vtree_link_up_nodes(
    compiler_t *compiler,
    object_t *object,
    vtree_list_t *vtree_list,
    length_t start_i
){
    ast_t *ast = &object->ast;

    for(length_t i = start_i; i != vtree_list->length; i++){
        vtree_t *vtree = vtree_list->vtrees[i];

        ast_composite_t *composite = ast_find_composite(ast, &vtree->signature);

        if(composite == NULL){
            strong_cstr_t typename = ast_type_str(&vtree->signature);
            compiler_warnf(compiler, vtree->signature.source, "Failed to find class '%s'", typename);
            free(typename);
            return FAILURE;
        }

        if(!composite->is_class){
            strong_cstr_t typename = ast_type_str(&vtree->signature);
            compiler_warnf(compiler, vtree->signature.source, "Cannot extend non-class '%s'", typename);
            return FAILURE;
        }

        if(composite->parent.elements_length == 0){
            continue; // No parent to link up to
        }

        ast_type_t parent;
        if(ast_translate_poly_parent_class(compiler, object, composite, &vtree->signature, &parent)){
            return FAILURE;
        }

        vtree_t *parent_vtree = vtree_list_find_or_append(vtree_list, &parent);

        if(parent_vtree == NULL){
            strong_cstr_t typename = ast_type_str(&vtree->signature);
            strong_cstr_t parent_typename = ast_type_str(&parent);
            compiler_panicf(compiler, parent.source, "Failed to resolve parent class '%s' for class '%s'", parent_typename, typename);
            free(parent_typename);
            free(typename);
            ast_type_free(&parent);
            return FAILURE;
        }

        ast_type_free(&parent);

        vtree->parent = parent_vtree;
        vtree_list_append(&parent_vtree->children, vtree);
    }

    return SUCCESS;
}

errorcode_t ir_gen_vtree_inject_addition_for_descendants(compiler_t *compiler, object_t *object, virtual_addition_t addition, length_t insertion_point, vtree_t *vtree){
    for(length_t i = 0; i != vtree->children.length; i++){
        vtree_t *child = vtree->children.vtrees[i];
        
        // Search for overriding method
        optional_func_pair_t result;

        if(ir_gen_vtree_search_for_single_override(compiler, object, &child->signature, addition.endpoint.ast_func_id, &result)){
            return FAILURE;
        }

        // Determine our addition's function endpoint based on whether we found an overriding method
        if(result.has){
            addition.endpoint = (ir_func_endpoint_t){
                .ast_func_id = result.value.ast_func_id,
                .ir_func_id = result.value.ir_func_id,
            };
        }

        // Inject determined function endpoint as the override
        ir_func_endpoint_list_insert_at(&child->table, addition.endpoint, insertion_point);

        // Recursively apply to children,
        // If we had an override, then own children's "default" implementation will be our override
        if(ir_gen_vtree_inject_addition_for_descendants(compiler, object, addition, insertion_point, child)){
            return FAILURE;
        }
    }

    return SUCCESS;
}
