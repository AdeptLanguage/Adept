
#ifndef _ISAAC_IR_GEN_EXPR_H
#define _ISAAC_IR_GEN_EXPR_H

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

// ---------------- ir_gen_expr ----------------
// ir_gens an AST expression into an IR value and
// stores the resulting AST type in 'out_expr_type'
// if 'out_expr_type' isn't NULL.
// If 'leave_mutable' is true and the resulting expression
// is mutable, the result won't automatically be dereferenced.
errorcode_t ir_gen_expr(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);

// ---------------- ir_gen_expr_* ----------------
// Genarates an IR value for a specific kind of AST expression
errorcode_t ir_gen_expr_and(ir_builder_t *builder, ast_expr_and_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_or(ir_builder_t *builder, ast_expr_or_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_str(ir_builder_t *builder, ast_expr_str_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_cstr(ir_builder_t *builder, ast_expr_cstr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_variable(ir_builder_t *builder, ast_expr_variable_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_call(ir_builder_t *builder, ast_expr_call_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_member(ir_builder_t *builder, ast_expr_member_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_address(ir_builder_t *builder, ast_expr_address_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_va_arg(ir_builder_t *builder, ast_expr_va_arg_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_func_addr(ir_builder_t *builder, ast_expr_func_addr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);

// ---------------- ir_gen_expr_* helper functions ----------------
// Functions that assist the ir_gen_expr_* functions
errorcode_t ir_gen_expr_pre_andor(ir_builder_t *builder, ast_expr_math_t *andor_expr, ir_value_t **a, ir_value_t **b,
        length_t *landing_a_block_id, length_t *landing_b_block_id, length_t *landing_more_block_id, ast_type_t *out_expr_type);

errorcode_t ir_gen_expr_member_get_field_info(ir_builder_t *builder, ast_expr_member_t *expr, ast_elem_t *elem, ast_type_t *struct_value_ast_type,
        length_t *field_index, ir_type_t **field_type, ast_type_t *out_expr_type);
// ----------------------------------------------------------------

// ---------------- ir_gen_math_operands ----------------
// ir_gens both expression operands of a math expression
// and returns a new IR math instruction with an undetermined
// instruction id (for caller to determine afterwards).
ir_instr_t* ir_gen_math_operands(ir_builder_t *builder, ast_expr_t *expr, ir_value_t **ir_value, unsigned int op_res, ast_type_t *out_expr_type);

// ---------------- ir_gen_call_function_value ----------------
// Generates instructions for calling a value that's a function pointer
errorcode_t ir_gen_call_function_value(ir_builder_t *builder, ast_type_t *ast_var_type,
        ir_type_t *ir_var_type, ast_expr_call_t *call, ir_value_t **arg_values, ast_type_t *arg_types,
        ir_value_t **inout_ir_value, ast_type_t *out_expr_type);

// Operation result auto-type-casting modes for 'ir_gen_math_operands'
#define MATH_OP_RESULT_MATCH 0x01
#define MATH_OP_RESULT_BOOL  0x02
#define MATH_OP_ALL_BOOL     0x03

// ---------------- ir_gen_expr_math ----------------
// Differentiates an operation for different data types
// When given 2 instruction ids: (instr3 == INSTRUCTION_NONE)
//     - instr1 is chosen for integers
//     - instr2 is chosen for floats
// When given 3 instruction ids:
//     - instr1 is chosen for unsigned integers
//     - instr2 is chosen for signed integers
//     - instr3 is chosen for floats
errorcode_t ir_gen_expr_math(ir_builder_t *builder, ast_expr_math_t *math_expr, ir_value_t **ir_value, ast_type_t *out_expr_type,
        unsigned int instr1, unsigned int instr2, unsigned int instr3, const char *op_verb, const char *overload, bool result_is_boolean);

// ---------------- i_vs_f_instruction ----------------
// Attempts to resolve conflict between two possible result types
// of a ternary expression
successful_t ir_gen_resolve_ternay_conflict(ir_builder_t *builder, ir_value_t **a, ir_value_t **b, ast_type_t *a_type, ast_type_t *b_type,
        length_t *inout_a_basicblock, length_t *inout_b_basicblock);

// ---------------- i_vs_f_instruction ----------------
// If the math instruction given will have it's
// instruction id changed based on what it operates on.
// 'int_instr' if it operates on integers.
// 'float_instr' if it operates on floating point values.
errorcode_t i_vs_f_instruction(ir_instr_math_t *instruction, unsigned int int_instr, unsigned int float_instr);

// ---------------- u_vs_s_vs_float_instruction ----------------
// If the math instruction given will have it's
// instruction id changed based on what it operates on.
// 'u_instr' if it operates on unsigned integers.
// 's_instr' if it operates on signed integers.
// 'float_instr' if it operates on floating point values.
errorcode_t u_vs_s_vs_float_instruction(ir_instr_math_t *instruction, unsigned int u_instr, unsigned int s_instr, unsigned int f_instr);

// Primitive catagory indicators returned by 'ir_type_get_catagory'
#define PRIMITIVE_NA 0x00 // N/A
#define PRIMITIVE_SI 0x01 // Signed Integer
#define PRIMITIVE_UI 0x02 // Unsigned Integer
#define PRIMITIVE_FP 0x03 // FLoating Point Value

// ---------------- ir_type_get_catagory ----------------
// Returns a general catagory for an IR type.
// (either signed, unsigned, or float)
char ir_type_get_catagory(ir_type_t *type);

#endif // _ISAAC_IR_GEN_EXPR_H
