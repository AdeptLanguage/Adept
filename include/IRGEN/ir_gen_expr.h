
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

// ---------------- ir_gen_conforming_expr ----------------
// Generates instructions for an AST expression to produce an IR value.
// The resulting value will then try to be conformed to the expected AST type given.
// NOTE: 'error_format' expects two '%s' format specifiers
// NOTE: Returns NULL on failure
ir_value_t *ir_gen_conforming_expr(ir_builder_t *builder, ast_expr_t *ast_value, ast_type_t *to_type, unsigned int conform_mode, source_t source, const char *error_format);

// ---------------- ir_gen_expr_* ----------------
// Genarates an IR value for a specific kind of AST expression
errorcode_t ir_gen_expr_and(ir_builder_t *builder, ast_expr_and_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_or(ir_builder_t *builder, ast_expr_or_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_str(ir_builder_t *builder, ast_expr_str_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_cstr(ir_builder_t *builder, ast_expr_cstr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_variable(ir_builder_t *builder, ast_expr_variable_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_call(ir_builder_t *builder, ast_expr_call_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_member(ir_builder_t *builder, ast_expr_member_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_address(ir_builder_t *builder, ast_expr_address_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_va_arg(ir_builder_t *builder, ast_expr_va_arg_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_initlist(ir_builder_t *builder, ast_expr_initlist_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_func_addr(ir_builder_t *builder, ast_expr_func_addr_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_dereference(ir_builder_t *builder, ast_expr_dereference_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_array_access(ir_builder_t *builder, ast_expr_array_access_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_cast(ir_builder_t *builder, ast_expr_cast_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_sizeof(ir_builder_t *builder, ast_expr_sizeof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_sizeof_value(ir_builder_t *builder, ast_expr_sizeof_value_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_phantom(ast_expr_phantom_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_call_method(ir_builder_t *builder, ast_expr_call_method_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_unary(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_new(ir_builder_t *builder, ast_expr_new_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_new_cstring(ir_builder_t *builder, ast_expr_new_cstring_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_enum_value(ir_builder_t *builder, ast_expr_enum_value_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_static_array(ir_builder_t *builder, ast_expr_static_data_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_static_struct(ir_builder_t *builder, ast_expr_static_data_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_typeinfo(ir_builder_t *builder, ast_expr_typeinfo_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_ternary(ir_builder_t *builder, ast_expr_ternary_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_crement(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_toggle(ir_builder_t *builder, ast_expr_unary_t *expr, ir_value_t **ir_value, bool leave_mutable, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_inline_declare(ir_builder_t *builder, ast_expr_inline_declare_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_typenameof(ir_builder_t *builder, ast_expr_typenameof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_embed(ir_builder_t *builder, ast_expr_embed_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);
errorcode_t ir_gen_expr_alignof(ir_builder_t *builder, ast_expr_alignof_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);

// ---------------- ir_gen_expr_* helper functions ----------------
// Functions that assist the ir_gen_expr_* functions

// Generates math for operations that either work on integers or floats
// If the instruction ID of the chosen instruction is INSTRUCTION_NONE,
// then FAILURE will be returned
// NOTE: Returns FAILURE on failure
errorcode_t ir_gen_expr_math_ivf(ir_builder_t *builder, ast_expr_math_t *expr, unsigned int ints_instr, unsigned int floats_instr,
        ir_value_t **ir_value, ast_type_t *out_expr_type);

// Generates math for operations that either work on unsigned integers, signed integers, or floats
// If the instruction ID of the chosen instruction is INSTRUCTION_NONE,
// then FAILURE will be returned
// NOTE: Returns FAILURE on failure
errorcode_t ir_gen_expr_math_uvsvf(ir_builder_t *builder, ast_expr_math_t *expr, unsigned int uints_instr, unsigned int sints_instr,
        unsigned int floats_instr, ir_value_t **ir_value, ast_type_t *out_expr_type);

// Generates the setup for either an 'AND' or an 'OR' expression
errorcode_t ir_gen_expr_pre_andor(ir_builder_t *builder, ast_expr_math_t *andor_expr, ir_value_t **a, ir_value_t **b,
        length_t *landing_a_block_id, length_t *landing_b_block_id, length_t *landing_more_block_id, ast_type_t *out_expr_type);

// ---------------- ir_gen_expr_func_addr_noop_result_for_defer ----------------
// Generates a no-op function value for func &__defer__(*Type) functions
// that don't exist
// (Used for backward compatibility)
errorcode_t ir_gen_expr_func_addr_noop_result_for_defer(ir_builder_t *builder, ast_type_t *match_arg, source_t source_on_error, ir_value_t **ir_value, ast_type_t *out_expr_type);

// ---------------- ir_field_info_t ----------------
// In-depth information about a field of a composite
// NOTE: This structure has ownership over 'ast_type' must be freed
typedef struct {
    ast_type_t ast_type; // (owned)
    ast_layout_endpoint_t endpoint;
    ast_layout_endpoint_path_t path;
    ir_type_t *ir_type;
} ir_field_info_t;

// ---------------- ir_gen_get_field_info ----------------
// Gets info about a field of a compsite
errorcode_t ir_gen_get_field_info(compiler_t *compiler, object_t *object, weak_cstr_t member, source_t source, ast_type_t *ast_type_of_composite, ir_field_info_t *out_field_info);

// ---------------- ir_gen_arguments ----------------
// Generates expressions for call arguments
errorcode_t ir_gen_arguments(ir_builder_t *builder, ast_expr_t **args, length_t arity, ir_value_t ***out_arg_values, ast_type_t **out_arg_types);

// ---------------- ir_gen_expr_call_method_find_appropriate_method ----------------
// Finds the appropriate method for a 'CALL METHOD' expression to call
errorcode_t ir_gen_expr_call_method_find_appropriate_method(ir_builder_t *builder, ast_expr_call_method_t *expr, ir_value_t ***arg_values,
        ast_type_t **arg_types, length_t *arg_arity, ast_type_t *gives, optional_funcpair_t *result);

// ---------------- ir_gen_expr_call_procedure_handle_pass_management ----------------
// Calls the pass management function on arguments for 'CALL'/'CALL METHOD' expression
errorcode_t ir_gen_expr_call_procedure_handle_pass_management(ir_builder_t *builder, length_t arity, ir_value_t **arg_values, ast_type_t *final_arg_types,
        trait_t target_traits, trait_t *target_arg_type_traits, length_t arity_without_variadic_arguments);

// ---------------- ir_gen_expr_call_procedure_handle_variadic_packing ----------------
// Packs variadic arguments into single variadic array argument for 'CALL'/'CALL METHOD' expression if required
errorcode_t ir_gen_expr_call_procedure_handle_variadic_packing(ir_builder_t *builder, ir_value_t ***arg_values, ast_type_t *arg_types,
        length_t *arity, funcpair_t *pair, ir_value_t **stack_pointer, source_t source_on_failure);

// ---------------- ir_gen_math_operands ----------------
// ir_gens both expression operands of a math expression
// and returns a new IR math instruction with an undetermined
// instruction id (for caller to determine afterwards).
ir_instr_math_t* ir_gen_math_operands(ir_builder_t *builder, ast_expr_math_t *expr, ir_value_t **ir_value, ast_type_t *out_expr_type);

// ---------------- ir_gen_call_function_value ----------------
// Generates instructions for calling a value that's a function pointer
errorcode_t ir_gen_call_function_value(ir_builder_t *builder, ast_type_t *ast_var_type,
        ast_expr_call_t *call, ir_value_t **arg_values, ast_type_t *arg_types,
        ir_value_t **inout_ir_value, ast_type_t *out_expr_type);

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
        unsigned int instr1, unsigned int instr2, unsigned int instr3, const char *op_verb, weak_cstr_t overload, bool result_is_boolean);

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

// Primitive category indicators returned by 'ir_type_get_category'
#define PRIMITIVE_NA 0x00 // N/A
#define PRIMITIVE_SI 0x01 // Signed Integer
#define PRIMITIVE_UI 0x02 // Unsigned Integer
#define PRIMITIVE_FP 0x03 // FLoating Point Value

// ---------------- ir_type_get_category ----------------
// Returns a general category for an IR type.
// (either signed, unsigned, or float)
char ir_type_get_category(ir_type_t *type);

#endif // _ISAAC_IR_GEN_EXPR_H
