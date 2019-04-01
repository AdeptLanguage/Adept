
#ifndef IR_GEN_STMT_H
#define IR_GEN_STMT_H

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

// ---------------- ir_gen_func_statements ----------------
// Generates the required intermediate representation for
// statements inside an AST function. Internally it
// creates an 'ir_builder_t' and calls 'ir_gen_statements'
errorcode_t ir_gen_func_statements(compiler_t *compiler, object_t *object, length_t ast_func_id, length_t ir_func_id, ir_jobs_t *jobs);

// ---------------- ir_gen_statements ----------------
// Generates intermediate representation for
// statements given an existing 'ir_builder_t'
errorcode_t ir_gen_statements(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length, bool *out_is_terminated);

#endif // IR_GEN_STMT_H
