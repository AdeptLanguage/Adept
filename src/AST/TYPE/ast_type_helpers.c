
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/string.h"

void ast_type_prepend_ptr(ast_type_t *type){
    // Prepends a '*' to an existing ast_type_t

    ast_elem_t **new_elements = malloc(sizeof(ast_elem_t*) * (type->elements_length + 1));
    memcpy(&new_elements[1], type->elements, sizeof(ast_elem_t*) * type->elements_length);
    new_elements[0] = ast_elem_pointer_make(NULL_SOURCE, false);

    free(type->elements);
    type->elements = new_elements;
    type->elements_length++;
}

ast_type_t ast_type_pointer_to(ast_type_t type){
    ast_type_prepend_ptr(&type);
    return type;
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
        case AST_ELEM_UNKNOWN_ENUM:
            break;
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func_elem = (ast_elem_func_t*) type->elements[i];
                if(ast_type_list_has_polymorph(func_elem->arg_types, func_elem->arity)) return true;
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
                if(ast_type_list_has_polymorph(generic_base->generics, generic_base->generics_length)) return true;
            }
            break;
        case AST_ELEM_LAYOUT: {
                ast_elem_layout_t *layout_elem = (ast_elem_layout_t*) type->elements[i];
                return ast_layout_skeleton_has_polymorph(&layout_elem->layout.skeleton);
            }
            break;
        default:
            die("ast_type_has_polymorph() - Unrecognized element ID 0x%08X\n", type->elements[i]->id);
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

ast_type_t ast_type_dereferenced_view(const ast_type_t *pointer_type){
    if(pointer_type->elements_length < 2 || pointer_type->elements[0]->id != AST_ELEM_POINTER){
        die("ast_type_dereferenced_view() - Cannot dereference non-pointer type\n");
    }

    return (ast_type_t){
        .elements = &pointer_type->elements[1],
        .elements_length = pointer_type->elements_length - 1,
        .source = pointer_type->elements[1]->source,
    };
}

void ast_type_dereference(ast_type_t *inout_type){
    if(inout_type->elements_length < 2 || inout_type->elements[0]->id != AST_ELEM_POINTER){
        die("ast_type_dereference() - Cannot dereference non-pointer type\n");
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
        die("ast_type_unwrap_fixed_array() - Cannot unwrap non-fixed-array type\n");
    }

    // Modify ast_type_t to remove a fixed-array element from the front
    // DANGEROUS: Manually deleting ast_elem_fixed_array_t
    free(inout_type->elements[0]);
    memmove(inout_type->elements, &inout_type->elements[1], sizeof(ast_elem_t*) * (inout_type->elements_length - 1));
    inout_type->elements_length--; // Reduce length accordingly
}

maybe_null_weak_cstr_t ast_type_base_name(const ast_type_t *type){
    if(type->elements_length == 0) return NULL;

    switch(type->elements[0]->id){
    case AST_ELEM_BASE:
        return ((ast_elem_base_t*) type->elements[0])->base;
    case AST_ELEM_GENERIC_BASE:
        return ((ast_elem_generic_base_t*) type->elements[0])->name;
    default:
        return NULL;
    }
}

successful_t ast_type_unknown_enum_like_extract_kinds(const ast_type_t *ast_type, strong_cstr_list_t *out_into_list){
    if(ast_type_is_unknown_enum(ast_type)){
        ast_elem_unknown_enum_t *unknown_enum = (ast_elem_unknown_enum_t*) ast_type->elements[0];

        strong_cstr_list_append(out_into_list, strclone(unknown_enum->kind_name));
        return true;
    }

    if(ast_type_is_unknown_plural_enum(ast_type)){
        ast_elem_unknown_plural_enum_t *unknown_plural_enum = (ast_elem_unknown_plural_enum_t*) ast_type->elements[0];

        for(length_t i = 0; i != unknown_plural_enum->kinds.length; i++){
            strong_cstr_list_append(out_into_list, strclone(unknown_plural_enum->kinds.items[i]));
        }

        return true;
    }
    
    return false;
}

maybe_index_t ast_elem_anonymous_enum_get_member_index(ast_elem_anonymous_enum_t *anonymous_enum, const char *member_name){
    return binary_string_search(anonymous_enum->kinds.items, anonymous_enum->kinds.length, member_name);
}
