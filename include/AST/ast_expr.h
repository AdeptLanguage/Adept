
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

#include "UTIL/ground.h"
#include "UTIL/datatypes.h"
#include "AST/ast_type.h"

// =============================================================
// ---------------- Possible AST Expression IDs ----------------
// =============================================================
#define EXPR_NONE             0x00000000
// Literal values ---------------------
#define EXPR_BYTE             0x00000001
#define EXPR_UBYTE            0x00000002
#define EXPR_SHORT            0x00000003
#define EXPR_USHORT           0x00000004
#define EXPR_INT              0x00000005
#define EXPR_UINT             0x00000006
#define EXPR_LONG             0x00000007
#define EXPR_ULONG            0x00000008
#define EXPR_USIZE            0x00000009
#define EXPR_FLOAT            0x0000000A
#define EXPR_DOUBLE           0x0000000B
#define EXPR_BOOLEAN          0x0000000C
#define EXPR_STR              0x0000000D
#define EXPR_CSTR             0x0000000E
#define EXPR_NULL             0x0000000F
#define EXPR_GENERIC_INT      0x00000010
#define EXPR_GENERIC_FLOAT    0x00000011
// Basic operators --------------------
#define EXPR_ADD              0x00000012
#define EXPR_SUBTRACT         0x00000013
#define EXPR_MULTIPLY         0x00000014
#define EXPR_DIVIDE           0x00000015
#define EXPR_MODULUS          0x00000016
#define EXPR_EQUALS           0x00000017
#define EXPR_NOTEQUALS        0x00000018
#define EXPR_GREATER          0x00000019
#define EXPR_LESSER           0x0000001A
#define EXPR_GREATEREQ        0x0000001B
#define EXPR_LESSEREQ         0x0000001C
#define EXPR_AND              0x0000001D
#define EXPR_OR               0x0000001E
#define EXPR_NOT              0x0000001F
#define EXPR_BIT_AND          0x00000020
#define EXPR_BIT_OR           0x00000021
#define EXPR_BIT_XOR          0x00000022
#define EXPR_BIT_COMPLEMENT   0x00000023
#define EXPR_BIT_LSHIFT       0x00000024
#define EXPR_BIT_RSHIFT       0x00000025
#define EXPR_BIT_LGC_LSHIFT   0x00000026
#define EXPR_BIT_LGC_RSHIFT   0x00000027
#define EXPR_NEGATE           0x00000028
#define EXPR_AT               0x00000029
// Complex operators ------------------
#define EXPR_CALL             0x0000002A
#define EXPR_VARIABLE         0x0000002B
#define EXPR_MEMBER           0x0000002C
#define EXPR_ADDRESS          0x0000002D
#define EXPR_FUNC_ADDR        0x0000002E
#define EXPR_DEREFERENCE      0x0000002F
#define EXPR_ARRAY_ACCESS     0x00000030
#define EXPR_CAST             0x00000031
#define EXPR_SIZEOF           0x00000032
#define EXPR_SIZEOF_VALUE     0x00000033
#define EXPR_CALL_METHOD      0x00000034
#define EXPR_NEW              0x00000035
#define EXPR_NEW_CSTRING      0x00000036
#define EXPR_ENUM_VALUE       0x00000037
#define EXPR_STATIC_ARRAY     0x00000038
#define EXPR_STATIC_STRUCT    0x00000039
#define EXPR_TYPEINFO         0x0000003A
#define EXPR_TERNARY          0x0000003B
#define EXPR_PREINCREMENT     0x0000003C
#define EXPR_PREDECREMENT     0x0000003D
#define EXPR_POSTINCREMENT    0x0000003E
#define EXPR_POSTDECREMENT    0x0000003F
#define EXPR_PHANTOM          0x00000040
#define EXPR_TOGGLE           0x00000041
#define EXPR_VA_ARG           0x00000042
#define EXPR_INITLIST         0x00000043
#define EXPR_POLYCOUNT        0x00000044
#define EXPR_LLVM_ASM         0x00000045
// Exclusive statements ---------------
#define EXPR_DECLARE          0x00000050
#define EXPR_DECLAREUNDEF     0x00000051
#define EXPR_ILDECLARE        0x00000052
#define EXPR_ILDECLAREUNDEF   0x00000053
#define EXPR_ASSIGN           0x00000054
#define EXPR_ADD_ASSIGN       0x00000055
#define EXPR_SUBTRACT_ASSIGN  0x00000056
#define EXPR_MULTIPLY_ASSIGN  0x00000057
#define EXPR_DIVIDE_ASSIGN    0x00000058
#define EXPR_MODULUS_ASSIGN   0x00000059
#define EXPR_AND_ASSIGN       0x0000005A
#define EXPR_OR_ASSIGN        0x0000005B
#define EXPR_XOR_ASSIGN       0x0000005C
#define EXPR_LS_ASSIGN        0x0000005D
#define EXPR_RS_ASSIGN        0x0000005E
#define EXPR_LGC_LS_ASSIGN    0x0000005F
#define EXPR_LGC_RS_ASSIGN    0x00000060
#define EXPR_RETURN           0x00000061
#define EXPR_IF               0x00000062
#define EXPR_UNLESS           0x00000063
#define EXPR_IFELSE           0x00000064
#define EXPR_UNLESSELSE       0x00000065
#define EXPR_WHILE            0x00000066
#define EXPR_UNTIL            0x00000067
#define EXPR_WHILECONTINUE    0x00000068
#define EXPR_UNTILBREAK       0x00000069
#define EXPR_EACH_IN          0x0000006A
#define EXPR_REPEAT           0x0000006B
#define EXPR_DELETE           0x0000006C
#define EXPR_BREAK            0x0000006D
#define EXPR_CONTINUE         0x0000006E
#define EXPR_FALLTHROUGH      0x0000006F
#define EXPR_BREAK_TO         0x00000070
#define EXPR_CONTINUE_TO      0x00000071
#define EXPR_SWITCH           0x00000072
#define EXPR_VA_START         0x00000073
#define EXPR_VA_END           0x00000074
#define EXPR_VA_COPY          0x00000075
#define EXPR_FOR              0x00000076
#define EXPR_DECLARE_CONSTANT 0x00000077

#define MAX_AST_EXPR EXPR_DECLARE_CONSTANT

// Static data that stores general expression syntax representations
extern const char *global_expression_rep_table[];

// ---------------- ast_expr_t ----------------
// General purpose struct for an expression
typedef struct {
    unsigned int id;  // What type of expression
    source_t source;  // Where in source code
} ast_expr_t;

// "AST/ast_constant.h" requires ast_expr_t to be defined
#include "AST/ast_constant.h"

// ---------------- ast_expr_list_t ----------------
// List structure for holding statements/expressions
typedef struct {
    ast_expr_t **statements;
    length_t length;
    length_t capacity;
} ast_expr_list_t;

// ---------------- ast_expr_byte_t ----------------
// Expression for a literal byte value
typedef struct {
    unsigned int id;
    source_t source;
    adept_byte value;
} ast_expr_byte_t;

// ---------------- ast_expr_ubyte_t ----------------
// Expression for a literal unsigned byte value
typedef struct {
    unsigned int id;
    source_t source;
    adept_ubyte value;
} ast_expr_ubyte_t;

// ---------------- ast_expr_short_t ----------------
// Expression for a literal short value
typedef struct {
    unsigned int id;
    source_t source;
    adept_short value;
} ast_expr_short_t;

// ---------------- ast_expr_ushort_t ----------------
// Expression for a literal unsigned short value
typedef struct {
    unsigned int id;
    source_t source;
    adept_ushort value;
} ast_expr_ushort_t;

// ---------------- ast_expr_int_t ----------------
// Expression for a literal integer value
typedef struct {
    unsigned int id;
    source_t source;
    adept_int value;
} ast_expr_int_t;

// ---------------- ast_expr_uint_t ----------------
// Expression for a literal unsigned integer value
typedef struct {
    unsigned int id;
    source_t source;
    adept_uint value;
} ast_expr_uint_t;

// ---------------- ast_expr_long_t ----------------
// Expression for a literal long value
typedef struct {
    unsigned int id;
    source_t source;
    adept_long value;
} ast_expr_long_t;

// ---------------- ast_expr_ulong_t ----------------
// Expression for a literal unsigned long value
typedef struct {
    unsigned int id;
    source_t source;
    adept_ulong value;
} ast_expr_ulong_t;

// ---------------- ast_expr_usize_t ----------------
// Expression for a literal unsigned size value
typedef struct {
    unsigned int id;
    source_t source;
    adept_usize value;
} ast_expr_usize_t;

// ---------------- ast_expr_float_t ----------------
// Expression for a literal 32-bit float value
typedef struct {
    unsigned int id;
    source_t source;
    adept_float value;
} ast_expr_float_t;

// ---------------- ast_expr_double_t ----------------
// Expression for a literal 64-bit float value
typedef struct {
    unsigned int id;
    source_t source;
    adept_double value;
} ast_expr_double_t;

// ---------------- ast_expr_boolean_t ----------------
// Expression for a literal boolean value
typedef struct {
    unsigned int id;
    source_t source;
    adept_bool value;
} ast_expr_boolean_t;

// ---------------- ast_expr_str_t ----------------
// Expression for a literal length-string value
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t array;
    length_t length;
} ast_expr_str_t;

// ---------------- ast_expr_cstr_t ----------------
// Expression for a literal null-terminated string value
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t value;
} ast_expr_cstr_t;

// ---------------- ast_expr_null_t ----------------
// Expression for 'null' value
typedef ast_expr_t ast_expr_null_t;

// ---------------- ast_expr_generic_int_t ----------------
// Expression for a generic integer value of an undetermined type
typedef struct {
    unsigned int id;
    source_t source;
    adept_generic_int value;
} ast_expr_generic_int_t;

// ---------------- ast_expr_generic_float_t ----------------
// Expression for a generic float value of an undetermined type
typedef struct {
    unsigned int id;
    source_t source;
    adept_generic_float value;
} ast_expr_generic_float_t;

// ---------------- ast_expr_math_t ----------------
// General purpose two-operand expression
// Used for: add, subtract, multiply, divide, modulus, power, equals,
//           notequals, greater, lesser, greatereq, lessereq, and, or
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *a;
    ast_expr_t *b;
} ast_expr_math_t, ast_expr_and_t, ast_expr_or_t;

// ---------------- ast_expr_unary_t ----------------
// General purpose single-operand expression
// Used for: address, dereference, bit complement, not, negate, delete, toggle
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_unary_t, ast_expr_address_t, ast_expr_dereference_t, ast_expr_bitwise_complement_t, ast_expr_not_t,
    ast_expr_negate_t, ast_expr_delete_t, ast_expr_toggle_t, ast_expr_va_start_t, ast_expr_va_end_t;

// ---------------- ast_expr_new_t ----------------
// Expression for 'new' keyword, used to dynamically
// allocate memory on the heap
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t type;
    ast_expr_t *amount; // Can be NULL to indicate single element
    bool is_undef;
} ast_expr_new_t;

// ---------------- ast_expr_new_cstring_t ----------------
// Dynamically allocates a null-terminated string on the heap
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t value;
} ast_expr_new_cstring_t;

// ---------------- ast_expr_enum_value_t ----------------
// Gets the constant value of an enum kind
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t enum_name;
    weak_cstr_t kind_name;
} ast_expr_enum_value_t;

// ---------------- ast_expr_static_data_t ----------------
// Represents a static array value
// (Used for EXPR_STATIC_ARRAY and EXPR_STATIC_STRUCT)
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t type;
    ast_expr_t **values;
    length_t length;
} ast_expr_static_data_t;

// ---------------- ast_expr_typeinfo_t ----------------
// Expression for getting runtime type information for a type
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t target;
} ast_expr_typeinfo_t;

// ---------------- ast_expr_ternary_t ----------------
// Expression for conditionally selecting between two expressions
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *condition;
    ast_expr_t *if_true;
    ast_expr_t *if_false;
} ast_expr_ternary_t;

// ---------------- ast_expr_call_t ----------------
// Expression for calling a function
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
    ast_expr_t **args;
    length_t arity;
    bool is_tentative;

    // Optional return type matching, only
    // exists if 'gives.elements_length' isn't zero
    ast_type_t gives;

    // Special flag to make this call
    // fail if it's tentative and the target function
    // isn't marked as implicit
    // (used for internal implicit calls)
    bool only_implicit;

    // Special flag to make this call
    // Not allow for implicit user casts
    bool no_user_casts;
} ast_expr_call_t;

// ---------------- ast_expr_variable_t ----------------
// Expression for accessing a variable
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t name;
} ast_expr_variable_t;

// ---------------- ast_expr_member_t ----------------
// Expression for accessing a member of a value
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
    strong_cstr_t member;
} ast_expr_member_t;

// ---------------- ast_expr_func_addr_t ----------------
// Expression for retrieving a function address
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t name;
    ast_type_t *match_args;
    length_t match_args_length;
    trait_t traits;
    bool tentative;
    bool has_match_args; // Indicates whether name-only
} ast_expr_func_addr_t;

// ---------------- ast_expr_array_access_t ----------------
// Expression for accessing an element in an array
// (Used for: EXPR_ARRAY_ACCESS and EXPR_AT)
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
    ast_expr_t *index;
} ast_expr_array_access_t;

// ---------------- ast_expr_cast_t ----------------
// Expression for casting a value to a specific type
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t to;
    ast_expr_t *from;
} ast_expr_cast_t;

// ---------------- ast_expr_sizeof_t ----------------
// Expression for getting the size of a type
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t type;
} ast_expr_sizeof_t;

// ---------------- ast_expr_sizeof_value_t ----------------
// Expression for getting the size of a type
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_sizeof_value_t;

// ---------------- ast_expr_phantom_t ----------------
// Expression for passing precomputed ir_value_t values into AST expressions
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t type;
    void *ir_value;
    bool is_mutable;
} ast_expr_phantom_t;

// ---------------- ast_expr_call_method_t ----------------
// Expression for calling a method
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
    ast_expr_t *value;
    ast_expr_t **args;
    length_t arity;
    bool is_tentative;

    // Optional return type matching, only
    // exists if 'gives.elements_length' isn't zero
    ast_type_t gives;
} ast_expr_call_method_t;

// ---------------- ast_expr_va_arg_t ----------------
// Expression for using 'va_arg'
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *va_list;
    ast_type_t arg_type;
} ast_expr_va_arg_t;

// ---------------- ast_expr_initlist_t ----------------
// Expression for initializer lists
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t **elements;
    length_t length;
} ast_expr_initlist_t;

// ---------------- ast_expr_polycount_t ----------------
// Expression for polymorphic count variable
// DANGEROUS: NOTE: 'sizeof(ast_expr_polycount_t) <= sizeof(ast_expr_usize_t)'
typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t name;
} ast_expr_polycount_t;

typedef struct {
    unsigned int id;
    source_t source;
    strong_cstr_t assembly;
    weak_cstr_t constraints;
    ast_expr_t **args;
    length_t arity;
    bool has_side_effects;
    bool is_stack_align;
    bool is_intel;
} ast_expr_llvm_asm_t;

// ---------------- ast_expr_declare_t ----------------
// Expression for declaring a variable
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t name;
    ast_type_t type;
    ast_expr_t *value;
    bool is_pod;
    bool is_assign_pod;
    bool is_static;
    bool is_const;
} ast_expr_declare_t;

// ---------------- ast_expr_inline_declare_t ----------------
// Expression for declaring a variable then getting the address of it
typedef ast_expr_declare_t ast_expr_inline_declare_t;

// ---------------- ast_expr_assign_t ----------------
// Expression for assigning a value to a variable
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *destination;
    ast_expr_t *value;
    bool is_pod;
} ast_expr_assign_t;

// ---------------- ast_expr_return_t ----------------
// Expression for returning a value from a function
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
    ast_expr_list_t last_minute;
} ast_expr_return_t;

// ---------------- ast_expr_if_t (and friends) ----------------
// Expressions that conditionally execute code found in a single block
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t label;
    ast_expr_t *value;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
} ast_expr_if_t, ast_expr_unless_t, ast_expr_while_t, ast_expr_until_t, ast_expr_whilecontinue_t, ast_expr_untilbreak_t;

// ---------------- ast_expr_ifelse_t (and friends) ----------------
// Expressions that conditionally execute code found in a two blocks
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t label;
    ast_expr_t *value;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
    ast_expr_t **else_statements;
    length_t else_statements_length;
    length_t else_statements_capacity;
} ast_expr_ifelse_t, ast_expr_unlesselse_t, ast_expr_ifwhileelse_t, ast_expr_unlessuntilelse_t;

// ---------------- ast_expr_each_in_t ----------------
// Expression for 'each in' loop. Used for iterating
// over a low-level array given a length.
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t label;
    strong_cstr_t it_name;
    ast_type_t *it_type;
    ast_expr_t *length;
    ast_expr_t *low_array;
    ast_expr_t *list;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
    bool is_static;
} ast_expr_each_in_t;

// ---------------- ast_expr_repeat_t ----------------
// Expression for 'repeat' loop. Used for iterating
// upto a given limit (which is excluded)
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t label;
    ast_expr_t *limit;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
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
    unsigned int id;
    source_t source;
    source_t label_source;
    weak_cstr_t label;
} ast_expr_break_to_t, ast_expr_continue_to_t;

// ---------------- ast_case_t ----------------
typedef struct {
    ast_expr_t *condition;
    source_t source;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
} ast_case_t;

// ---------------- ast_expr_switch_t ----------------
// Expression for switching based on a value
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
    ast_case_t *cases;
    length_t cases_length;
    length_t cases_capacity;
    ast_expr_t **default_statements;
    length_t default_statements_length;
    length_t default_statements_capacity;
    bool is_exhaustive;
} ast_expr_switch_t;

// ---------------- ast_expr_va_copy_t ----------------
// Expression for 'va_copy' instruction
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *dest_value;
    ast_expr_t *src_value;
} ast_expr_va_copy_t;

// ---------------- ast_expr_for_t ----------------
// Expression for 'for' loop
typedef struct {
    unsigned int id;
    source_t source;
    weak_cstr_t label;
    ast_expr_list_t before;
    ast_expr_t *condition;
    ast_expr_list_t statements;
} ast_expr_for_t;

// ---------------- ast_expr_declare_constant_t ----------------
// (NOTE: ast_expr_declare_constant_t is declared in AST/ast_constant.h)

// ---------------- expr_is_mutable ----------------
// Tests to see if the result of an expression will be mutable
bool expr_is_mutable(ast_expr_t *expr);

// ---------------- ast_expr_str ----------------
// Generates a c-string given an AST expression
strong_cstr_t ast_expr_str(ast_expr_t *expr);

// ---------------- ast_expr_free ----------------
// Frees data within an AST expression
void ast_expr_free(ast_expr_t *expr);

// ---------------- ast_expr_free_fully ----------------
// Frees data within an AST expression and the container
void ast_expr_free_fully(ast_expr_t *expr);

// ---------------- ast_exprs_free ----------------
// Calls 'ast_expr_free' for each expression in a list
void ast_exprs_free(ast_expr_t **expr, length_t length);

// ---------------- ast_exprs_free_fully ----------------
// Calls 'ast_expr_free_fully' for each expression in a list
void ast_exprs_free_fully(ast_expr_t **expr, length_t length);

// ---------------- ast_expr_clone ----------------
// Clones an expression, producing a duplicate
ast_expr_t *ast_expr_clone(ast_expr_t* expr);

// ---------------- ast_expr_create_bool ----------------
// Creates a boolean expression
void ast_expr_create_bool(ast_expr_t **out_expr, adept_bool value, source_t source);

// ---------------- ast_expr_create_long ----------------
// Creates a long expression
void ast_expr_create_long(ast_expr_t **out_expr, adept_long value, source_t source);

// ---------------- ast_expr_create_double ----------------
// Creates a double expression
void ast_expr_create_double(ast_expr_t **out_expr, adept_double value, source_t source);

// ---------------- ast_expr_create_string ----------------
// Creates a string expression
void ast_expr_create_string(ast_expr_t **out_expr, char *array, length_t length, source_t source);

// ---------------- ast_expr_create_cstring ----------------
// Creates a cstring expression
void ast_expr_create_cstring(ast_expr_t **out_expr, char *value, source_t source);

// ---------------- ast_expr_create_null ----------------
// Creates a null expression
void ast_expr_create_null(ast_expr_t **out_expr, source_t source);

// ---------------- ast_expr_create_call ----------------
// Creates a call expression
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call(ast_expr_t **out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_call_in_place ----------------
// Creates a call expression without allocating memory on the heap
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call_in_place(ast_expr_call_t *out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_method_call ----------------
// Creates a call method expression
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call_method(ast_expr_t **out_expr, strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_call_method_in_place ----------------
// Creates a call method expression without allocating memory on the heap
// NOTE: 'gives' may be NULL or 'gives.elements_length' be zero
//       to indicate no return matching
void ast_expr_create_call_method_in_place(ast_expr_call_method_t *out_expr, strong_cstr_t name, ast_expr_t *value,
        length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source);

// ---------------- ast_expr_create_variable ----------------
// Creates a variable expression
void ast_expr_create_variable(ast_expr_t **out_expr, weak_cstr_t name, source_t source);

// ---------------- ast_expr_create_enum_value ----------------
// Creates an enum value expression
void ast_expr_create_enum_value(ast_expr_t **out_expr, weak_cstr_t name, weak_cstr_t kind, source_t source);

// ---------------- ast_expr_create_ternary ----------------
// Creates a ternary expression
void ast_expr_create_ternary(ast_expr_t **out_expr, ast_expr_t *condition, ast_expr_t *if_true, ast_expr_t *if_false, source_t source);

// ---------------- ast_expr_create_cast ----------------
// Creates a cast expression
void ast_expr_create_cast(ast_expr_t **out_expr, ast_type_t to, ast_expr_t *from, source_t source);

// ---------------- ast_expr_list_init ----------------
// Initializes an ast_expr_list_t with a given capacity
void ast_expr_list_init(ast_expr_list_t *list, length_t capacity);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_AST_EXPR_H
