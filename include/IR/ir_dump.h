
#ifndef _ISAAC_IR_DUMP_H
#define _ISAAC_IR_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    =================================== ir_dump.h ===================================
    Module for dumping intermediate representation
    ----------------------------------------------------------------------------
*/

#include <stdio.h>

#include "IR/ir.h"
#include "IR/ir_module.h"
#include "UTIL/ground.h"

// ---------------- ir_module_dump ----------------
// Generates a string representation from an IR
// module and writes it to a file
void ir_module_dump(ir_module_t *ir_module, const char *filename);

// ---------------- ir_dump_functions (and friends) ----------------
// Dumps a specific part of an IR module
void ir_dump_function(FILE *file, ir_func_t *function, func_id_t ir_func_id, ir_func_t *all_funcs);
void ir_dump_functions(FILE *file, ir_funcs_t *funcs);
void ir_dump_basicsblocks(FILE *file, ir_basicblocks_t basicblocks, ir_func_t *functions);
void ir_dump_instruction(FILE *file, ir_instr_t *instruction, length_t instr_index, ir_func_t *all_funcs);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_DUMP_H
