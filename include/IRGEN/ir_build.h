
#ifndef _ISAAC_IR_BUILD_H
#define _ISAAC_IR_BUILD_H

/*
    =============================== ir_builder.h ===============================
    Module for building intermediate representation
    ----------------------------------------------------------------------------
*/

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

// ---------------- build_varptr ----------------
// Builds a varptr instruction to either static or local
// variable based on bridge_var_t
ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, bridge_var_t *var);

// ---------------- build_lvarptr ----------------
// Builds a local varptr instruction
ir_value_t *build_lvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_varptr ----------------
// Builds a global varptr instruction
ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_svarptr ----------------
// Builds a static varptr instruction
ir_value_t *build_svarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_malloc ----------------
// Builds a malloc instruction
ir_value_t *build_malloc(ir_builder_t *builder, ir_type_t *type, ir_value_t *amount, bool is_undef);

// ---------------- build_zeroinit ----------------
// Builds a zero initialization instruction
void build_zeroinit(ir_builder_t *builder, ir_value_t *destination);

// ---------------- build_memcpy ----------------
// Builds a memcpy instruction
void build_memcpy(ir_builder_t *builder, ir_value_t *destination, ir_value_t *value, ir_value_t *num_bytes, bool is_volatile);

// ---------------- build_load ----------------
// Builds a load instruction
ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source);

// ---------------- build_store ----------------
// Builds a store instruction
ir_instr_store_t *build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source);

// ---------------- build_call ----------------
// Builds a call instruction
ir_value_t *build_call(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length, source_t source);

// ---------------- build_call_ignore_result ----------------
// Builds a call instruction, but doesn't give back a reference to the result
void build_call_ignore_result(ir_builder_t *builder, func_id_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length, source_t code_source);

// ---------------- build_call_address ----------------
// Builds a call function address instruction
ir_value_t *build_call_address(ir_builder_t *builder, ir_type_t *return_type, ir_value_t *address, ir_value_t **arg_values, length_t arity);

// ---------------- build_break ----------------
// Builds a break instruction
void build_break(ir_builder_t *builder, length_t basicblock_id);

// ---------------- build_cond_break ----------------
// Builds a conditional break instruction
void build_cond_break(ir_builder_t *builder, ir_value_t *condition, length_t true_block_id, length_t false_block_id);

// ---------------- build_equals ----------------
// Builds an equals instruction
ir_value_t *build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b);

// ---------------- build_array_access ----------------
// Builds an array access instruction
ir_value_t *build_array_access(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, source_t code_source);
ir_value_t *build_array_access_ex(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, ir_type_t *result_type, source_t code_source);

// ---------------- build_member ----------------
// Builds a member access instruction
ir_value_t *build_member(ir_builder_t *builder, ir_value_t *value, length_t member, ir_type_t *result_type, source_t code_source);

// ---------------- build_return ----------------
// Builds a return instruction
void build_return(ir_builder_t *builder, ir_value_t *value);

// ---------------- build_static_struct ----------------
// Builds a static struct
ir_value_t *build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable);

// ---------------- build_struct_construction ----------------
// Builds a struct construction
ir_value_t *build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length);

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

// ---------------- build_static_array ----------------
// Builds a static array
ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *item_type, ir_value_t **values, length_t length);

// ---------------- build_anon_global ----------------
// Builds an anonymous global variable
ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant);

#endif // _ISAAC_IR_BUILD_H
