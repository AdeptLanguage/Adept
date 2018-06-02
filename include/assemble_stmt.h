
#ifndef ASSEMBLE_STMT_H
#define ASSEMBLE_STMT_H

/*
    ============================= assemble_stmt.h =============================
    Module for generating intermediate-representation from abstract
    syntax tree statements. (Doesn't include expressions)
    ---------------------------------------------------------------------------
*/

#include "ir.h"
#include "ast.h"
#include "ground.h"
#include "assemble.h"
#include "compiler.h"
#include "irbuilder.h"

// ---------------- assemble_func_statements ----------------
// Generates the required intermediate representation for
// statements inside an AST function. Internally it
// creates an 'ir_builder_t' and calls 'assemble_statements'
int assemble_func_statements(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ir_func_t *module_func);

// ---------------- assemble_statements ----------------
// Generates intermediate representation for
// statements given an existing 'ir_builder_t'
int assemble_statements(ir_builder_t *builder, ast_expr_t **statements, length_t statements_length);

#endif // ASSEMBLE_STMT_H
