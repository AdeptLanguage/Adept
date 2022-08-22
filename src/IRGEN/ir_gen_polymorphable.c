
#include <stdbool.h>
#include <stdlib.h>

#include "AST/POLY/ast_translate.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_check_prereq.h"
#include "IRGEN/ir_gen_polymorphable.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/util.h"

static errorcode_t enforce_polymorph(
    compiler_t *compiler,
    object_t *object,
    ast_poly_catalog_t *catalog,
    ast_elem_polymorph_t *polymorph_elem,
    ast_poly_catalog_type_t *type_var,
    ast_type_t *concrete_replacement,
    bool permit_similar_primitives
){
    if(type_var == NULL){
        ast_poly_catalog_add_type(catalog, polymorph_elem->name, concrete_replacement);
        return SUCCESS;
    }

    if(ast_types_identical(concrete_replacement, &type_var->binding)){
        return SUCCESS;
    }

    if((permit_similar_primitives || polymorph_elem->allow_auto_conversion) && is_allowed_builtin_auto_conversion(compiler, object, concrete_replacement, &type_var->binding)){
        return SUCCESS;
    }

    return FAILURE;
}

bool ir_gen_does_extend(compiler_t *compiler, object_t *object, ast_type_t *subject_usage, ast_type_t *parent, ast_poly_catalog_t *catalog){
    if(!ast_type_is_base_like(parent)) return false;

    ast_t *ast = &object->ast;
    ast_type_t *subject = subject_usage;
    ast_type_t subject_storage;

    bool parent_is_generic = ast_type_is_generic_base(parent);
    ast_composite_t *parent_composite = ast_find_composite(ast, parent);

    if(parent_composite == NULL || !parent_composite->is_class) return false;

    while(true){
        if(!ast_type_is_base_like(subject)) goto failure;

        bool subject_is_generic = ast_type_is_generic_base(subject);
        ast_composite_t *subject_composite = ast_find_composite(ast, subject);

        if(!subject_composite->is_class) goto failure;

        errorcode_t errorcode = ir_gen_polymorphable(compiler, object, parent, subject, catalog, true);
        if(errorcode == ALT_FAILURE) goto failure;

        if(subject_is_generic == parent_is_generic && errorcode == SUCCESS){
            goto success;
        }

        if(subject_composite->parent.elements_length == 0){
            break; // No more ancestors to check, did not match
        }

        ast_type_t next_subject;
        if(ast_translate_poly_parent_class(compiler, object, subject_composite, subject, &next_subject)){
            goto failure;
        }

        if(subject == &subject_storage){
            ast_type_free(&subject_storage);
        } else {
            subject = &subject_storage;
        }

        subject_storage = next_subject;
    }

failure:
    if(subject == &subject_storage) ast_type_free(&subject_storage);
    return false;

success:
    if(subject == &subject_storage) ast_type_free(&subject_storage);
    return true;
}

static errorcode_t enforce_prereq(
    compiler_t *compiler,
    object_t *object,
    ast_type_t *polymorphic_type,
    ast_type_t *whole_concrete_type,
    ast_poly_catalog_t *catalog,
    length_t index
){
    ast_elem_polymorph_prereq_t *prereq = (ast_elem_polymorph_prereq_t*) polymorphic_type->elements[index];
    enum special_prereq special_prereq = (enum special_prereq) -1;

    ast_type_t concrete_type = {
        .elements = &whole_concrete_type->elements[index],
        .elements_length = whole_concrete_type->elements_length - index,
        .source = whole_concrete_type->elements[index]->source,
    };

    // Handle special prerequisites
    if(is_special_prerequisite(prereq->similarity_prerequisite, &special_prereq)){
        bool meets_prereq = false;

        if(ir_gen_check_prereq(compiler, object, &concrete_type, special_prereq, &meets_prereq)){
            return FAILURE;
        }
        
        if(meets_prereq){
            return SUCCESS;
        }
    }

    // If we failed a special prereq, then return false
    if(special_prereq != (enum special_prereq) -1) return FAILURE;

    // Enforce struct-field prerequisites
    // `$T~MyStruct`
    if(prereq->similarity_prerequisite){
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

        ast_field_map_t *field_map;
        ast_elem_t *concrete_elem = whole_concrete_type->elements[index];

        if(concrete_elem->id == AST_ELEM_BASE){
            weak_cstr_t given_name = ((ast_elem_base_t*) concrete_elem)->base;
            ast_composite_t *given = ast_composite_find_exact(&object->ast, given_name);

            if(given == NULL){
                // Undeclared struct given, no error should be necessary
                internalerrorprintf("ir_gen_polymorphable() - Failed to find struct '%s', which should exist\n", given_name);
                return FAILURE;
            }

            field_map = &given->layout.field_map;
        } else if(concrete_elem->id != AST_ELEM_GENERIC_BASE){
            ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) concrete_elem;

            if(generic_base_elem->name_is_polymorphic){
                internalerrorprintf("ir_gen_polymorphable() - Encountered polymorphic generic struct name in middle of AST type\n");
                return FAILURE;
            }

            weak_cstr_t given_name = generic_base_elem->name;
            ast_poly_composite_t *given = ast_poly_composite_find_exact(&object->ast, given_name);

            if(given == NULL){
                internalerrorprintf("ir_gen_polymorphable() - Failed to find polymorphic struct '%s', which should exist\n", given_name);
                return FAILURE;
            }

            field_map = &given->layout.field_map;
        } else {
            return FAILURE;
        }

        // Ensure polymorphic prerequisite met
        ast_layout_endpoint_t ignore_endpoint;
        for(length_t f = 0; f != field_count; f++){
            weak_cstr_t field_name = ast_simple_field_map_get_name_at_index(&similar->layout.field_map, f);

            // Ensure an endpoint with the same name exists
            if(!ast_field_map_find(field_map, field_name, &ignore_endpoint)) return FAILURE;
        }
    }

    // Enforce extends prerequisite
    // `$T extends ParentClass`
    if(prereq->extends.elements_length != 0){
        if(!ir_gen_does_extend(compiler, object, &concrete_type, &prereq->extends, catalog)){
            return FAILURE;
        }
    }

    return SUCCESS;
}

static errorcode_t ir_gen_polymorphable_elem_prereq(
    compiler_t *compiler,
    object_t *object,
    ast_type_t *polymorphic_type,
    ast_type_t *concrete_type,
    ast_poly_catalog_t *catalog,
    length_t index,
    bool permit_similar_primitives
){
    // Verify that we are at the final element of the concrete type, otherwise it
    // isn't possible for the prerequisite to be fulfilled
    if(index + 1 != concrete_type->elements_length) return FAILURE;
    
    // Ensure there aren't other elements following the polymorphic element
    if(index + 1 != polymorphic_type->elements_length){
        internalerrorprintf("ir_gen_polymorphable_elem_prereq() - Encountered polymorphic element in middle of AST type\n");
        return FAILURE;
    }

    if(enforce_prereq(compiler, object, polymorphic_type, concrete_type, catalog, index)) return FAILURE;

    // DANGEROUS: AST type with semi-ownership
    length_t replacement_length = concrete_type->elements_length - index;

    ast_type_t replacement = (ast_type_t){
        .elements = memclone((void*) &concrete_type->elements[index], sizeof(ast_elem_t*) * replacement_length),
        .elements_length = replacement_length,
        .source = concrete_type->elements[index]->source,
    };

    // Ensure consistency with catalog
    ast_elem_polymorph_prereq_t *prereq = (ast_elem_polymorph_prereq_t*) polymorphic_type->elements[index];
    ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, prereq->name);

    // Ok since 'ast_elem_polymorph_prereq_t' is guaranteed to overlap with 'ast_elem_polymorph_t'
    errorcode_t res = enforce_polymorph(compiler, object, catalog, (ast_elem_polymorph_t*) prereq, type_var, &replacement, permit_similar_primitives);
    free(replacement.elements);
    return res;
}

errorcode_t ir_gen_polymorphable(compiler_t *compiler, object_t *object, ast_type_t *polymorphic_type, ast_type_t *concrete_type, ast_poly_catalog_t *catalog, bool permit_similar_primitives){
    if(polymorphic_type->elements_length > concrete_type->elements_length) return FAILURE;

    // TODO: CLEANUP: Cleanup this dirty code

    for(length_t i = 0; i != concrete_type->elements_length; i++){
        ast_elem_t *poly_elem = polymorphic_type->elements[i];

        ast_elem_t *concrete_elem = concrete_type->elements[i];

        // To determine which of (SUCCESS, FAILURE, or ALT_FAILURE)
        errorcode_t res;
        
        // Ensure the two non-polymorphic elements match
        switch(poly_elem->id){
        case AST_ELEM_BASE:
        case AST_ELEM_POINTER:
        case AST_ELEM_ARRAY:
        case AST_ELEM_FIXED_ARRAY:
        case AST_ELEM_VAR_FIXED_ARRAY:
            if(!ast_elem_identical(poly_elem, concrete_elem)) return FAILURE;
            break;
        case AST_ELEM_FUNC: {
                if(poly_elem->id != concrete_elem->id) return FAILURE;
                ast_elem_func_t *func_elem_a = (ast_elem_func_t*) polymorphic_type->elements[i];
                ast_elem_func_t *func_elem_b = (ast_elem_func_t*) concrete_type->elements[i];
                if((func_elem_a->traits & AST_FUNC_VARARG) != (func_elem_b->traits & AST_FUNC_VARARG)) return FAILURE;
                if((func_elem_a->traits & AST_FUNC_STDCALL) != (func_elem_b->traits & AST_FUNC_STDCALL)) return FAILURE;
                if(func_elem_a->arity != func_elem_b->arity) return FAILURE;

                res = ir_gen_polymorphable(compiler, object, func_elem_a->return_type, func_elem_b->return_type, catalog, false);
                if(res != SUCCESS) return res;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    res = ir_gen_polymorphable(compiler, object, &func_elem_a->arg_types[a], &func_elem_b->arg_types[a], catalog, false);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                if(poly_elem->id != concrete_elem->id) return FAILURE;
                ast_elem_generic_base_t *generic_base_a = (ast_elem_generic_base_t*) polymorphic_type->elements[i];
                ast_elem_generic_base_t *generic_base_b = (ast_elem_generic_base_t*) concrete_type->elements[i];

                if(generic_base_a->name_is_polymorphic || generic_base_b->name_is_polymorphic){
                    internalerrorprintf("ir_gen_polymorphable() - Polymorphic names for generic structs is unimplemented\n");
                    return ALT_FAILURE;
                }

                if(generic_base_a->generics_length != generic_base_b->generics_length) return FAILURE;
                if(!streq(generic_base_a->name, generic_base_b->name)) return FAILURE;

                for(length_t i = 0; i != generic_base_a->generics_length; i++){
                    res = ir_gen_polymorphable(compiler, object, &generic_base_a->generics[i], &generic_base_b->generics[i], catalog, false);
                    if(res != SUCCESS) return res;
                }
            }
            break;
        case AST_ELEM_POLYMORPH: {
                // Ensure there aren't other elements following the polymorphic element
                if(i + 1 != polymorphic_type->elements_length){
                    internalerrorprintf("ir_gen_polymorphable() - Encountered polymorphic element in middle of AST type\n");
                    return ALT_FAILURE;
                }

                // DANGEROUS: AST type with semi-ownership
                length_t replacement_length = concrete_type->elements_length - i;

                ast_type_t replacement = (ast_type_t){
                    .elements = memclone(&concrete_type->elements[i], sizeof(ast_elem_t*) * replacement_length),
                    .elements_length = replacement_length,
                    .source = concrete_type->elements[i]->source,
                };

                // Ensure consistency with catalog
                ast_elem_polymorph_t *polymorphic_elem = (ast_elem_polymorph_t*) polymorphic_type->elements[i];
                ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, polymorphic_elem->name);

                if(enforce_polymorph(compiler, object, catalog, polymorphic_elem, type_var, &replacement, permit_similar_primitives && i == 0)){
                    free(replacement.elements);
                    return FAILURE;
                }

                i = concrete_type->elements_length - 1;
                free(replacement.elements);
            }
            return SUCCESS;
        case AST_ELEM_POLYMORPH_PREREQ:
            if(ir_gen_polymorphable_elem_prereq(compiler, object, polymorphic_type, concrete_type, catalog, i, permit_similar_primitives && i == 0)){
                return FAILURE;
            }
            return SUCCESS;
        case AST_ELEM_POLYCOUNT: {
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
            }
            break;
        default:
            internalerrorprintf("ir_gen_polymorphable() - Encountered unrecognized element ID 0x%8X\n", polymorphic_type->elements[i]->id);
            return ALT_FAILURE;
        }
    }

    return SUCCESS;
}
