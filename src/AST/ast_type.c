
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/hash.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

void ast_type_free(ast_type_t *type){
    ast_elems_free(type->elements, type->elements_length);
    free(type->elements);
}

void ast_type_free_fully(ast_type_t *type){
    if(type == NULL) return;
    ast_type_free(type);
    free(type);
}

void ast_types_free(ast_type_t *types, length_t length){
    for(length_t t = 0; t != length; t++) ast_type_free(&types[t]);
}

void ast_types_free_fully(ast_type_t *types, length_t length){
    for(length_t t = 0; t != length; t++) ast_type_free(&types[t]);
    free(types);
}

void ast_elems_free(ast_elem_t **elements, length_t elements_length){
    for(length_t i = 0; i != elements_length; i++){
        ast_elem_t *elem = elements[i];

        switch(elem->id){
        case AST_ELEM_BASE:
            free(((ast_elem_base_t*) elem)->base);
            break;
        case AST_ELEM_POINTER:
        case AST_ELEM_ARRAY:
        case AST_ELEM_FIXED_ARRAY:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
            break;
        case AST_ELEM_VAR_FIXED_ARRAY: 
            ast_expr_free_fully(((ast_elem_var_fixed_array_t*) elem)->length);
            break;
        case AST_ELEM_FUNC:
            if(((ast_elem_func_t*) elem)->ownership){
                ast_elem_func_t *func_elem = (ast_elem_func_t*) elem;
                ast_types_free(func_elem->arg_types, func_elem->arity);
                free(func_elem->arg_types);
                ast_type_free_fully(func_elem->return_type);
            }
            break;
        case AST_ELEM_POLYCOUNT:
            free(((ast_elem_polycount_t*) elem)->name);
            break;
        case AST_ELEM_POLYMORPH:
            free(((ast_elem_polymorph_t*) elem)->name);
            break;
        case AST_ELEM_POLYMORPH_PREREQ:
            free(((ast_elem_polymorph_prereq_t*) elem)->name);
            free(((ast_elem_polymorph_prereq_t*) elem)->similarity_prerequisite);
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) elem;
                ast_types_free_fully(generic_base_elem->generics, generic_base_elem->generics_length);
                free(generic_base_elem->name);
            }
            break;
        case AST_ELEM_LAYOUT:
            ast_layout_free(&(((ast_elem_layout_t*) elem)->layout));
            break;
        default:
            panic("ast_elems_free() - Unrecognized type element ID at index %zu\n", i);
        }

        free(elem);
    }
}

void ast_elem_free(ast_elem_t *elem){
    ast_elems_free(&elem, 1);
}

void ast_type_prepend_ptr(ast_type_t *type){
    // Prepends a '*' to an existing ast_type_t

    ast_elem_t **new_elements = malloc(sizeof(ast_elem_t*) * (type->elements_length + 1));
    memcpy(&new_elements[1], type->elements, sizeof(ast_elem_t*) * type->elements_length);
    new_elements[0] = ast_elem_pointer_make(NULL_SOURCE);

    free(type->elements);
    type->elements = new_elements;
    type->elements_length++;
}

bool ast_types_identical(const ast_type_t *a, const ast_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must be exactly the same to be considered identical (Exception is 'ulong' and 'usize')
    // NOTE: Even if two types represent the same type, they are still not identical
    // [pointer] [base "ubyte"]    [base "CStringAlias"]      -> false
    // [pointer] [base "ubyte"]    [pointer] [base: "ubyte"]  -> true

    if(a->elements_length != b->elements_length) return false;

    for(length_t i = 0; i != a->elements_length; i++){
        unsigned int id = a->elements[i]->id;

        if(id != b->elements[i]->id) return false;

        // Do more specific checking if needed
        // TODO: CLEANUP: Cleanup this messy code
        switch(id){
        case AST_ELEM_BASE: {
                weak_cstr_t a_name = ((ast_elem_base_t*) a->elements[i])->base;
                weak_cstr_t b_name = ((ast_elem_base_t*) b->elements[i])->base;

                if(streq(a_name, "usize") || streq(a_name, "ulong")){
                    // HACK: Cheap little hack to get 'usize' to be treated the same as 'ulong'
                    // MAYBE TODO: Create special function func non-identical comparison, or modify the name of this function
                    if(streq(b_name, "usize") || streq(b_name, "ulong")) break;
                }
                if(streq(a_name, "successful") || streq(a_name, "bool")){
                    // HACK: Cheap little hack to get 'successful' to be treated the same as 'bool'
                    // MAYBE TODO: Create special function func non-identical comparison, or modify the name of this function
                    if(streq(b_name, "successful") || streq(b_name, "bool")) break;
                }
                if(!streq(a_name, b_name)) return false;
            }
            break;
        case AST_ELEM_FIXED_ARRAY:
            if(((ast_elem_fixed_array_t*) a->elements[i])->length != ((ast_elem_fixed_array_t*) b->elements[i])->length) return false;
            break;
        case AST_ELEM_VAR_FIXED_ARRAY:
            // We cannot know
            internalwarningprintf("ast_types_identical() unexpectedly comparing two AST_ELEM_VAR_FIXED_ARRAY elements. Assuming they are different\n");
            return false;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem_a = (ast_elem_func_t*) a->elements[i];
                ast_elem_func_t *func_elem_b = (ast_elem_func_t*) b->elements[i];
                if((func_elem_a->traits & AST_FUNC_VARARG) != (func_elem_b->traits & AST_FUNC_VARARG)) return false;
                if((func_elem_a->traits & AST_FUNC_STDCALL) != (func_elem_b->traits & AST_FUNC_STDCALL)) return false;
                if(func_elem_a->arity != func_elem_b->arity) return false;
                if(!ast_types_identical(func_elem_a->return_type, func_elem_b->return_type)) return false;

                for(length_t a = 0; a != func_elem_a->arity; a++){
                    if(!ast_types_identical(&func_elem_a->arg_types[a], &func_elem_b->arg_types[a])) return false;
                }
            }
            break;
        case AST_ELEM_POINTER:
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_a = (ast_elem_generic_base_t*) a->elements[i];
                ast_elem_generic_base_t *generic_base_b = (ast_elem_generic_base_t*) b->elements[i];

                if(generic_base_a->name_is_polymorphic || generic_base_b->name_is_polymorphic){
                    panic("ast_types_identical() - Polymorphic names feature for generic structs is unimplemented\n");
                }

                if(generic_base_a->generics_length != generic_base_b->generics_length) return false;
                if(!streq(generic_base_a->name, generic_base_b->name)) return false;

                for(length_t i = 0; i != generic_base_a->generics_length; i++){
                    if(!ast_types_identical(&generic_base_a->generics[i], &generic_base_b->generics[i])) return false;
                }
            }
            break;
        case AST_ELEM_POLYMORPH: {
                ast_elem_polymorph_t *polymorph_a = (ast_elem_polymorph_t*) a->elements[i];
                ast_elem_polymorph_t *polymorph_b = (ast_elem_polymorph_t*) b->elements[i];
                if(!streq(polymorph_a->name, polymorph_b->name)) return false;
                if(polymorph_a->allow_auto_conversion != polymorph_b->allow_auto_conversion) return false;
            }
            break;
        case AST_ELEM_POLYCOUNT: {
                ast_elem_polycount_t *polycount_a = (ast_elem_polycount_t*) a->elements[i];
                ast_elem_polycount_t *polycount_b = (ast_elem_polycount_t*) b->elements[i];
                if(!streq(polycount_a->name, polycount_b->name)) return false;
            }
            break;
        case AST_ELEM_POLYMORPH_PREREQ: {
                ast_elem_polymorph_prereq_t *polymorph_a = (ast_elem_polymorph_prereq_t*) a->elements[i];
                ast_elem_polymorph_prereq_t *polymorph_b = (ast_elem_polymorph_prereq_t*) b->elements[i];
                if(!streq(polymorph_a->name, polymorph_b->name)) return false;
                if(!streq(polymorph_a->similarity_prerequisite, polymorph_b->similarity_prerequisite)) return false;
                if(polymorph_a->allow_auto_conversion != polymorph_b->allow_auto_conversion) return false;
            }
            break;
        case AST_ELEM_LAYOUT: {
                ast_elem_layout_t *layout_elem_a = (ast_elem_layout_t*) a->elements[i];
                ast_elem_layout_t *layout_elem_b = (ast_elem_layout_t*) b->elements[i];

                if(!ast_layouts_identical(&layout_elem_a->layout, &layout_elem_b->layout)) return false;
            }
            break;
        default:
            panic("ast_types_identical() - Unrecognized element ID %d\n", (int) id);
        }
    }

    return true;
}

bool ast_type_lists_identical(const ast_type_t *a, const ast_type_t *b, length_t length){
    for(length_t i = 0; i != length; i++){
        if(!ast_types_identical(&a[i], &b[i])) return false;
    }
    return true;
}

bool ast_type_is_void(const ast_type_t *type){
    return ast_type_is_base_of(type, "void");
}

bool ast_type_is_base(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_BASE) return false;
    return true;
}

bool ast_type_is_base_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_BASE) return false;
    return true;
}

bool ast_type_is_base_of(const ast_type_t *type, const char *base){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_BASE) return false;
    if(!streq(((ast_elem_base_t*) type->elements[0])->base, base)) return false;
    return true;
}

bool ast_type_is_base_ptr_of(const ast_type_t *type, const char *base){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_BASE) return false;
    if(!streq(((ast_elem_base_t*) type->elements[1])->base, base)) return false;
    return true;
}

bool ast_type_is_base_like(const ast_type_t *type){
    if(type->elements_length != 1) return false;

    unsigned int elem_id = type->elements[0]->id;

    if(elem_id == AST_ELEM_BASE) return true;
    if(elem_id == AST_ELEM_GENERIC_BASE) return true;
    return false;
}

bool ast_type_is_pointer(const ast_type_t *type){
    return type->elements_length > 1 && type->elements[0]->id == AST_ELEM_POINTER;
}

bool ast_type_is_pointer_to(const ast_type_t *type, const ast_type_t *to){
    if(type->elements_length < 2 || type->elements_length != to->elements_length + 1) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    ast_type_t stripped = *type;
    ast_elem_t *stripped_elements[to->elements_length];
    memcpy(stripped_elements, &stripped.elements[1], sizeof(ast_elem_t*) * to->elements_length);
    stripped.elements = stripped_elements;
    stripped.elements_length--;
    
    return ast_types_identical(&stripped, to);
}

bool ast_type_is_pointer_to_base(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;
    
    return type->elements[1]->id == AST_ELEM_BASE;
}

bool ast_type_is_pointer_to_generic_base(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    return type->elements[1]->id == AST_ELEM_GENERIC_BASE;
}

bool ast_type_is_pointer_to_base_like(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    unsigned int second_elem_id = type->elements[1]->id;
    if(second_elem_id == AST_ELEM_BASE) return true;
    if(second_elem_id == AST_ELEM_GENERIC_BASE) return true;

    return false;
}

bool ast_type_is_polymorph(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_POLYMORPH) return false;
    return true;
}

bool ast_type_is_polymorph_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_POLYMORPH) return false;
    return true;
}

bool ast_type_is_generic_base(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_GENERIC_BASE) return false;
    return true;
}

bool ast_type_is_generic_base_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER || type->elements[1]->id != AST_ELEM_GENERIC_BASE) return false;
    return true;
}

bool ast_type_is_fixed_array(const ast_type_t *type){
    if(type->elements_length < 2) return false;
    if(type->elements[0]->id != AST_ELEM_FIXED_ARRAY) return false;
    return true;
}

bool ast_type_is_func(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_FUNC) return false;
    return true;
}

bool ast_type_has_polymorph(const ast_type_t *type){
    for(length_t i = 0; i != type->elements_length; i++){
        switch(type->elements[i]->id){
        case AST_ELEM_BASE:
        case AST_ELEM_POINTER:
        case AST_ELEM_GENERIC_INT:
        case AST_ELEM_GENERIC_FLOAT:
        case AST_ELEM_FIXED_ARRAY:
        case AST_ELEM_VAR_FIXED_ARRAY:
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                for(length_t i = 0; i != func_elem->arity; i++){
                    if(ast_type_has_polymorph(&func_elem->arg_types[i])) return true;
                }
                if(ast_type_has_polymorph(func_elem->return_type)) return true;
            }
            break;
        case AST_ELEM_POLYMORPH:
        case AST_ELEM_POLYMORPH_PREREQ:
        case AST_ELEM_POLYCOUNT:
            return true;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) type->elements[i];
                if(generic_base->name_is_polymorphic) return true;
                
                for(length_t i = 0; i != generic_base->generics_length; i++){
                    if(ast_type_has_polymorph(&generic_base->generics[i])) return true;
                }
            }
            break;
        case AST_ELEM_LAYOUT: {
                ast_elem_layout_t *layout_elem = (ast_elem_layout_t*) type->elements[i];
                return ast_layout_skeleton_has_polymorph(&layout_elem->layout.skeleton);
            }
            break;
        default:
            panic("ast_type_has_polymorph() - Unrecognized element ID 0x%08X\n", type->elements[i]->id);
        }
    }
    return false;
}

bool ast_type_list_has_polymorph(const ast_type_t *types, length_t length){
    for(length_t i = 0; i != length; i++){
        if(ast_type_has_polymorph(&types[i])) return true;
    }
    return false;
}

ast_type_t ast_type_dereferenced_view(ast_type_t *pointer_type){
    if(pointer_type->elements_length < 2 || pointer_type->elements[0]->id != AST_ELEM_POINTER){
        panic("ast_type_dereferenced_view() - Cannot dereference non-pointer type\n");
    }

    return (ast_type_t){
        .elements = &pointer_type->elements[1],
        .elements_length = pointer_type->elements_length - 1,
        .source = pointer_type->elements[1]->source,
    };
}

void ast_type_dereference(ast_type_t *inout_type){
    if(inout_type->elements_length < 2 || inout_type->elements[0]->id != AST_ELEM_POINTER){
        panic("ast_type_dereference() - Cannot dereference non-pointer type\n");
    }

    // Modify ast_type_t to remove a pointer element from the front
    // DANGEROUS: Manually deleting ast_elem_pointer_t
    free(inout_type->elements[0]);
    memmove(inout_type->elements, &inout_type->elements[1], sizeof(ast_elem_t*) * (inout_type->elements_length - 1));
    inout_type->elements_length--; // Reduce length accordingly
    inout_type->source = inout_type->elements[0]->source;
}

ast_type_t ast_type_unwrapped_view(ast_type_t *type){
    ast_type_t unwrapped_view;
    unwrapped_view.elements = &type->elements[1];
    unwrapped_view.elements_length = type->elements_length - 1;
    unwrapped_view.source = type->source;
    return unwrapped_view;
}

void ast_type_unwrap_fixed_array(ast_type_t *inout_type){
    if(inout_type->elements_length < 2 || inout_type->elements[0]->id != AST_ELEM_FIXED_ARRAY){
        panic("ast_type_unwrap_fixed_array() - Cannot unwrap non-fixed-array type\n");
    }

    // Modify ast_type_t to remove a fixed-array element from the front
    // DANGEROUS: Manually deleting ast_elem_fixed_array_t
    free(inout_type->elements[0]);
    memmove(inout_type->elements, &inout_type->elements[1], sizeof(ast_elem_t*) * (inout_type->elements_length - 1));
    inout_type->elements_length--; // Reduce length accordingly
}

void ast_poly_catalog_init(ast_poly_catalog_t *catalog){
    catalog->types = NULL;
    catalog->types_length = 0;
    catalog->types_capacity = 0;
    catalog->counts = NULL;
    catalog->counts_length = 0;
    catalog->counts_capacity = 0;
}

void ast_poly_catalog_free(ast_poly_catalog_t *catalog){
    for(length_t i = 0; i != catalog->types_length; i++){
        ast_type_free(&catalog->types[i].binding);
    }
    free(catalog->types);
    free(catalog->counts);
}

void ast_poly_catalog_add_type(ast_poly_catalog_t *catalog, weak_cstr_t name, ast_type_t *binding){
    expand((void**) &catalog->types, sizeof(ast_poly_catalog_type_t), catalog->types_length, &catalog->types_capacity, 1, 4);
    ast_poly_catalog_type_t *type_var = &catalog->types[catalog->types_length++];
    type_var->name = name;
    type_var->binding = ast_type_clone(binding);
}

void ast_poly_catalog_add_count(ast_poly_catalog_t *catalog, weak_cstr_t name, length_t binding){
    expand((void**) &catalog->counts, sizeof(ast_poly_catalog_count_t), catalog->counts_length, &catalog->counts_capacity, 1, 4);
    ast_poly_catalog_count_t *count_var = &catalog->counts[catalog->counts_length++];
    count_var->name = name;
    count_var->binding = binding;
}

ast_poly_catalog_type_t *ast_poly_catalog_find_type(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // TODO: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != catalog->types_length; i++){
        if(streq(catalog->types[i].name, name)) return &catalog->types[i];
    }

    return NULL;
}

ast_poly_catalog_count_t *ast_poly_catalog_find_count(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // TODO: CLEANUP: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != catalog->counts_length; i++){
        if(streq(catalog->counts[i].name, name)) return &catalog->counts[i];
    }

    return NULL;
}

hash_t ast_type_hash(ast_type_t *type){
    hash_t master_hash = 0;

    for(length_t i = 0; i != type->elements_length; i++){
        master_hash = hash_combine(master_hash, ast_elem_hash(type->elements[i]));
    }

    return master_hash;
}

hash_t ast_elem_hash(ast_elem_t *element){
    hash_t element_hash = hash_data(&element->id, sizeof(element->id));
    
    switch(element->id){
    case AST_ELEM_BASE: {
            ast_elem_base_t *base_elem = (ast_elem_base_t*) element;
            element_hash = hash_combine(element_hash, hash_data(base_elem->base, strlen(base_elem->base)));
        }
        break;
    case AST_ELEM_POINTER:
    case AST_ELEM_ARRAY:
    case AST_ELEM_GENERIC_INT:
    case AST_ELEM_GENERIC_FLOAT:
        break;
    case AST_ELEM_FIXED_ARRAY: {
            ast_elem_fixed_array_t *fixed_array_elem = (ast_elem_fixed_array_t*) element;
            element_hash = hash_combine(element_hash, hash_data(&fixed_array_elem->length, sizeof(fixed_array_elem->length)));
        }
        break;
    case AST_ELEM_VAR_FIXED_ARRAY:
        internalwarningprintf("ast_elem_hash() unexpectedly got AST_ELEM_VAR_FIXED_ARRAY element. Returning faux hash\n");
        break;
    case AST_ELEM_FUNC: {
            ast_elem_func_t *func_elem = (ast_elem_func_t*) element;
            for(length_t i = 0; i != func_elem->arity; i++){
                element_hash = hash_combine(element_hash, ast_type_hash(&func_elem->arg_types[i]));
            }
            element_hash = hash_combine(element_hash, ast_type_hash(func_elem->return_type));
            element_hash = hash_combine(element_hash, hash_data(&func_elem->traits, sizeof(func_elem->traits)));
        }
        break;
    case AST_ELEM_POLYMORPH: {
            ast_elem_polymorph_t *polymorph = (ast_elem_polymorph_t*) element;
            element_hash = hash_combine(element_hash, hash_data(polymorph->name, strlen(polymorph->name)));
        }
        break;
    case AST_ELEM_POLYCOUNT: {
            ast_elem_polycount_t *polycount = (ast_elem_polycount_t*) element;
            element_hash = hash_combine(element_hash, hash_data("#", 1));
            element_hash = hash_combine(element_hash, hash_data(polycount->name, strlen(polycount->name)));
        }
        break;
    case AST_ELEM_POLYMORPH_PREREQ: {
            ast_elem_polymorph_prereq_t *polymorph_prereq = (ast_elem_polymorph_prereq_t*) element;
            element_hash = hash_combine(element_hash, hash_data(polymorph_prereq->name, strlen(polymorph_prereq->name)));
            element_hash = hash_combine(element_hash, hash_data(polymorph_prereq->similarity_prerequisite, strlen(polymorph_prereq->similarity_prerequisite)));
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) element;
            element_hash = hash_combine(element_hash, hash_data(generic_base_elem->name, strlen(generic_base_elem->name)));

            for(length_t i = 0; i != generic_base_elem->generics_length; i++){
                element_hash = hash_combine(element_hash, ast_type_hash(&generic_base_elem->generics[i]));
            }

            element_hash = hash_combine(element_hash, hash_data(&generic_base_elem->name_is_polymorphic, sizeof(generic_base_elem->name_is_polymorphic)));
        }
        break;
    default:
        panic("ast_elem_hash() - Unrecognized type element ID\n");
    }

    return element_hash;
}
