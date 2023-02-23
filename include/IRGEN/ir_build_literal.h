
#ifndef _ISAAC_IR_BUILD_LITERAL_H
#define _ISAAC_IR_BUILD_LITERAL_H

#include <stdbool.h>

#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGE/bridge.h"
#include "BRIDGE/rtti_collector.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_type_map.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/datatypes.h"
#include "UTIL/func_pair.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"

// ---------------- build_struct_literal ----------------
// Builds a static struct
ir_value_t *build_struct_literal(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable);

// ---------------- build_const_struct_literal ----------------
// Builds a struct construction
ir_value_t *build_const_struct_literal(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length);

// ---------------- build_offsetof ----------------
// Builds an 'offsetof' value
ir_value_t *build_offsetof(ir_builder_t *builder, ir_type_t *type, length_t index);
ir_value_t *build_offsetof_ex(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type, length_t index);

// ---------------- build_const_sizeof ----------------
// Builds constant 'sizeof' value
ir_value_t *build_const_sizeof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type);

// ---------------- build_const_alignof ----------------
// Builds constant 'alignof' value
ir_value_t *build_const_alignof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type);

// ---------------- build_const_add ----------------
// Builds constant 'add' value
// NOTE: Does NOT check whether 'a' and 'b' are of the same type
ir_value_t *build_const_add(ir_pool_t *pool, ir_value_t *a, ir_value_t *b);

// ---------------- build_array_literal ----------------
// Builds a static array
ir_value_t *build_array_literal(ir_pool_t *pool, ir_type_t *item_type, ir_value_t **values, length_t length);

// ---------------- build_literal_int ----------------
// Builds a literal int value
ir_value_t *build_literal_int(ir_pool_t *pool, adept_int value);

// ---------------- build_literal_usize ----------------
// Builds a literal usize value
ir_value_t *build_literal_usize(ir_pool_t *pool, adept_usize value);

// ---------------- build_unknown_enum_value ----------------
// Builds an enum value of an unknown type
ir_value_t *build_unknown_enum_value(ir_pool_t *pool, source_t source, weak_cstr_t kind_name);

// ---------------- build_literal_str ----------------
// Builds a literal string value
// NOTE: If no 'String' type is present, an error will be printed and NULL will be returned
ir_value_t *build_literal_str(ir_builder_t *builder, char *array, length_t length);

// ---------------- build_literal_cstr ----------------
// Builds a literal c-string value
ir_value_t *build_literal_cstr(ir_builder_t *builder, weak_cstr_t value);
ir_value_t *build_literal_cstr_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value);

// ---------------- build_literal_cstr ----------------
// Builds a literal c-string value
ir_value_t *build_literal_cstr_of_size(ir_builder_t *builder, weak_cstr_t value, length_t size);
ir_value_t *build_literal_cstr_of_size_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value, length_t size);

// ---------------- build_null_pointer ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer(ir_pool_t *pool);

// ---------------- build_null_pointer_of_type ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type);

// ---------------- build_literal_func_addr ----------------
// Builds a literal function address
ir_value_t *build_func_addr(ir_pool_t *pool, ir_type_t *result_type, func_id_t ir_func_id);
ir_value_t *build_func_addr_by_name(ir_pool_t *pool, ir_type_t *result_type, const char *name);

// ---------------- build_const_cast ----------------
// Builds a constant cast value and returns it
// NOTE: const_cast_value_type is a valid VALUE_TYPE_* cast value
ir_value_t *build_const_cast(ir_pool_t *pool, unsigned int const_cast_value_type, ir_value_t *from, ir_type_t *to);

#define build_const_bitcast(pool, from, to)     build_const_cast(pool, VALUE_TYPE_CONST_BITCAST, from, to)
#define build_const_zext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_ZEXT, from, to)
#define build_const_sext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_SEXT, from, to)
#define build_const_trunc(pool, from, to)       build_const_cast(pool, VALUE_TYPE_CONST_TRUNC, from, to)
#define build_const_fext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_FEXT, from, to)
#define build_const_ftrunc(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FTRUNC, from, to)
#define build_const_inttoptr(pool, from, to)    build_const_cast(pool, VALUE_TYPE_CONST_INTTOPTR, from, to)
#define build_const_ptrtoint(pool, from, to)    build_const_cast(pool, VALUE_TYPE_CONST_PTRTOINT, from, to)
#define build_const_fptoui(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FPTOUI, from, to)
#define build_const_fptosi(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FPTOSI, from, to)
#define build_const_uitofp(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_UITOFP, from, to)
#define build_const_sitofp(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_SITOFP, from, to)
#define build_const_reinterpret(pool, from, to) build_const_cast(pool, VALUE_TYPE_CONST_REINTERPRET, from, to)

// ---------------- build_bool ----------------
// Builds a literal boolean value
ir_value_t *build_bool(ir_pool_t *pool, adept_bool value);

#endif // _ISAAC_IR_BUILD_LITERAL_H
