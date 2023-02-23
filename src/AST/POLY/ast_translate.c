
#include <assert.h>
#include <stdlib.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/POLY/ast_translate.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"

errorcode_t ast_translate_poly_parent_class(
    compiler_t *compiler,
    ast_t *ast,
    ast_composite_t *the_child_class,
    const ast_type_t *the_child_class_concrete_usage, 
    ast_type_t *out_type
){
    ast_type_t *poly_extended_type = &the_child_class->parent;

    assert(poly_extended_type->elements_length != 0);

    if(!ast_type_is_generic_base(poly_extended_type) || !ast_type_has_polymorph(poly_extended_type)){
        *out_type = ast_type_clone(poly_extended_type);
        return SUCCESS;
    }

    // Determine what generic type parameters exist and can be used
    weak_cstr_t *generics;
    length_t generics_length;

    if(the_child_class->is_polymorphic){
        ast_poly_composite_t *polymorphic_the_class = (ast_poly_composite_t*) the_child_class;

        // If the child class is polymorphic, then we'll be able to use its type parameters
        // in its "extends" clause
        generics = polymorphic_the_class->generics;
        generics_length = polymorphic_the_class->generics_length;
    } else {
        // Otherwise, we don't have access to any polymorphic type variables
        generics = NULL;
        generics_length = 0;
    }

    // Assert that the number of generic polymorphic parameters for the "extends" usage and the actual parent class are the same
    ast_elem_generic_base_t *concrete_generic_base = (ast_elem_generic_base_t*) the_child_class_concrete_usage->elements[0];
    assert(generics_length == concrete_generic_base->generics_length);

    // Find parent composite
    ast_poly_composite_t *parent_composite = (ast_poly_composite_t*) ast_find_composite(ast, poly_extended_type);
    
    if(parent_composite == NULL){
        const char *missing_parent_name = ((ast_elem_generic_base_t*) poly_extended_type->elements[0])->name;

        strong_cstr_t typename = ast_type_str(the_child_class_concrete_usage);
        compiler_panicf(compiler, the_child_class->source, "Cannot find parent class '%s' for type '%s'", missing_parent_name, typename);
        free(typename);
        return FAILURE;
    }

    // Assert that the stated parent type is a polymorphic class
    assert(parent_composite->is_class);
    assert(parent_composite->is_polymorphic);

    // Create polymorph catalog
    ast_poly_catalog_t catalog;
    ast_poly_catalog_init(&catalog);
    ast_poly_catalog_add_types(&catalog, generics, concrete_generic_base->generics, generics_length);

    // Resolve polymorphs in the stated parent type
    errorcode_t errorcode = ast_resolve_type_polymorphs(compiler, NULL, &catalog, poly_extended_type, out_type);
    ast_poly_catalog_free(&catalog);
    return errorcode ? FAILURE : SUCCESS;
}
