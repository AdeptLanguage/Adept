
#ifndef _ISAAC_IR_GEN_QUALIFIERS_H
#define _ISAAC_IR_GEN_QUALIFIERS_H

/*
    =========================== ir_gen_qualifiers.h ============================
    Module for ensuring function qualifiers are not violated
    ----------------------------------------------------------------------------
*/

#include "AST/ast.h"
#include "DRVR/compiler.h"

errorcode_t ensure_not_violating_no_discard(compiler_t *compiler, bool no_discard_active, source_t call_source, ast_func_t *callee);

errorcode_t ensure_not_violating_disallow(compiler_t *compiler, source_t call_source, ast_func_t *callee);

#endif // _ISAAC_IR_GEN_QUALIFIERS_H
