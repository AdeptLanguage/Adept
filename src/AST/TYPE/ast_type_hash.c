
#include <string.h>

#include "AST/TYPE/ast_type_hash.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/hash.h"
#include "UTIL/trait.h"

static hash_t ast_elem_base_hash(ast_elem_base_t *elem, hash_t working_hash){
    return hash_combine(working_hash, hash_data(elem->base, strlen(elem->base)));
}

static hash_t ast_elem_fixed_array_hash(ast_elem_fixed_array_t *elem, hash_t working_hash){
    return hash_combine(working_hash, hash_data(&elem->length, sizeof(elem->length)));
}

static hash_t ast_elem_func_hash(ast_elem_func_t *elem, hash_t working_hash){
    working_hash = hash_combine(working_hash, ast_types_hash(elem->arg_types, elem->arity));
    working_hash = hash_combine(working_hash, ast_type_hash(elem->return_type));
    return         hash_combine(working_hash, hash_data(&elem->traits, sizeof(elem->traits)));
}

static hash_t ast_elem_polymorph_hash(ast_elem_polymorph_t *elem, hash_t working_hash){
    return hash_combine(working_hash, hash_data(elem->name, strlen(elem->name)));
}

static hash_t ast_elem_polycount_hash(ast_elem_polycount_t *elem, hash_t working_hash){
    working_hash = hash_combine(working_hash, hash_data("#", 1));
    return         hash_combine(working_hash, hash_data(elem->name, strlen(elem->name)));
}

static hash_t ast_elem_polymorph_prereq_hash(ast_elem_polymorph_prereq_t *elem, hash_t working_hash){
    working_hash = hash_combine(working_hash, hash_data(elem->similarity_prerequisite, strlen(elem->similarity_prerequisite)));
    working_hash = hash_combine(working_hash, hash_data(elem->name, strlen(elem->name)));
    working_hash = elem->extends.elements_length != 0 ? hash_combine(working_hash, ast_type_hash(&elem->extends)) : working_hash;
    return working_hash;
}

static hash_t ast_elem_generic_base_hash(ast_elem_generic_base_t *elem, hash_t working_hash){
    working_hash = hash_combine(working_hash, hash_data(&elem->name_is_polymorphic, sizeof(elem->name_is_polymorphic)));
    working_hash = hash_combine(working_hash, hash_data(elem->name, strlen(elem->name)));
    return         hash_combine(working_hash, ast_types_hash(elem->generics, elem->generics_length));
}

static hash_t ast_elem_unknown_enum_hash(ast_elem_unknown_enum_t *elem, hash_t working_hash){
    return hash_combine(working_hash, hash_data(elem->kind_name, strlen(elem->kind_name)));
}

static hash_t ast_elem_hash(ast_elem_t *elem){
    hash_t id_hash = hash_data(&elem->id, sizeof(elem->id));
    
    switch(elem->id){
    case AST_ELEM_BASE:
        return ast_elem_base_hash((ast_elem_base_t*) elem, id_hash);
    case AST_ELEM_POINTER:
    case AST_ELEM_ARRAY:
    case AST_ELEM_GENERIC_INT:
    case AST_ELEM_GENERIC_FLOAT:
        // No unique data to hash against beside element ID, which we already accounted for
        break;
    case AST_ELEM_FIXED_ARRAY:
        return ast_elem_fixed_array_hash((ast_elem_fixed_array_t*) elem, id_hash);
    case AST_ELEM_FUNC:
        return ast_elem_func_hash((ast_elem_func_t*) elem, id_hash);
    case AST_ELEM_POLYMORPH:
        return ast_elem_polymorph_hash((ast_elem_polymorph_t*) elem, id_hash);
    case AST_ELEM_POLYCOUNT:
        return ast_elem_polycount_hash((ast_elem_polycount_t*) elem, id_hash);
    case AST_ELEM_POLYMORPH_PREREQ:
        return ast_elem_polymorph_prereq_hash((ast_elem_polymorph_prereq_t*) elem, id_hash);
    case AST_ELEM_GENERIC_BASE:
        return ast_elem_generic_base_hash((ast_elem_generic_base_t*) elem, id_hash);
    case AST_ELEM_VAR_FIXED_ARRAY:
        internalwarningprintf("ast_elem_hash() - Cannot hash AST_ELEM_VAR_FIXED_ARRAY element, returning faux hash\n");
        break;
    case AST_ELEM_UNKNOWN_ENUM:
        return ast_elem_unknown_enum_hash((ast_elem_unknown_enum_t*) elem, id_hash);
    default:
        die("ast_elem_hash() - Unrecognized type element ID\n");
    }

    return id_hash;
}

hash_t ast_type_hash(ast_type_t *type){
    hash_t master_hash = 0;

    for(length_t i = 0; i != type->elements_length; i++){
        master_hash = hash_combine(master_hash, ast_elem_hash(type->elements[i]));
    }

    return master_hash;
}

hash_t ast_types_hash(ast_type_t *types, length_t length){
    hash_t hash = 0;

    for(length_t i = 0; i != length; i++){
        hash = hash_combine(hash, ast_type_hash(&types[i]));
    }

    return hash;
}
