
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
errorcode_t ir_gen_stmts(ir_builder_t *builder, ast_expr_list_t *stmt_list, bool *out_is_terminated);

// ---------------- ir_gen_stmt_return ----------------
// Generates IR instructions for a return statement
errorcode_t ir_gen_stmt_return(ir_builder_t *builder, ast_expr_return_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_call_like ----------------
// Generates IR instructions for a call-like statement
errorcode_t ir_gen_stmt_call_like(ir_builder_t *builder, ast_expr_t *call_like_stmt);

// ---------------- ir_gen_stmt_declare ----------------
// Generates IR instructions for a variable declaration statement
errorcode_t ir_gen_stmt_declare(ir_builder_t *builder, ast_expr_declare_t *stmt);

// ---------------- ir_gen_do_construct ----------------
// Generates a .__constructor__() call to construct a value
errorcode_t ir_gen_do_construct(
    ir_builder_t *builder,
    ir_value_t *variable,
    const ast_type_t *ast_type,
    ast_expr_list_t *inputs,
    source_t source
);

// ---------------- ir_gen_stmt_declare_try_init ----------------
// Tries to initialize a variable that requires initialization
// that was recently declared by a declare statement.
// NOTE: Called from 'ir_gen_stmt_declare'
errorcode_t ir_gen_stmt_declare_try_init(ir_builder_t *primary_builder, ast_expr_declare_t *stmt, ir_type_t *ir_type);

// ---------------- ir_gen_stmt_assignment_like ----------------
// Generates IR instructions for an assignment-like statement
errorcode_t ir_gen_stmt_assignment_like(ir_builder_t *builder, ast_expr_assign_t *stmt);

// ---------------- ir_gen_stmt_simple_conditional ----------------
// Generates IR instructions for an 'if'/'unless' conditional statement
errorcode_t ir_gen_stmt_simple_conditional(ir_builder_t *builder, ast_expr_if_t *stmt);

// ---------------- ir_gen_stmt_dual_conditional ----------------
// Generates IR instructions for an 'if-else'/'unless-else' conditional statement
errorcode_t ir_gen_stmt_dual_conditional(ir_builder_t *builder, ast_expr_ifelse_t *stmt);

// ---------------- ir_gen_stmt_simple_loop ----------------
// Generates IR instructions for an 'while'/'until' conditional statement
errorcode_t ir_gen_stmt_simple_loop(ir_builder_t *builder, ast_expr_while_t *stmt);

// ---------------- ir_gen_stmt_recurrent_loop ----------------
// Generates IR instructions for an 'while-continue'/'until-break' conditional statement
errorcode_t ir_gen_stmt_recurrent_loop(ir_builder_t *builder, ast_expr_whilecontinue_t *expr);

// ---------------- ir_gen_stmt_delete ----------------
// Generates IR instructions for a 'delete' statement
errorcode_t ir_gen_stmt_delete(ir_builder_t *builder, ast_expr_delete_t *stmt);

// ---------------- ir_gen_stmt_break ----------------
// Generates IR instructions for a 'break' statement
errorcode_t ir_gen_stmt_break(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_continue ----------------
// Generates IR instructions for a 'continue' statement
errorcode_t ir_gen_stmt_continue(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_fallthrough ----------------
// Generates IR instructions for a 'fallthrough' statement
errorcode_t ir_gen_stmt_fallthrough(ir_builder_t *builder, ast_expr_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_break_to ----------------
// Generates IR instructions for a 'break-to' statement
errorcode_t ir_gen_stmt_break_to(ir_builder_t *builder, ast_expr_break_to_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_continue_to ----------------
// Generates IR instructions for a 'continue-to' statement
errorcode_t ir_gen_stmt_continue_to(ir_builder_t *builder, ast_expr_continue_to_t *stmt, bool *out_is_terminated);

// ---------------- ir_gen_stmt_each ----------------
// Generates IR instructions for an 'each-in' loop
errorcode_t ir_gen_stmt_each(ir_builder_t *builder, ast_expr_each_in_t *stmt);

// ---------------- ir_gen_stmt_repeat ----------------
// Generates IR instructions for an 'repeat' loop
errorcode_t ir_gen_stmt_repeat(ir_builder_t *builder, ast_expr_repeat_t *stmt);

// ---------------- exhaustive_switch_check ----------------
// Performs exhaustive switch checking for switch values
errorcode_t exhaustive_switch_check(ir_builder_t *builder, weak_cstr_t enum_name, source_t switch_source, unsigned long long uniqueness_values[], length_t uniqueness_values_length);

// ---------------- ir_gen_stmts_auto_terminate ----------------
// If not 'already_terminated', then this function will handle
// variable deference calls and breaking to the continuation block
errorcode_t ir_gen_stmts_auto_terminate(ir_builder_t *builder, bool already_terminated, length_t continuation_block_id);

// ---------------- ir_gen_variable_deference ----------------
// Generates IR instructions to make variable deference calls up until a scope.
// NOTE: If 'up_until_scope' is NULL, then all parent scopes will be visited.
errorcode_t ir_gen_variable_deference(ir_builder_t *builder, bridge_scope_t *up_until_scope);

// ---------------- ir_gen_perform_pod_assignment ----------------
// Generates IR instructions for a POD (plain-old-data) assignment
errorcode_t ir_gen_perform_pod_assignment(ir_builder_t *builder, ir_value_t **value, ast_type_t *value_ast_type,
        ir_value_t *destination, ast_type_t *destination_ast_type, source_t source_on_error);

#endif // _ISAAC_IR_GEN_STMT_H
