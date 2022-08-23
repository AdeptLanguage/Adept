
#include <stdbool.h>

#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast_type.h"
#include "UTIL/ground.h"

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

    ast_type_t dereferenced_view = ast_type_dereferenced_view(type);
    return ast_types_identical(&dereferenced_view, to);
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

bool ast_type_is_polymorph_like_ptr(const ast_type_t *type){
    if(type->elements_length != 2) return false;
    if(type->elements[0]->id != AST_ELEM_POINTER) return false;

    unsigned int elem_type = type->elements[1]->id;
    if(elem_type != AST_ELEM_POLYMORPH && elem_type != AST_ELEM_POLYMORPH_PREREQ) return false;

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

bool ast_type_is_fixed_array_like(const ast_type_t *type){
    if(type->elements_length < 2) return false;

    unsigned int wrapper = type->elements[0]->id;
    return wrapper == AST_ELEM_FIXED_ARRAY || wrapper == AST_ELEM_POLYCOUNT;
}

bool ast_type_is_func(const ast_type_t *type){
    if(type->elements_length != 1) return false;
    if(type->elements[0]->id != AST_ELEM_FUNC) return false;
    return true;
}
