
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"

static ast_elem_t *ast_elem_empty_clone(const ast_elem_t*);
static ast_elem_t *ast_elem_base_clone(const ast_elem_base_t*);
static ast_elem_t *ast_elem_fixed_array_clone(const ast_elem_fixed_array_t*);
static ast_elem_t *ast_elem_var_fixed_array_clone(const ast_elem_var_fixed_array_t*);
static ast_elem_t *ast_elem_polycount_clone(const ast_elem_polycount_t*);
static ast_elem_t *ast_elem_func_clone(const ast_elem_func_t*);
static ast_elem_t *ast_elem_polymorph_clone(const ast_elem_polymorph_t*);
static ast_elem_t *ast_elem_polymorph_prereq_clone(const ast_elem_polymorph_prereq_t*);
static ast_elem_t *ast_elem_generic_base_clone(const ast_elem_generic_base_t*);
static ast_elem_t *ast_elem_layout_clone(const ast_elem_layout_t*);

ast_type_t ast_type_clone(const ast_type_t *original){
    return (ast_type_t){
        .elements = ast_elems_clone(original->elements, original->elements_length),
        .elements_length = original->elements_length,
        .source = original->source,
    };
}

ast_elem_t *ast_elem_clone(const ast_elem_t *original){
    switch(original->id){
    case AST_ELEM_BASE:
        return ast_elem_base_clone((ast_elem_base_t*) original);
    case AST_ELEM_POINTER:
        return ast_elem_empty_clone(original);
    case AST_ELEM_ARRAY:
        return ast_elem_empty_clone(original);
    case AST_ELEM_FIXED_ARRAY:
        return ast_elem_fixed_array_clone((ast_elem_fixed_array_t*) original);
    case AST_ELEM_VAR_FIXED_ARRAY:
        return ast_elem_var_fixed_array_clone((ast_elem_var_fixed_array_t*) original);
    case AST_ELEM_POLYCOUNT:
        return ast_elem_polycount_clone((ast_elem_polycount_t*) original);
    case AST_ELEM_GENERIC_INT:
        return ast_elem_empty_clone(original);
    case AST_ELEM_GENERIC_FLOAT:
        return ast_elem_empty_clone(original);
    case AST_ELEM_FUNC:
        return ast_elem_func_clone((ast_elem_func_t*) original);
    case AST_ELEM_POLYMORPH:
        return ast_elem_polymorph_clone((ast_elem_polymorph_t*) original);
    case AST_ELEM_POLYMORPH_PREREQ:
        return ast_elem_polymorph_prereq_clone((ast_elem_polymorph_prereq_t*) original);
    case AST_ELEM_GENERIC_BASE:
        return ast_elem_generic_base_clone((ast_elem_generic_base_t*) original);
    case AST_ELEM_LAYOUT:
        return ast_elem_layout_clone((ast_elem_layout_t*) original);
    default:
        die("ast_elem_clone() - Unrecognized type element ID\n");
    }

    return NULL;
}

static ast_elem_t *ast_elem_empty_clone(const ast_elem_t *original){
    return (ast_elem_t*) memcpy(
        malloc(sizeof(ast_elem_t)),
        original,
        sizeof(ast_elem_t)
    );
}

static ast_elem_t *ast_elem_base_clone(const ast_elem_base_t *original){
    ast_elem_base_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_base_t){
        .id = AST_ELEM_BASE,
        .source = original->source,
        .base = strclone(original->base),
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_fixed_array_clone(const ast_elem_fixed_array_t *original){
    ast_elem_fixed_array_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_fixed_array_t){
        .id = AST_ELEM_FIXED_ARRAY,
        .source = original->source,
        .length = original->length,
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_var_fixed_array_clone(const ast_elem_var_fixed_array_t *original){
    ast_elem_var_fixed_array_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_var_fixed_array_t){
        .id = AST_ELEM_VAR_FIXED_ARRAY,
        .source = original->source,
        .length = ast_expr_clone(original->length),
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_polycount_clone(const ast_elem_polycount_t *original){
    ast_elem_polycount_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_polycount_t){
        .id = AST_ELEM_POLYCOUNT,
        .source = original->source,
        .name = strclone(original->name),
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_func_clone(const ast_elem_func_t *original){
    ast_elem_func_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_func_t){
        .id = AST_ELEM_FUNC,
        .source = original->source,
        .arg_types = ast_types_clone(original->arg_types, original->arity),
        .arity = original->arity,
        .return_type = ast_types_clone(original->return_type, 1),
        .traits = original->traits,
        .ownership = true,
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_polymorph_clone(const ast_elem_polymorph_t *original){
    ast_elem_polymorph_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_polymorph_t){
        .id = AST_ELEM_POLYMORPH,
        .source = original->source,
        .name = strclone(original->name),
        .allow_auto_conversion = original->allow_auto_conversion,
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_polymorph_prereq_clone(const ast_elem_polymorph_prereq_t *original){
    ast_elem_polymorph_prereq_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_polymorph_prereq_t){
        .id = AST_ELEM_POLYMORPH_PREREQ,
        .source = original->source,
        .name = strclone(original->name),
        .similarity_prerequisite = strclone(original->similarity_prerequisite),
        .allow_auto_conversion = original->allow_auto_conversion,
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_generic_base_clone(const ast_elem_generic_base_t *original){
    ast_elem_generic_base_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_generic_base_t){
        .id = AST_ELEM_GENERIC_BASE,
        .source = original->source,
        .name = strclone(original->name),
        .generics = ast_types_clone(original->generics, original->generics_length),
        .generics_length = original->generics_length,
        .name_is_polymorphic = original->name_is_polymorphic,
    };

    return (ast_elem_t*) clone;
}

static ast_elem_t *ast_elem_layout_clone(const ast_elem_layout_t *original){
    ast_elem_layout_t *clone = malloc(sizeof *original);

    *clone = (ast_elem_layout_t){
        .id = AST_ELEM_LAYOUT,
        .source = original->source,
        .layout = ast_layout_clone(&original->layout),
    };

    return (ast_elem_t*) clone;
}

// -------------------------------------------------------------------------------------------

ast_type_t *ast_types_clone(ast_type_t *types, length_t length){
    ast_type_t *new_types = malloc(sizeof(ast_type_t) * length);

    for(length_t i = 0; i != length; i++){
        new_types[i] = ast_type_clone(&types[i]);
    }

    return new_types;
}

ast_elem_t **ast_elems_clone(ast_elem_t **elements, length_t length){
    ast_elem_t **new_elements = malloc(sizeof(ast_elem_t*) * length);

    for(length_t i = 0; i != length; i++){
        new_elements[i] = ast_elem_clone(elements[i]);
    }

    return new_elements;
}
