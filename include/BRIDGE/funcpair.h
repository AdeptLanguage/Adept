
#ifndef FUNCPAIR_H
#define FUNCPAIR_H

#include "AST/ast.h"
#include "IR/ir.h"

// ---------------- funcpair_t ----------------
// A container for function results
typedef struct {
    ast_func_t *ast_func;
    ir_func_t *ir_func;
    length_t ast_func_id;
    length_t ir_func_id;
} funcpair_t;

#endif // FUNCPAIR_H
