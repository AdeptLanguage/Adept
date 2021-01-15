
#ifndef _ISAAC_IR_GEN_STMT_H
#define _ISAAC_IR_GEN_STMT_H

/*
    ============================== ir_gen_stmt.h ==============================
    Module for generating intermediate-representation from abstract
    syntax tree statements. (Doesn't include expressions)
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_stmts ----------------
// Generates intermediate representation for
// statements given an existing 'ir_builder_t'
errorcode_t ir_gen_stmts(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length, bool *out_is_terminated);

// ---------------- ir_gen_stmt_return ----------------
// Generates IR instructions for a return statement
errorcode_t ir_gen_stmt_return(ir_builder_t *builder, ast_expr_return_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_call_like ----------------
// Generates IR instructions for a call-like statement
errorcode_t ir_gen_stmt_call_like(ir_builder_t *builder, ast_expr_t *call_like_stmt);

// ---------------- ir_gen_stmt_declare ----------------
// Generates IR instructions for a variable declaration statement
errorcode_t ir_gen_stmt_declare(ir_builder_t *builder, ast_expr_declare_t *stmt);

// ---------------- exhaustive_switch_check ----------------
// Performs exhaustive switch checking for switch values
errorcode_t exhaustive_switch_check(ir_builder_t *builder, weak_cstr_t enum_name, source_t switch_source, unsigned long long uniqueness_values[], length_t uniqueness_values_length);

#endif // _ISAAC_IR_GEN_STMT_H
