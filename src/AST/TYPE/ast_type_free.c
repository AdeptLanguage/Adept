
#include <stdlib.h>

#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"

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
        case AST_ELEM_UNKNOWN_ENUM:
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
        case AST_ELEM_POLYMORPH_PREREQ: {
                ast_elem_polymorph_prereq_t *poly_prereq_elem = (ast_elem_polymorph_prereq_t*) elem;
                free(poly_prereq_elem->name);
                free(poly_prereq_elem->similarity_prerequisite);
                ast_type_free(&poly_prereq_elem->extends);
            }
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
            die("ast_elems_free() - Unrecognized type element ID %zu at index %zu\n", (size_t) elem->id, i);
        }

        free(elem);
    }
}

void ast_elem_free(ast_elem_t *elem){
    ast_elems_free(&elem, 1);
}
