
#ifndef _ISAAC_IR_BUILD_INSTR_H
#define _ISAAC_IR_BUILD_INSTR_H

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

// ---------------- build_cast ----------------
// Casts an IR value to an IR type
// NOTE: const_cast_value_type is a valid VALUE_TYPE_* cast value
// NOTE: nonconst_cast_instr_type is a valid INSTRUCTION_* cast value
ir_value_t *build_cast(ir_builder_t *builder, unsigned int const_cast_value_type, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t *to);

// ---------------- build_nonconst_cast ----------------
// Builds a non-constant cast instruction and returns
// the casted result as an ir_value_t*
// NOTE: nonconst_cast_instr_type is a valid INSTRUCTION_* cast value
ir_value_t *build_nonconst_cast(ir_builder_t *builder, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t* to);

// ---------------- build_<cast> ----------------
// Builds a specialized value cast
#define build_bitcast(builder, from, to)     build_cast(builder, VALUE_TYPE_CONST_BITCAST, INSTRUCTION_BITCAST, from, to)
#define build_zext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_ZEXT, INSTRUCTION_ZEXT, from, to)
#define build_sext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_SEXT, INSTRUCTION_SEXT, from, to)
#define build_trunc(builder, from, to)       build_cast(builder, VALUE_TYPE_CONST_TRUNC, INSTRUCTION_TRUNC, from, to)
#define build_fext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_FEXT, INSTRUCTION_FEXT, from, to)
#define build_ftrunc(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FTRUNC, INSTRUCTION_FTRUNC, from, to)
#define build_inttoptr(builder, from, to)    build_cast(builder, VALUE_TYPE_CONST_INTTOPTR, INSTRUCTION_INTTOPTR, from, to)
#define build_ptrtoint(builder, from, to)    build_cast(builder, VALUE_TYPE_CONST_PTRTOINT, INSTRUCTION_PTRTOINT, from, to)
#define build_fptoui(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FPTOUI, INSTRUCTION_FPTOUI, from, to)
#define build_fptosi(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FPTOSI, INSTRUCTION_FPTOSI, from, to)
#define build_uitofp(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_UITOFP, INSTRUCTION_UITOFP, from, to)
#define build_sitofp(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_SITOFP, INSTRUCTION_SITOFP, from, to)
#define build_reinterpret(builder, from, to) build_cast(builder, VALUE_TYPE_CONST_REINTERPRET, INSTRUCTION_REINTERPRET, from, to)

// ---------------- build_alloc ----------------
// Allocates space on the stack for a variable of a type
// NOTE: This is only used for dynamic allocations w/ STACK_SAVE and STACK_RESTORE
ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type);
ir_value_t *build_alloc_array(ir_builder_t *builder, ir_type_t *type, ir_value_t *count);
ir_value_t *build_alloc_aligned(ir_builder_t *builder, ir_type_t *type, unsigned int alignment);

// ---------------- build_stack_restore ----------------
// Saves the current position of the stack by returning the stack pointer
ir_value_t *build_stack_save(ir_builder_t *builder);

// ---------------- build_stack_restore ----------------
// Restores the stack to a previous position
void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer);

// ---------------- build_math ----------------
// Builds a basic math instruction
ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result);

// ---------------- build_phi2 ----------------
// Builds a PHI2 instruction
ir_value_t *build_phi2(ir_builder_t *builder, ir_type_t *result_type, ir_value_t *a, ir_value_t *b, length_t landing_a_block_id, length_t landing_b_block_id);

// ---------------- build_llvm_asm ----------------
// Builds an inline assembly instruction
void build_llvm_asm(ir_builder_t *builder, bool is_intel, weak_cstr_t assembly, weak_cstr_t constraints, ir_value_t **args, length_t arity, bool has_side_effects, bool is_stack_align);

// ---------------- build_deinit_svars ----------------
// Builds an instruction that will handle the deinitialization
// of all static variables
// NOTE: Most of the time, 'build_global_cleanup'
// should be used instead of this function, since
// it covers all main-related deinitialization routines
void build_deinit_svars(ir_builder_t *builder);

// ---------------- build_unreachable ----------------
// Builds an instruction that indicates an unreachable code path
void build_unreachable(ir_builder_t *builder);

// ---------------- build_global_cleanup ----------------
// Builds all main-related deinitialization routines
void build_global_cleanup(ir_builder_t *builder);

#endif // _ISAAC_IR_BUILD_INSTR_H
