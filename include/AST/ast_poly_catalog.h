
#ifndef _ISAAC_AST_POLY_CATALOG_H
#define _ISAAC_AST_POLY_CATALOG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    =========================== ast_poly_catalog.h ===========================
    Polymorphic mapping catalog 'ast_poly_catalog_t' definition.

    Polymorphic catalogs are used to map polymorphic type parameters such as '$T'
    to concrete types like 'int', and to map polymorphic count parameters such
    as $#N to concrete integers like 8
    ---------------------------------------------------------------------------
*/

#include "UTIL/list.h"
#include "UTIL/ground.h"
#include "AST/ast_type_lean.h"

// ---------------- ast_poly_catalog_type_t ----------------
// Single polymorphic type binding
typedef struct {
    weak_cstr_t name;
    ast_type_t binding;
} ast_poly_catalog_type_t;

// ---------------- ast_poly_catalog_count_t ----------------
// Single polymorphic count binding
typedef struct {
    weak_cstr_t name;
    length_t binding;
} ast_poly_catalog_count_t;

// ---------------- ast_poly_catalog_types_t ----------------
typedef listof(ast_poly_catalog_type_t, types) ast_poly_catalog_types_t;

// ---------------- ast_poly_catalog_counts_t ----------------
typedef listof(ast_poly_catalog_count_t, counts) ast_poly_catalog_counts_t;

// ---------------- ast_poly_catalog_t ----------------
// Catalog of polymorphic bindings
typedef struct {
    ast_poly_catalog_types_t  types;
    ast_poly_catalog_counts_t counts;
} ast_poly_catalog_t;

// ---------------- ast_poly_catalog_init ----------------
// Initializes a polymorphic catalog
void ast_poly_catalog_init(ast_poly_catalog_t *catalog);

// ---------------- ast_poly_catalog_free ----------------
// Frees a polymorphic catalog
void ast_poly_catalog_free(ast_poly_catalog_t *catalog);

// ---------------- ast_poly_catalog_add_type ----------------
// Adds a type binding to a polymorphic catalog
void ast_poly_catalog_add_type(ast_poly_catalog_t *catalog, weak_cstr_t name, const ast_type_t *binding);

// ---------------- ast_poly_catalog_add_count ----------------
// Adds a count binding to polymorphic catalog
void ast_poly_catalog_add_count(ast_poly_catalog_t *catalog, weak_cstr_t name, length_t binding);

// ---------------- ast_poly_catalog_find_type ----------------
// Finds a type binding within a polymorphic catalog
ast_poly_catalog_type_t *ast_poly_catalog_find_type(ast_poly_catalog_t *catalog, weak_cstr_t name);

// ---------------- ast_poly_catalog_find_count ----------------
// Finds a count binding within polymorphic catalog
ast_poly_catalog_count_t *ast_poly_catalog_find_count(ast_poly_catalog_t *catalog, weak_cstr_t name);

// ---------------- ast_poly_catalog_types_append ----------------
// Appends a single type binding to a polymorphic type binding list
#define ast_poly_catalog_types_append(LIST, VALUE)  list_append(LIST, VALUE, ast_poly_catalog_type_t)

// ---------------- ast_poly_catalog_counts_append ----------------
// Appends a single count binding to a polymorphic count binding list
#define ast_poly_catalog_counts_append(LIST, VALUE) list_append(LIST, VALUE, ast_poly_catalog_count_t)

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_POLY_CATALOG_H
