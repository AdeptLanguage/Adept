
#include <stdlib.h>

#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "UTIL/ground.h"
#include "UTIL/util.h"

void ast_poly_catalog_init(ast_poly_catalog_t *catalog){
    *catalog = (ast_poly_catalog_t){
        .types = {0},
        .counts = {0},
    };
}

void ast_poly_catalog_free(ast_poly_catalog_t *catalog){
    for(length_t i = 0; i != catalog->types.length; i++){
        ast_type_free(&catalog->types.types[i].binding);
    }
    free(catalog->types.types);
    free(catalog->counts.counts);
}

void ast_poly_catalog_add_type(ast_poly_catalog_t *catalog, weak_cstr_t name, const ast_type_t *binding){
    ast_poly_catalog_types_append(&catalog->types, (
        (ast_poly_catalog_type_t){
            .name = name,
            .binding = ast_type_clone(binding),
        }
    ));
}

void ast_poly_catalog_add_count(ast_poly_catalog_t *catalog, weak_cstr_t name, length_t binding){
    ast_poly_catalog_counts_append(&catalog->counts, (
        (ast_poly_catalog_count_t){
            .name = name,
            .binding = binding,
        }
    ));
}

ast_poly_catalog_type_t *ast_poly_catalog_find_type(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // Linear search is probably the fastest here

    for(length_t i = 0; i != catalog->types.length; i++){
        if(streq(catalog->types.types[i].name, name)) return &catalog->types.types[i];
    }

    return NULL;
}

ast_poly_catalog_count_t *ast_poly_catalog_find_count(ast_poly_catalog_t *catalog, weak_cstr_t name){
    // Linear search is probably the fastest here

    for(length_t i = 0; i != catalog->counts.length; i++){
        if(streq(catalog->counts.counts[i].name, name)) return &catalog->counts.counts[i];
    }

    return NULL;
}
