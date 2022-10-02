
#ifndef _ISAAC_AST_EXPR_H
#define _ISAAC_AST_EXPR_H

/*
    =============================== ast_expr.h ================================
    Module for manipulating abstract syntax tree expressions
    ---------------------------------------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "AST/EXPR/ast_expr_ids.h" // IWYU pragma: export
#include "AST/ast_expr_lean.h" // IWYU pragma: export
#include "AST/ast_named_expression.h"
#include "AST/ast_type_lean.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"

// ---------------- ast_expr_byte_t ----------------
// Expression for a literal byte value
typedef struct { DERIVE_AST_EXPR; adept_byte value; } ast_expr_byte_t;

// ---------------- ast_expr_ubyte_t ----------------
// Expression for a literal unsigned byte value
typedef struct { DERIVE_AST_EXPR; adept_ubyte value; } ast_expr_ubyte_t;

// ---------------- ast_expr_short_t ----------------
// Expression for a literal short value
typedef struct { DERIVE_AST_EXPR; adept_short value; } ast_expr_short_t;

// ---------------- ast_expr_ushort_t ----------------
// Expression for a literal unsigned short value
typedef struct { DERIVE_AST_EXPR; adept_ushort value; } ast_expr_ushort_t;

// ---------------- ast_expr_int_t ----------------
// Expression for a literal integer value
typedef struct { DERIVE_AST_EXPR; adept_int value; } ast_expr_int_t;

// ---------------- ast_expr_uint_t ----------------
// Expression for a literal unsigned integer value
typedef struct { DERIVE_AST_EXPR; adept_uint value; } ast_expr_uint_t;

// ---------------- ast_expr_long_t ----------------
// Expression for a literal long value
typedef struct { DERIVE_AST_EXPR; adept_long value; } ast_expr_long_t;

// ---------------- ast_expr_ulong_t ----------------
// Expression for a literal unsigned long value
typedef struct { DERIVE_AST_EXPR; adept_ulong value; } ast_expr_ulong_t;

// ---------------- ast_expr_usize_t ----------------
// Expression for a literal unsigned size value
typedef struct { DERIVE_AST_EXPR; adept_usize value; } ast_expr_usize_t;

// ---------------- ast_expr_float_t ----------------
// Expression for a literal 32-bit float value
typedef struct { DERIVE_AST_EXPR; adept_float value; } ast_expr_float_t;

// ---------------- ast_expr_double_t ----------------
// Expression for a literal 64-bit float value
typedef struct { DERIVE_AST_EXPR; adept_double value; } ast_expr_double_t;

// ---------------- ast_expr_boolean_t ----------------
// Expression for a literal boolean value
typedef struct { DERIVE_AST_EXPR; adept_bool value; } ast_expr_boolean_t;

// ---------------- ast_expr_str_t ----------------
// Expression for a literal length-string value
typedef struct { DERIVE_AST_EXPR; weak_cstr_t array; length_t length; } ast_expr_str_t;

// ---------------- ast_expr_cstr_t ----------------
// Expression for a literal null-terminated string value
typedef struct { DERIVE_AST_EXPR; weak_cstr_t value; } ast_expr_cstr_t;

// ---------------- ast_expr_null_t ----------------
// Expression for 'null' value
typedef ast_expr_t ast_expr_null_t;

// ---------------- ast_expr_generic_int_t ----------------
// Expression for a generic integer value of an undetermined type
typedef struct { DERIVE_AST_EXPR; adept_generic_int value; } ast_expr_generic_int_t;

// ---------------- ast_expr_generic_float_t ----------------
// Expression for a generic float value of an undetermined type
typedef struct { DERIVE_AST_EXPR; adept_generic_float value; } ast_expr_generic_float_t;

// ---------------- ast_expr_math_t ----------------
// General purpose two-operand expression
// Used for: add, subtract, multiply, divide, modulus, power, equals,
//           notequals, greater, lesser, greatereq, lessereq, and, or
typedef struct { DERIVE_AST_EXPR; ast_expr_t *a, *b; } ast_expr_math_t, ast_expr_and_t, ast_expr_or_t;

// ---------------- ast_expr_unary_t ----------------
// General purpose single-operand expression
// Used for: address, dereference, bit complement, not, negate, delete, toggle
typedef struct { DERIVE_AST_EXPR; ast_expr_t *value; } ast_expr_unary_t,
    // Aliases
    ast_expr_address_t, ast_expr_dereference_t, ast_expr_bitwise_complement_t, ast_expr_not_t,
    ast_expr_negate_t, ast_expr_delete_t, ast_expr_toggle_t, ast_expr_va_start_t, ast_expr_va_end_t, ast_expr_sizeof_value_t;

// ---------------- ast_expr_unary_type_t ----------------
// Generic expression that only operates on a type
typedef struct {
    DERIVE_AST_EXPR;
    ast_type_t type;
} ast_expr_unary_type_t;

// ---------------- ast_expr_new_t ----------------
// Expression for 'new' keyword, used to dynamically
// allocate memory on the heap
typedef struct {
    DERIVE_AST_EXPR;
    ast_type_t type;
    ast_expr_t *amount; // Can be NULL to indicate single element
    bool is_undef;
    optional_ast_expr_list_t inputs;
} ast_expr_new_t;

// ---------------- ast_expr_new_cstring_t ----------------
// Dynamically allocates a null-terminated string on the heap
typedef struct { DERIVE_AST_EXPR; weak_cstr_t value; } ast_expr_new_cstring_t;

// ---------------- ast_expr_enum_value_t ----------------
// Gets the constant value of an enum kind
typedef struct { DERIVE_AST_EXPR; weak_cstr_t enum_name; weak_cstr_t kind_name; } ast_expr_enum_value_t;

// ---------------- ast_expr_static_data_t ----------------
// Represents a static array value
// (Used for EXPR_STATIC_ARRAY and EXPR_STATIC_STRUCT)
typedef struct {
    DERIVE_AST_EXPR;
    ast_type_t type;
    ast_expr_t **values;
    length_t length;
} ast_expr_static_data_t;

// ---------------- ast_expr_typeinfo_t ----------------
// Expression for getting runtime type information for a type
typedef ast_expr_unary_type_t ast_expr_typeinfo_t;

// ---------------- ast_expr_ternary_t ----------------
// Expression for conditionally selecting between two expressions
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *condition;
    ast_expr_t *if_true;
    ast_expr_t *if_false;
} ast_expr_ternary_t;

// ---------------- ast_expr_call_t ----------------
// Expression for calling a function
typedef struct {
    DERIVE_AST_EXPR;
    strong_cstr_t name;
    ast_expr_t **args;
    length_t arity;

    // Optional return type matching, only
    // exists if 'gives.elements_length' isn't zero
    ast_type_t gives;

    // Ignore this call if it can't be compiled
    bool is_tentative : 1;

    // Make this call fail if it's tentative and
    // the target function isn't marked as implicit
    // (used for internal implicit calls)
    bool only_implicit : 1;

    // Don't allow for implicit user casts
    bool no_user_casts : 1;

    // Throw an error when trying to use functions marked as "no discard"
    bool no_discard : 1;
} ast_expr_call_t;

// ---------------- ast_expr_variable_t ----------------
// Expression for accessing a variable
typedef struct { DERIVE_AST_EXPR; weak_cstr_t name; } ast_expr_variable_t;

// ---------------- ast_expr_member_t ----------------
// Expression for accessing a member of a value
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *value;
    strong_cstr_t member;
} ast_expr_member_t;

// ---------------- ast_expr_func_addr_t ----------------
// Expression for retrieving a function address
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t name;
    ast_type_t *match_args;
    length_t match_args_length;
    trait_t traits;

    // Yield 'null' instead of throwing an error
    // if the target function couldn't be found
    bool tentative : 1;

    // Indicates whether name-only or not
    // (since just because match_args == NULL doesn't mean name-only)
    bool has_match_args : 1; 
} ast_expr_func_addr_t;

// ---------------- ast_expr_array_access_t ----------------
// Expression for accessing an element in an array
// (Used for: EXPR_ARRAY_ACCESS and EXPR_AT)
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *value;
    ast_expr_t *index;
} ast_expr_array_access_t;

// ---------------- ast_expr_cast_t ----------------
// Expression for casting a value to a specific type
typedef struct {
    DERIVE_AST_EXPR;
    ast_type_t to;
    ast_expr_t *from;
} ast_expr_cast_t;

// ---------------- ast_expr_sizeof_t ----------------
// Expression for getting the sizeof a type
typedef ast_expr_unary_type_t ast_expr_sizeof_t;

// ---------------- ast_expr_alignof_t ----------------
// Expression for getting the alignment a type
typedef ast_expr_unary_type_t ast_expr_alignof_t;

// ---------------- ast_expr_phantom_t ----------------
// Expression for passing precomputed ir_value_t values into AST expressions
typedef struct {
    DERIVE_AST_EXPR;
    ast_type_t type;
    void *ir_value;
    bool is_mutable;
} ast_expr_phantom_t;

// ---------------- ast_expr_call_method_t ----------------
// Expression for calling a method
typedef struct {
    DERIVE_AST_EXPR;
    strong_cstr_t name;
    ast_expr_t *value;
    ast_expr_t **args;
    length_t arity;

    // Optional return type matching, only
    // exists if 'gives.elements_length' isn't zero
    ast_type_t gives;
    
    // Ignore this call if it can't be compiled
    bool is_tentative : 1;

    // Whether __defer__ should not be called on dropped temporary values
    bool allow_drop : 1;

    // Throw an error when trying to use functions marked as "no discard"
    bool no_discard : 1;
} ast_expr_call_method_t;

// ---------------- ast_expr_va_arg_t ----------------
// Expression for using 'va_arg'
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *va_list;
    ast_type_t arg_type;
} ast_expr_va_arg_t;

// ---------------- ast_expr_initlist_t ----------------
// Expression for initializer lists
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t **elements;
    length_t length;
} ast_expr_initlist_t;

// ---------------- ast_expr_polycount_t ----------------
// Expression for polymorphic count variable
// DANGEROUS: NOTE: 'sizeof(ast_expr_polycount_t) <= sizeof(ast_expr_usize_t)'
typedef struct { DERIVE_AST_EXPR; strong_cstr_t name; } ast_expr_polycount_t;

// ---------------- ast_expr_typenameof ----------------
// Expression for getting the name of a type (as a C-String) at compile-time
typedef ast_expr_unary_type_t ast_expr_typenameof_t;

// ---------------- ast_expr_asm_t ----------------
// Expression for inline LLVM assembly (LLVM backend only)
typedef struct {
    DERIVE_AST_EXPR;
    strong_cstr_t assembly;
    weak_cstr_t constraints;
    ast_expr_t **args;
    length_t arity;
    bool has_side_effects : 1;
    bool is_stack_align : 1;
    bool is_intel : 1;
} ast_expr_llvm_asm_t;

// ---------------- ast_expr_embed_t ----------------
// Expression for embedding files at compile-time
typedef struct { DERIVE_AST_EXPR; strong_cstr_t filename; } ast_expr_embed_t;

// ---------------- ast_expr_declare_t ----------------
// Expression for declaring a variable
#define AST_EXPR_DECLARATION_POD        TRAIT_1
#define AST_EXPR_DECLARATION_ASSIGN_POD TRAIT_2
#define AST_EXPR_DECLARATION_STATIC     TRAIT_3
#define AST_EXPR_DECLARATION_CONST      TRAIT_4
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t name;
    ast_type_t type;
    ast_expr_t *value;
    trait_t traits;
    optional_ast_expr_list_t inputs;
} ast_expr_declare_t;

// ---------------- ast_expr_inline_declare_t ----------------
// Expression for declaring a variable then getting the address of it
typedef ast_expr_declare_t ast_expr_inline_declare_t;

// ---------------- ast_expr_assign_t ----------------
// Expression for assigning a value to a variable
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *destination;
    ast_expr_t *value;
    bool is_pod;
} ast_expr_assign_t;

// ---------------- ast_expr_return_t ----------------
// Expression for returning a value from a function
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *value;
    ast_expr_list_t last_minute;
} ast_expr_return_t;

// ---------------- ast_expr_conditional_t (and variants) ----------------
// Expressions that conditionally execute code found in a single block
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t label;
    ast_expr_t *value;
    ast_expr_list_t statements;
} ast_expr_conditional_t,
    // Aliases
    ast_expr_if_t, ast_expr_unless_t, ast_expr_while_t, ast_expr_until_t, ast_expr_whilecontinue_t, ast_expr_untilbreak_t;

// ---------------- ast_expr_conditional_else_t (and friends) ----------------
// Expressions that conditionally execute code found in a two blocks
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t label;
    ast_expr_t *value;
    ast_expr_list_t statements;
    ast_expr_list_t else_statements;
} ast_expr_conditional_else_t,
    // Aliases
    ast_expr_ifelse_t, ast_expr_unlesselse_t, ast_expr_ifwhileelse_t, ast_expr_unlessuntilelse_t;

// ---------------- ast_expr_conditionless_blocK_t ----------------
// A conditionless block
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_list_t statements;
} ast_expr_conditionless_block_t;

// ---------------- ast_expr_each_in_t ----------------
// Expression for 'each in' loop. Used for iterating
// over a low-level array given a length.
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t label;
    strong_cstr_t it_name;
    ast_type_t *it_type;
    ast_expr_t *length;
    ast_expr_t *low_array;
    ast_expr_t *list;
    ast_expr_list_t statements;
    bool is_static;
} ast_expr_each_in_t;

// ---------------- ast_expr_repeat_t ----------------
// Expression for 'repeat' loop. Used for iterating
// upto a given limit (which is excluded)
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t label;
    ast_expr_t *limit;
    ast_expr_list_t statements;
    maybe_null_weak_cstr_t idx_overload_name;
    bool is_static;
} ast_expr_repeat_t;

// ---------------- ast_expr_break_t ----------------
// Expression for breaking from the nearest enclosure
typedef ast_expr_t ast_expr_break_t;

// ---------------- ast_expr_continue_t ----------------
// Expression for continuing in the nearest enclosure
typedef ast_expr_t ast_expr_continue_t;

// ---------------- ast_expr_fallthrough_t ----------------
// Expression for falling through a case in the nearest switch enclosure
typedef ast_expr_t ast_expr_fallthrough_t;

// ---------------- ast_expr_break_to_t (and friends) ----------------
// Expressions for performing a specific operation given a label
typedef struct {
    DERIVE_AST_EXPR;
    source_t label_source;
    weak_cstr_t label;
} ast_expr_break_to_t, ast_expr_continue_to_t;

// ---------------- ast_case_t ----------------
//  A single 'case' of a 'switch' statement
typedef struct {
    ast_expr_t *condition;
    ast_expr_list_t statements;
    source_t source;
} ast_case_t;

typedef listof(ast_case_t, cases) ast_case_list_t;

// ---------------- ast_expr_switch_t ----------------
// Expression for switching based on a value
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *value;
    ast_case_list_t cases;
    ast_expr_list_t or_default;
    bool is_exhaustive;
} ast_expr_switch_t;

// ---------------- ast_expr_va_copy_t ----------------
// Expression for 'va_copy' instruction
typedef struct {
    DERIVE_AST_EXPR;
    ast_expr_t *dest_value;
    ast_expr_t *src_value;
} ast_expr_va_copy_t;

// ---------------- ast_expr_for_t ----------------
// Expression for 'for' loop
typedef struct {
    DERIVE_AST_EXPR;
    weak_cstr_t label;
    ast_expr_list_t before;
    ast_expr_list_t after;
    ast_expr_t *condition;
    ast_expr_list_t statements;
} ast_expr_for_t;

// ---------------- ast_expr_declare_named_expression_t ----------------
// Expression for declaring a named expression
typedef struct { DERIVE_AST_EXPR; ast_named_expression_t named_expression; } ast_expr_declare_named_expression_t;

// ---------------- expr_is_mutable ----------------
// Tests to see if the result of an expression will be mutable
bool expr_is_mutable(ast_expr_t *expr);

// ---------------- ast_expr_str ----------------
// Generates a c-string given an AST expression
strong_cstr_t ast_expr_str(ast_expr_t *expr);

// ---------------- ast_expr_free ----------------
// Frees data within an AST expression (expression can be NULL)
void ast_expr_free(ast_expr_t *expr);

// ---------------- ast_expr_free_fully ----------------
// Frees data within an AST expression and the container (expression can be NULL)
void ast_expr_free_fully(ast_expr_t *expr);

// ---------------- ast_exprs_free ----------------
// Calls 'ast_expr_free' for each expression in a list (expressions can be NULL)
void ast_exprs_free(ast_expr_t **expr, length_t length);

// ---------------- ast_exprs_free_fully ----------------
// Calls 'ast_expr_free_fully' for each expression in a list (expressions can be NULL)
void ast_exprs_free_fully(ast_expr_t **expr, length_t length);

// ---------------- ast_expr_clone ----------------
// Clones an expression, producing a duplicate
ast_expr_t *ast_expr_clone(ast_expr_t* expr);

// ---------------- ast_expr_clone_if_not_null ----------------
// If `expr` is NULL, then NULL otherwise clones an expression, producing a duplicate
ast_expr_t *ast_expr_clone_if_not_null(ast_expr_t *expr);

// ---------------- ast_expr_create_bool ----------------
// Creates a boolean expression
ast_expr_t *ast_expr_create_bool(adept_bool value, source_t source);

// ---------------- ast_expr_create_long ----------------
// Creates a long expression
ast_expr_t *ast_expr_create_long(adept_long value, source_t source);

// ---------------- ast_expr_create_double ----------------
// Creates a double expression
ast_expr_t *ast_expr_create_double(adept_double value, source_t source);

// ---------------- ast_expr_create_string ----------------
// Creates a string expression
ast_expr_t *ast_expr_create_string(char *array, length_t length, source_t source);

// ---------------- ast_expr_create_cstring ----------------
// Creates a cstring expression
ast_expr_t *ast_expr_create_cstring(char *value, source_t source);

// ---------------- ast_expr_create_null ----------------
// Creates a null expression
ast_expr_t *ast_expr_create_null(source_t source);

// ---------------- ast_expr_create_call ----------------
// Creates a call expression
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
ast_expr_t *ast_expr_create_call(strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_call_in_place ----------------
// Creates a call expression without allocating memory on the heap
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call_in_place(ast_expr_call_t *out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_method_call ----------------
// Creates a call method expression
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
ast_expr_t *ast_expr_create_call_method(strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_call_method_in_place ----------------
// Creates a call method expression without allocating memory on the heap
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call_method_in_place(ast_expr_call_method_t *out_expr, strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_variable ----------------
// Creates a variable expression
ast_expr_t *ast_expr_create_variable(weak_cstr_t name, source_t source);

// ---------------- ast_expr_create_enum_value ----------------
// Creates an enum value expression
ast_expr_t *ast_expr_create_enum_value(weak_cstr_t name, weak_cstr_t kind, source_t source);

// ---------------- ast_expr_create_ternary ----------------
// Creates a ternary expression
ast_expr_t *ast_expr_create_ternary(ast_expr_t *condition, ast_expr_t *if_true, ast_expr_t *if_false, source_t source);

// ---------------- ast_expr_create_cast ----------------
// Creates a cast expression
ast_expr_t *ast_expr_create_cast(ast_type_t to, ast_expr_t *from, source_t source);

// ---------------- ast_expr_create_phantom ----------------
// Creates a phantom expression
// NOTE: Ownership of 'ast_type' will be taken
ast_expr_t *ast_expr_create_phantom(ast_type_t ast_type, void *ir_value, source_t source, bool is_mutable);

// ---------------- ast_expr_create_typenameof ----------------
// Creates a typenameof expression
// NOTE: Ownership of 'strong_type' will be taken
ast_expr_t *ast_expr_create_typenameof(ast_type_t strong_type, source_t source);

// ---------------- ast_expr_create_embed ----------------
// Creates an embed expression
// NOTE: Ownership of 'filename' will be taken
ast_expr_t *ast_expr_create_embed(strong_cstr_t filename, source_t source);

// ---------------- ast_expr_create_declaration ----------------
// Creates a declare expression
// NOTE: Ownership of 'type', 'value', and 'inputs' will be taken
ast_expr_t *ast_expr_create_declaration(
    unsigned int expr_id,
    source_t source,
    weak_cstr_t name,
    ast_type_t type,
    trait_t traits,
    ast_expr_t *value,
    optional_ast_expr_list_t inputs
);

// ---------------- ast_expr_create_assignment ----------------
// Creates an assign expression
// NOTE: Ownership of 'mutable_expression' and 'value' will be taken
ast_expr_t *ast_expr_create_assignment(unsigned int stmt_id, source_t source, ast_expr_t *mutable_expression, ast_expr_t *value, bool is_pod);

// ---------------- ast_expr_create_return ----------------
// Creates a return statement
// NOTE: Ownership of 'value' and 'last_minute' will be taken
ast_expr_t *ast_expr_create_return(source_t source, ast_expr_t *value, ast_expr_list_t last_minute);

// ---------------- ast_expr_create_member ----------------
// Creates a member expression
// NOTE: Ownership of 'value' and 'member_name' will be taken
ast_expr_t *ast_expr_create_member(ast_expr_t *value, strong_cstr_t member_name, source_t source);

// ---------------- ast_expr_create_access ----------------
// Creates an array access expression
// NOTE: Ownership of 'value' and 'index' will be taken
ast_expr_t *ast_expr_create_access(ast_expr_t *value, ast_expr_t *index, source_t source);

// ---------------- ast_expr_create_va_arg ----------------
// Creates a va_arg expression
// NOTE: Ownership of 'va_list_value' and 'arg_type' will be taken
ast_expr_t *ast_expr_create_va_arg(source_t source, ast_expr_t *va_list_value, ast_type_t arg_type);

// ---------------- ast_expr_create_polycount ----------------
// Creates a polycount expression
ast_expr_t *ast_expr_create_polycount(source_t source, strong_cstr_t name);

// ---------------- ast_expr_create_va_copy ----------------
// Creates a va_copy statement
ast_expr_t *ast_expr_create_va_copy(source_t source, ast_expr_t *dest_value, ast_expr_t *src_value);
                
// ---------------- ast_expr_create_for ----------------
// Creates a for-loop statement
ast_expr_t *ast_expr_create_for(source_t source, weak_cstr_t label, ast_expr_list_t before, ast_expr_list_t after, ast_expr_t *condition, ast_expr_list_t statements);

// ---------------- ast_expr_create_unary ----------------
// Creates a unary expression
ast_expr_t *ast_expr_create_unary(unsigned int expr_id, source_t source, ast_expr_t *value);
#define ast_expr_create_toggle(SOURCE, VALUE) ast_expr_create_unary(EXPR_TOGGLE, (SOURCE), (VALUE))
#define ast_expr_create_pre_increment(SOURCE, VALUE) ast_expr_create_unary(EXPR_PREINCREMENT, (SOURCE), (VALUE))
#define ast_expr_create_post_increment(SOURCE, VALUE) ast_expr_create_unary(EXPR_POSTINCREMENT, (SOURCE), (VALUE))
#define ast_expr_create_pre_decrement(SOURCE, VALUE) ast_expr_create_unary(EXPR_PREDECREMENT, (SOURCE), (VALUE))
#define ast_expr_create_post_decrement(SOURCE, VALUE) ast_expr_create_unary(EXPR_POSTDECREMENT, (SOURCE), (VALUE))

// ---------------- ast_expr_create_initlist ----------------
// Creates an initializer list expression
ast_expr_t *ast_expr_create_initlist(source_t source, ast_expr_t **values, length_t length);

// ---------------- ast_expr_create_math ----------------
// Creates a math expression
ast_expr_t *ast_expr_create_math(source_t source, unsigned int expr_id, ast_expr_t *left, ast_expr_t *right);

// ---------------- ast_expr_create_switch ----------------
// Creates a switch statement
ast_expr_t *ast_expr_create_switch(source_t source, ast_expr_t *value, ast_case_list_t cases, ast_expr_list_t or_default, bool is_exhaustive);

// ---------------- ast_expr_create_declare_named_expression ----------------
// Creates a declare-named-expression statement
ast_expr_t *ast_expr_create_declare_named_expression(source_t source, ast_named_expression_t named_expression);

// ---------------- ast_expr_list_create ----------------
// Creates an ast_expr_list_t with a given capacity
ast_expr_list_t ast_expr_list_create(length_t initial_capacity);

// ---------------- ast_expr_list_create ----------------
// Frees an ast_expr_list_t
// All contained expressions will be fully freed
void ast_expr_list_free(ast_expr_list_t *list);

// ---------------- ast_expr_list_append ----------------
// Appends an expression to an ast_expr_list_t
#define ast_expr_list_append(LIST, VALUE) list_append((LIST), (VALUE), ast_expr_t*)

// ---------------- ast_expr_list_append_unchecked ----------------
// Assumes enough space exists for another item and appends an expression to an ast_expr_list_t
void ast_expr_list_append_unchecked(ast_expr_list_t *list, ast_expr_t *expr);

// ---------------- ast_expr_list_clone ----------------
// Deep-clones an AST expression list
ast_expr_list_t ast_expr_list_clone(ast_expr_list_t *list);

// ---------------- optional_ast_expr_list_clone ----------------
// Clones an optional_ast_expr_list_t
optional_ast_expr_list_t optional_ast_expr_list_clone(optional_ast_expr_list_t *list);

// ---------------- ast_expr_deduce_to_size ----------------
// Attempts to collapse an ast_expr_t into an unsigned integer value
errorcode_t ast_expr_deduce_to_size(ast_expr_t *expr, length_t *out_value);

// ---------------- ast_case_clone ----------------
// Deep-clones an 'ast_case_t'
ast_case_t ast_case_clone(ast_case_t *original);

// ---------------- ast_case_list_clone ----------------
// Deep-clones an 'ast_case_list_t'
ast_case_list_t ast_case_list_clone(ast_case_list_t *original);

// ---------------- ast_case_list_free----------------
// Frees an 'ast_case_list_t'
void ast_case_list_free(ast_case_list_t *list);

// ---------------- ast_case_list_free----------------
// Appends a new empty case to an 'ast_case_list_t'
// and returns a temporary pointer to it
ast_case_t *ast_case_list_append(ast_case_list_t *list, ast_case_t ast_case);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_EXPR_H
