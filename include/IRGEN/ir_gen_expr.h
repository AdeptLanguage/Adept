
#ifndef IR_GEN_EXPR_H
#define IR_GEN_EXPR_H

/*
    ============================== ir_gen_expr.h ==============================
    Module for generating intermediate-representation from an abstract
    syntax tree expression. (Doesn't include statements)
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_builder.h"

// ---------------- ir_gen_expression ----------------
// ir_gens an AST expression into an IR value and
// stores the resulting AST type in 'out_expr_type'
// if 'out_expr_type' isn't NULL.
// If 'leave_mutable' is true and the resulting expression
// is mutable, the result won't automatically be dereferenced.
int ir_gen_expression(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);

// ---------------- ir_gen_math_operands ----------------
// ir_gens both expression operands of a math expression
// and returns a new IR math instruction with an undetermined
// instruction id (for caller to determine afterwards).
ir_instr_t* ir_gen_math_operands(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, unsigned int op_res, ast_type_t *out_expr_type);

// Operation result auto-type-casting modes for 'ir_gen_math_operands'
#define MATH_OP_RESULT_MATCH 0x01
#define MATH_OP_RESULT_BOOL  0x02
#define MATH_OP_ALL_BOOL     0x03

// ---------------- i_vs_f_instruction ----------------
// If the math instruction given will have it's
// instruction id changed based on what it operates on.
// 'int_instr' if it operates on integers.
// 'float_instr' if it operates on floating point values.
int i_vs_f_instruction(ir_instr_math_t *instruction, unsigned int int_instr, unsigned int float_instr);

// ---------------- u_vs_s_vs_float_instruction ----------------
// If the math instruction given will have it's
// instruction id changed based on what it operates on.
// 'u_instr' if it operates on unsigned integers.
// 's_instr' if it operates on signed integers.
// 'float_instr' if it operates on floating point values.
int u_vs_s_vs_float_instruction(ir_instr_math_t *instruction, unsigned int u_instr, unsigned int s_instr, unsigned int f_instr);

// Primitive catagory indicators returned by 'ir_type_get_catagory'
#define PRIMITIVE_NA 0x00 // N/A
#define PRIMITIVE_SI 0x01 // Signed Integer
#define PRIMITIVE_UI 0x02 // Unsigned Integer
#define PRIMITIVE_FP 0x03 // FLoating Point Value

// ---------------- ir_type_get_catagory ----------------
// Returns a general catagory for an IR type.
// (either signed, unsigned, or float)
char ir_type_get_catagory(ir_type_t *type);


#endif // IR_GEN_EXPR_H
