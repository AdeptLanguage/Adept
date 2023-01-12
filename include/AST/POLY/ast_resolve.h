
#ifndef _ISAAC_AST_RESOLVE_H
#define _ISAAC_AST_RESOLVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AST/ast_expr_lean.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type_lean.h"
#include "BRIDGE/rtti_collector.h"
#include "DRVR/compiler.h"
#include "UTIL/ground.h"

// ---------------- ast_resolve_type_polymorphs ----------------
// Resolves any polymorphic variables within an AST type
// NOTE: Will show error messages on failure
// NOTE: in_type == out_type is allowed
// NOTE: out_type is same as in_type if out_type == null
// NOTE: Will also give result to rtti_collector if not NULL and !(compiler->traits & COMPILER_NO_TYPEINFO)
errorcode_t ast_resolve_type_polymorphs(compiler_t *compiler, rtti_collector_t *rtti_collector, ast_poly_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type);

// ---------------- ast_resolve_expr_polymorphs ----------------
// Resolves any polymorphic variables within an AST expression
errorcode_t ast_resolve_expr_polymorphs(compiler_t *compiler, rtti_collector_t *rtti_collector, ast_poly_catalog_t *catalog, ast_expr_t *expr);

// ---------------- ast_resolve_exprs_polymorphs ----------------
// Resolves any polymorphic variables within a list of AST expressions
errorcode_t ast_resolve_exprs_polymorphs(
    compiler_t *compiler,
    rtti_collector_t *rtti_collector,
    ast_poly_catalog_t *catalog,
    ast_expr_t **exprs,
    length_t count
);

// ---------------- ast_resolve_expr_list_polymorphs ----------------
// Resolves any polymorphic variables within an AST expression list
// NOTE: This function is just a helper for using ast_expr_list_t values for 'ast_resolve_exprs_polymorphs'
errorcode_t ast_resolve_expr_list_polymorphs(
    compiler_t *compiler,
    rtti_collector_t *rtti_collector,
    ast_poly_catalog_t *catalog,
    ast_expr_list_t *exprs
);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_RESOLVE_H
