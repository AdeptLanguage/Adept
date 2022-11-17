
#include "AST/TYPE/ast_type_identical.h"

#include <stdio.h>

#include "AST/ast.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"

static bool ast_elem_base_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_base_t *a = (ast_elem_base_t*) raw_a;
    ast_elem_base_t *b = (ast_elem_base_t*) raw_b;

    weak_cstr_t a_name = a->base;
    weak_cstr_t b_name = b->base;

    if(streq(a_name, "usize") || streq(a_name, "ulong")){
        // HACK: Cheap little hack to get 'usize' to be treated the same as 'ulong'
        if(streq(b_name, "usize") || streq(b_name, "ulong")) return true;
    }

    if(streq(a_name, "successful") || streq(a_name, "bool")){
        // HACK: Cheap little hack to get 'successful' to be treated the same as 'bool'
        if(streq(b_name, "successful") || streq(b_name, "bool")) return true;
    }

    return streq(a_name, b_name);
}

static bool ast_elem_fixed_array_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_fixed_array_t *a = (ast_elem_fixed_array_t*) raw_a;
    ast_elem_fixed_array_t *b = (ast_elem_fixed_array_t*) raw_b;

    return a->length == b->length;
}

static bool ast_elem_func_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_func_t *a = (ast_elem_func_t*) raw_a;
    ast_elem_func_t *b = (ast_elem_func_t*) raw_b;

    if((a->traits & AST_FUNC_VARARG)  != (b->traits & AST_FUNC_VARARG))  return false;
    if((a->traits & AST_FUNC_STDCALL) != (b->traits & AST_FUNC_STDCALL)) return false;

    if(a->arity != b->arity) return false;
    if(!ast_types_identical(a->return_type, b->return_type)) return false;

    return ast_type_lists_identical(a->arg_types, b->arg_types, a->arity);
}

static bool ast_elem_generic_base_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_generic_base_t *a = (ast_elem_generic_base_t*) raw_a;
    ast_elem_generic_base_t *b = (ast_elem_generic_base_t*) raw_b;

    if(a->name_is_polymorphic || b->name_is_polymorphic){
        die("ast_types_identical() - Polymorphic names feature for generic structs is unimplemented\n");
    }

    if(a->generics_length != b->generics_length) return false;
    if(!streq(a->name, b->name)) return false;

    return ast_type_lists_identical(a->generics, b->generics, a->generics_length);
}

static bool ast_elem_polymorph_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_polymorph_t *a = (ast_elem_polymorph_t*) raw_a;
    ast_elem_polymorph_t *b = (ast_elem_polymorph_t*) raw_b;
    
    if(a->allow_auto_conversion != b->allow_auto_conversion) return false;

    return streq(a->name, b->name);
}

static bool ast_elem_polycount_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_polycount_t *a = (ast_elem_polycount_t*) raw_a;
    ast_elem_polycount_t *b = (ast_elem_polycount_t*) raw_b;

    return streq(a->name, b->name);
}

static bool ast_elem_polymorph_prereq_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_polymorph_prereq_t *a = (ast_elem_polymorph_prereq_t*) raw_a;
    ast_elem_polymorph_prereq_t *b = (ast_elem_polymorph_prereq_t*) raw_b;

    if(a->allow_auto_conversion != b->allow_auto_conversion)           return false;
    if(!streq(a->similarity_prerequisite, b->similarity_prerequisite)) return false;
    if(!ast_types_identical(&a->extends, &b->extends))                 return false;

    return streq(a->name, b->name);
}

static bool ast_elem_layout_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_layout_t *a = (ast_elem_layout_t*) raw_a;
    ast_elem_layout_t *b = (ast_elem_layout_t*) raw_b;

    return ast_layouts_identical(&a->layout, &b->layout);
}

static bool ast_elem_unknown_enum_identical(ast_elem_t *raw_a, ast_elem_t *raw_b){
    ast_elem_unknown_enum_t *a = (ast_elem_unknown_enum_t*) raw_a;
    ast_elem_unknown_enum_t *b = (ast_elem_unknown_enum_t*) raw_b;

    return streq(a->kind_name, b->kind_name);
}

bool ast_types_identical(const ast_type_t *a, const ast_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must be EXACTLY the same to be considered identical
    // NOTE: Regular type aliases are not considered identical
    // EXCEPTIONS: ulong/usize and bool/successful

    if(a->elements_length != b->elements_length) return false;

    for(length_t i = 0; i != a->elements_length; i++){
        ast_elem_t *a_elem = a->elements[i];
        ast_elem_t *b_elem = b->elements[i];

        if(a_elem->id != b_elem->id) return false;

        switch(a_elem->id){
        case AST_ELEM_BASE:
            if(!ast_elem_base_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_POINTER:
            // Always equal to itself
            break;
        case AST_ELEM_GENERIC_BASE:
            if(!ast_elem_generic_base_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_FIXED_ARRAY:
            if(!ast_elem_fixed_array_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_FUNC:
            if(!ast_elem_func_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_POLYMORPH:
            if(!ast_elem_polymorph_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_POLYCOUNT:
            if(!ast_elem_polycount_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_POLYMORPH_PREREQ:
            if(!ast_elem_polymorph_prereq_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_LAYOUT:
            if(!ast_elem_layout_identical(a_elem, b_elem)) return false;
            break;
        case AST_ELEM_VAR_FIXED_ARRAY:
            // ERROR: We cannot know if they are the same size
            internalerrorprintf("ast_types_identical() - Cannot compare uncollapsed AST_ELEM_VAR_FIXED_ARRAY elements\n");
            printf("    Assuming they are different...\n");
            return false;
        case AST_ELEM_UNKNOWN_ENUM:
            if(!ast_elem_unknown_enum_identical(a_elem, b_elem)) return false;
            break;
        default:
            die("ast_types_identical() - Unrecognized element ID %d\n", (int) a_elem->id);
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

bool ast_elem_identical(ast_elem_t *a, ast_elem_t *b){
    ast_type_t type_a = (ast_type_t){
        .elements = &a,
        .elements_length = 1,
        .source = a->source,
    };

    ast_type_t type_b = (ast_type_t){
        .elements = &b,
        .elements_length = 1,
        .source = b->source,
    };

    return ast_types_identical(&type_a, &type_b);
}
