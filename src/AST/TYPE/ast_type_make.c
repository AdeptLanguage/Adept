
#include <stdbool.h>
#include <stdlib.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/ast_type.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"

// =====================================================================
// =                          ast_elem_*_make                          =
// =====================================================================

ast_elem_t *ast_elem_empty_make(unsigned int id, source_t source){
    return (ast_elem_t*) malloc_init(ast_elem_t, {
        .id = id,
        .source = source,
    });
}

ast_elem_t *ast_elem_base_make(strong_cstr_t base, source_t source){
    return (ast_elem_t*) malloc_init(ast_elem_base_t, {
        .id = AST_ELEM_BASE,
        .source = source,
        .base = base,
    });
}

ast_elem_t *ast_elem_generic_base_make(strong_cstr_t base, source_t source, ast_type_t *generics, length_t generics_length){
    return (ast_elem_t*) malloc_init(ast_elem_generic_base_t, {
        .id = AST_ELEM_GENERIC_BASE,
        .source = source,
        .name = base,
        .name_is_polymorphic = false,
        .generics = generics,
        .generics_length = generics_length,
    });
}

ast_elem_t *ast_elem_polymorph_make(strong_cstr_t name, source_t source, bool allow_auto_conversion){
    return (ast_elem_t*) malloc_init(ast_elem_polymorph_t, {
        .id = AST_ELEM_POLYMORPH,
        .source = source,
        .name = name,
        .allow_auto_conversion = allow_auto_conversion,
    });
}

ast_elem_t *ast_elem_polymorph_prereq_make(strong_cstr_t name, source_t source, bool allow_auto_conversion, maybe_null_strong_cstr_t similarity_prerequisite, ast_type_t extends){
    return (ast_elem_t*) malloc_init(ast_elem_polymorph_prereq_t, {
        .id = AST_ELEM_POLYMORPH_PREREQ,
        .source = source,
        .name = name,
        .allow_auto_conversion = allow_auto_conversion,
        .similarity_prerequisite = similarity_prerequisite,
        .extends = extends,
    });
}

ast_elem_t *ast_elem_func_make(source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership){
    return (ast_elem_t*) malloc_init(ast_elem_func_t, {
        .id = AST_ELEM_FUNC,
        .source = source,
        .arg_types = arg_types,
        .arity = arity,
        .return_type = return_type,
        .traits = traits,
        .ownership = have_ownership,
    });
}

ast_elem_t *ast_elem_fixed_array_make(source_t source, length_t count){
    return (ast_elem_t*) malloc_init(ast_elem_fixed_array_t, {
        .id = AST_ELEM_FIXED_ARRAY,
        .source = source,
        .length = count,
    });
}

ast_elem_t *ast_elem_var_fixed_array_make(source_t source, ast_expr_t *length){
    return (ast_elem_t*) malloc_init(ast_elem_var_fixed_array_t, {
        .id = AST_ELEM_VAR_FIXED_ARRAY,
        .source = source,
        .length = length,
    });
}

ast_elem_t *ast_elem_unknown_enum_make(source_t source, weak_cstr_t kind_name){
    return (ast_elem_t*) malloc_init(ast_elem_unknown_enum_t, {
        .id = AST_ELEM_UNKNOWN_ENUM,
        .source = source,
        .kind_name = kind_name,
    });
}

ast_elem_t *ast_elem_unknown_plural_enum_make(source_t source, strong_cstr_list_t kinds){
    return (ast_elem_t*) malloc_init(ast_elem_unknown_plural_enum_t, {
        .id = AST_ELEM_UNKNOWN_PLURAL_ENUM,
        .source = source,
        .kinds = kinds,
    });
}

ast_elem_t *ast_elem_anonymous_enum_make(source_t source, const char **raw_kinds, length_t length){
    // Allocate list of kinds
    strong_cstr_list_t kinds = (strong_cstr_list_t){
        .items = malloc(sizeof(strong_cstr_t) * length),
        .length = length,
        .capacity = length
    };

    // Fill in kinds list
    for(length_t i = 0; i != length; i++){
        kinds.items[i] = strclone(raw_kinds[i]);
    }

    // Sort kinds list
    strong_cstr_list_sort(&kinds);

    // Return completed anonymous enum type element
    return (ast_elem_t*) malloc_init(ast_elem_anonymous_enum_t, {
        .id = AST_ELEM_ANONYMOUS_ENUM,
        .source = source,
        .kinds = kinds,
    });
}

// =====================================================================
// =                            from_*elems                            =
// =====================================================================

static ast_type_t from_1elems(ast_elem_t *a){
    ast_type_t out_type;
    out_type.elements = malloc(sizeof(ast_elem_t*));
    out_type.elements[0] = a;
    out_type.elements_length = 1;
    out_type.source = NULL_SOURCE;
    return out_type;
}

static ast_type_t from_2elems(ast_elem_t *a, ast_elem_t *b){
    ast_type_t out_type;
    out_type.elements = malloc(sizeof(ast_elem_t*) * 2);
    out_type.elements[0] = a;
    out_type.elements[1] = b;
    out_type.elements_length = 2;
    out_type.source = NULL_SOURCE;
    return out_type;
}

static ast_type_t from_3elems(ast_elem_t *a, ast_elem_t *b, ast_elem_t *c){
    ast_type_t out_type;
    out_type.elements = malloc(sizeof(ast_elem_t*) * 3);
    out_type.elements[0] = a;
    out_type.elements[1] = b;
    out_type.elements[2] = c;
    out_type.elements_length = 3;
    out_type.source = NULL_SOURCE;
    return out_type;
}

// =====================================================================
// =                          ast_type_make_*                          =
// =====================================================================

ast_type_t ast_type_make_base(strong_cstr_t base){
    return from_1elems(
        ast_elem_base_make(base, NULL_SOURCE)
    );
}

ast_type_t ast_type_make_base_ptr(strong_cstr_t base){
    return from_2elems(
        ast_elem_pointer_make(NULL_SOURCE),
        ast_elem_base_make(base, NULL_SOURCE)
    );
}

ast_type_t ast_type_make_base_ptr_ptr( strong_cstr_t base){
    return from_3elems(
        ast_elem_pointer_make(NULL_SOURCE),
        ast_elem_pointer_make(NULL_SOURCE),
        ast_elem_base_make(base, NULL_SOURCE)
    );
}

ast_type_t ast_type_make_base_with_polymorphs(strong_cstr_t base, weak_cstr_t *generics, length_t length){
    ast_type_t *polymorphs = ast_type_make_polymorph_list(generics, length);

    return from_1elems(
        ast_elem_generic_base_make(base, NULL_SOURCE, polymorphs, length)
    );
}

ast_type_t ast_type_make_polymorph(strong_cstr_t name, bool allow_auto_conversion){
    return from_1elems(
        ast_elem_polymorph_make(name, NULL_SOURCE, allow_auto_conversion)
    );
}

ast_type_t ast_type_make_polymorph_prereq(strong_cstr_t name, bool allow_auto_conversion, maybe_null_strong_cstr_t similarity_prereq, ast_type_t maybe_extends){
    return from_1elems(
        ast_elem_polymorph_prereq_make(name, NULL_SOURCE, allow_auto_conversion, similarity_prereq, maybe_extends)
    );
}

ast_type_t ast_type_make_func_ptr(source_t source, ast_type_t *arg_types, length_t arity, ast_type_t *return_type, trait_t traits, bool have_ownership){
    return from_1elems(
        ast_elem_func_make(source, arg_types, arity, return_type, traits, have_ownership)
    );
}

ast_type_t ast_type_make_generic_int(void){
    return from_1elems(
        ast_elem_generic_int_make(NULL_SOURCE)
    );
}

ast_type_t ast_type_make_generic_float(void){
    return from_1elems(
        ast_elem_generic_float_make(NULL_SOURCE)
    );
}

ast_type_t ast_type_make_unknown_enum(source_t source, weak_cstr_t kind_name){
    ast_type_t type = from_1elems(
        ast_elem_unknown_enum_make(source, kind_name)
    );

    type.source = source;
    return type;
}

ast_type_t ast_type_make_unknown_plural_enum(source_t source, strong_cstr_list_t kinds){
    ast_type_t type = from_1elems(
        ast_elem_unknown_plural_enum_make(source, kinds)
    );

    type.source = source;
    return type;
}

ast_type_t ast_type_make_anonymous_enum(source_t source, const char **kinds, length_t length){
    ast_type_t type = from_1elems(
        ast_elem_anonymous_enum_make(source, kinds, length)
    );

    type.source = source;
    return type;
}

// =====================================================================
// =                              helpers                              =
// =====================================================================

ast_type_t *ast_type_make_polymorph_list(weak_cstr_t *generics, length_t generics_length){
    ast_type_t *types = malloc(sizeof(ast_type_t) * generics_length);

    for(length_t i = 0; i < generics_length; i++){
        types[i] = ast_type_make_polymorph(strclone(generics[i]), false);
    }

    return types;
}
