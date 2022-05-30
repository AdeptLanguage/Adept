
#ifndef _ISAAC_FUNCPAIR_H
#define _ISAAC_FUNCPAIR_H

#include <stdbool.h>

#include "AST/ast.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "UTIL/ground.h"

// ---------------- funcpair_t ----------------
// A container for function results
typedef struct {
    ast_func_t *ast_func;
    ir_func_t *ir_func;
    func_id_t ast_func_id;
    func_id_t ir_func_id;
} funcpair_t;

typedef optional(funcpair_t) optional_funcpair_t;

void optional_funcpair_set(optional_funcpair_t *result, bool has, func_id_t ast_func_id, func_id_t ir_func_id, object_t *object);

#endif // _ISAAC_FUNCPAIR_H
