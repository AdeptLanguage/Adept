
#ifndef AST_EXPR_H
#define AST_EXPR_H

/*
    =============================== ast_expr.h ================================
    Module for manipulating abstract syntax tree expressions
    ---------------------------------------------------------------------------
*/

#include "ground.h"
#include "ast_type.h"

// =============================================================
// ---------------- Possible AST Expression IDs ----------------
// =============================================================
#define EXPR_NONE           0x00000000
// Literal values --------------------
#define EXPR_BYTE           0x00000001
#define EXPR_UBYTE          0x00000002
#define EXPR_SHORT          0x00000003
#define EXPR_USHORT         0x00000004
#define EXPR_INT            0x00000005
#define EXPR_UINT           0x00000006
#define EXPR_LONG           0x00000007
#define EXPR_ULONG          0x00000008
#define EXPR_FLOAT          0x00000009
#define EXPR_DOUBLE         0x0000000A
#define EXPR_BOOLEAN        0x0000000B
#define EXPR_STR            0x0000000C
#define EXPR_CSTR           0x0000000D
#define EXPR_NULL           0x0000000E
#define EXPR_GENERIC_INT    0x0000000F
#define EXPR_GENERIC_FLOAT  0x00000010
// Basic operators -------------------
#define EXPR_ADD            0x00000011
#define EXPR_SUBTRACT       0x00000012
#define EXPR_MULTIPLY       0x00000013
#define EXPR_DIVIDE         0x00000014
#define EXPR_MODULUS        0x00000015
#define EXPR_EQUALS         0x00000016
#define EXPR_NOTEQUALS      0x00000017
#define EXPR_GREATER        0x00000018
#define EXPR_LESSER         0x00000019
#define EXPR_GREATEREQ      0x0000001A
#define EXPR_LESSEREQ       0x0000001B
#define EXPR_AND            0x0000001C
#define EXPR_OR             0x0000001D
#define EXPR_NOT            0x0000001E
// Complex operators -----------------
#define EXPR_CALL           0x0000001F
#define EXPR_VARIABLE       0x00000020
#define EXPR_MEMBER         0x00000021
#define EXPR_ADDRESS        0x00000022
#define EXPR_FUNC_ADDR      0x00000023
#define EXPR_DEREFERENCE    0x00000024
#define EXPR_ARRAY_ACCESS   0x00000025
#define EXPR_CAST           0x00000026
#define EXPR_SIZEOF         0x00000027
#define EXPR_CALL_METHOD    0x00000028
#define EXPR_NEW            0x00000029
// Exclusive statements --------------
#define EXPR_DECLARE        0x0000002A
#define EXPR_DECLAREUNDEF   0x0000002B
#define EXPR_ASSIGN         0x0000002C
#define EXPR_ADDASSIGN      0x0000002D
#define EXPR_SUBTRACTASSIGN 0x0000002E
#define EXPR_MULTIPLYASSIGN 0x0000002F
#define EXPR_DIVIDEASSIGN   0x00000030
#define EXPR_MODULUSASSIGN  0x00000031
#define EXPR_RETURN         0x00000032
#define EXPR_IF             0x00000033
#define EXPR_UNLESS         0x00000034
#define EXPR_IFELSE         0x00000035
#define EXPR_UNLESSELSE     0x00000036
#define EXPR_WHILE          0x00000037
#define EXPR_UNTIL          0x00000038
#define EXPR_WHILECONTINUE  0x00000039
#define EXPR_UNTILBREAK     0x0000003A
#define EXPR_EACH_FOR       0x0000003B
#define EXPR_DELETE         0x0000003C
#define EXPR_BREAK          0x0000003D
#define EXPR_CONTINUE       0x0000003E
#define EXPR_BREAK_TO       0x0000003F
#define EXPR_CONTINUE_TO    0x00000040

#define MAX_AST_EXPR EXPR_CONTINUE_TO

// ---------------- EXPR_IS_MUTABLE(expr) ----------------
// Tests to see if the result of an expression will be mutable
#define EXPR_IS_MUTABLE(a) (a == EXPR_VARIABLE || a == EXPR_MEMBER || a == EXPR_DEREFERENCE || a == EXPR_ARRAY_ACCESS)

// ---------------- EXPR_IS_TERMINATION(expr) ----------------
// Tests to see if the expression is a termination statement
#define EXPR_IS_TERMINATION(a) (a == EXPR_RETURN || a == EXPR_BREAK || a == EXPR_CONTINUE || a == EXPR_BREAK_TO || a == EXPR_CONTINUE_TO)

// Static data that stores general expression syntax representations
extern const char *global_expression_rep_table[];

// ---------------- ast_expr_t ----------------
// General purpose struct for an expression
typedef struct {
    unsigned int id;  // What type of expression
    source_t source;  // Where in source code
} ast_expr_t;

// ---------------- ast_expr_byte_t ----------------
// Expression for a literal byte value
typedef struct {
    unsigned int id;
    source_t source;
    char value;
} ast_expr_byte_t;

// ---------------- ast_expr_ubyte_t ----------------
// Expression for a literal unsigned byte value
typedef struct {
    unsigned int id;
    source_t source;
    unsigned char value;
} ast_expr_ubyte_t;

// ---------------- ast_expr_short_t ----------------
// Expression for a literal short value
typedef struct {
    unsigned int id;
    source_t source;
    int value;
} ast_expr_short_t;

// ---------------- ast_expr_ushort_t ----------------
// Expression for a literal unsigned short value
typedef struct {
    unsigned int id;
    source_t source;
    unsigned int value;
} ast_expr_ushort_t;

// ---------------- ast_expr_int_t ----------------
// Expression for a literal integer value
typedef struct {
    unsigned int id;
    source_t source;
    long long value;
} ast_expr_int_t;

// ---------------- ast_expr_uint_t ----------------
// Expression for a literal unsigned integer value
typedef struct {
    unsigned int id;
    source_t source;
    unsigned long value;
} ast_expr_uint_t;

// ---------------- ast_expr_long_t ----------------
// Expression for a literal long value
typedef struct {
    unsigned int id;
    source_t source;
    long long value;
} ast_expr_long_t;

// ---------------- ast_expr_ulong_t ----------------
// Expression for a literal unsigned long value
typedef struct {
    unsigned int id;
    source_t source;
    unsigned long long value;
} ast_expr_ulong_t;

// ---------------- ast_expr_float_t ----------------
// Expression for a literal 32-bit float value
typedef struct {
    unsigned int id;
    source_t source;
    double value; // Using double for minimal data lost
} ast_expr_float_t;

// ---------------- ast_expr_double_t ----------------
// Expression for a literal 64-bit float value
typedef struct {
    unsigned int id;
    source_t source;
    double value;
} ast_expr_double_t;

// ---------------- ast_expr_boolean_t ----------------
// Expression for a literal boolean value
typedef struct {
    unsigned int id;
    source_t source;
    bool value;
} ast_expr_boolean_t;

// ---------------- ast_expr_str_t ----------------
// Expression for a literal length-string value
typedef struct {
    unsigned int id;
    source_t source;
    char *value;
} ast_expr_str_t;

// ---------------- ast_expr_cstr_t ----------------
// Expression for a literal null-terminated string value
typedef struct {
    unsigned int id;
    source_t source;
    char *value;
} ast_expr_cstr_t;

// ---------------- ast_expr_null_t ----------------
// Expression for 'null' value
typedef ast_expr_t ast_expr_null_t;

// ---------------- ast_expr_generic_int_t ----------------
// Expression for a generic integer value of an undetermined type
typedef struct {
    unsigned int id;
    source_t source;
    long long value;
} ast_expr_generic_int_t;

// ---------------- ast_expr_generic_float_t ----------------
// Expression for a generic float value of an undetermined type
typedef struct {
    unsigned int id;
    source_t source;
    double value;
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
} ast_expr_math_t;

// ---------------- ast_expr_not_t ----------------
// Expression for not operator
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_not_t;

// ---------------- ast_expr_new_t ----------------
// Expression for 'new' keyword, used to dynamically
// allocate memory on the heap
typedef struct {
    unsigned int id;
    source_t source;
    ast_type_t type;
    ast_expr_t *amount; // Can be NULL to indicate single element
} ast_expr_new_t;

// ---------------- ast_expr_call_t ----------------
// Expression for calling a function
typedef struct {
    unsigned int id;
    source_t source;
    const char *name;
    ast_expr_t **args;
    length_t arity;
} ast_expr_call_t;

// ---------------- ast_expr_variable_t ----------------
// Expression for accessing a variable
typedef struct {
    unsigned int id;
    source_t source;
    char *name;
} ast_expr_variable_t;

// ---------------- ast_expr_member_t ----------------
// Expression for accessing a member of a value
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
    char *member;
} ast_expr_member_t;

// ---------------- ast_expr_address_t ----------------
// Expression for forcing a value to be kept mutable
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_address_t;

// ---------------- ast_expr_func_addr_t ----------------
// Expression for retrieving a function address
typedef struct {
    unsigned int id;
    source_t source;
    const char *name;
    ast_unnamed_arg_t *match_args; // Can be NULL to indicate name-only
    length_t match_args_length;
    trait_t traits;
} ast_expr_func_addr_t;

// ---------------- ast_expr_deref_t ----------------
// Expression for dereferencing a value
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_deref_t;

// ---------------- ast_expr_array_access_t ----------------
// Expression for accessing an element in an array
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

// ---------------- ast_expr_call_method_t ----------------
// Expression for calling a method
typedef struct {
    unsigned int id;
    source_t source;
    const char *name;
    ast_expr_t *value;
    ast_expr_t **args;
    length_t arity;
} ast_expr_call_method_t;

// ---------------- ast_expr_declare_t ----------------
// Expression for declaring a variable
typedef struct {
    unsigned int id;
    source_t source;
    const char *name;
    ast_type_t type;
    ast_expr_t *value;
} ast_expr_declare_t;

// ---------------- ast_expr_assign_t ----------------
// Expression for assigning a value to a variable
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *destination;
    ast_expr_t *value;
} ast_expr_assign_t;

// ---------------- ast_expr_return_t ----------------
// Expression for returning a value from a function
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_return_t;

// ---------------- ast_expr_if_t (and friends) ----------------
// Expressions that conditionally execute code found in a single block
typedef struct {
    unsigned int id;
    source_t source;
    char *label;
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
    char *label;
    ast_expr_t *value;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
    ast_expr_t **else_statements;
    length_t else_statements_length;
    length_t else_statements_capacity;
} ast_expr_ifelse_t, ast_expr_unlesselse_t, ast_expr_ifwhileelse_t, ast_expr_unlessuntilelse_t;

// ---------------- ast_expr_each_for_t ----------------
// Expression for 'each for' loop. Used for iterating
// over a low-level array given a length.
typedef struct {
    unsigned int id;
    source_t source;
    char *label;
    ast_expr_t *length;
    ast_expr_t *low_array;
    ast_expr_t **statements;
    length_t statements_length;
    length_t statements_capacity;
} ast_expr_each_for_t;

// ---------------- ast_expr_delete_t ----------------
// Expression for 'delete' keyword. Frees dynamically
// allocated memory previously allocated with 'new'
typedef struct {
    unsigned int id;
    source_t source;
    ast_expr_t *value;
} ast_expr_delete_t;

// ---------------- ast_expr_break_t ----------------
// Expression for breaking from the nearest enclosure
typedef ast_expr_t ast_expr_break_t;

// ---------------- ast_expr_continue_t ----------------
// Expression for continuing in the nearest enclosure
typedef ast_expr_t ast_expr_continue_t;

// ---------------- ast_expr_break_to_t (and friends) ----------------
// Expressions for performing a specific operation given a label
typedef struct {
    unsigned int id;
    source_t source;
    source_t label_source;
    char *label;
} ast_expr_break_to_t, ast_expr_continue_to_t;

// ---------------- ast_expr_list_t ----------------
// List structure for holding statements/expressions
typedef struct {
    ast_expr_t **statements;
    length_t length;
    length_t capacity;
} ast_expr_list_t;

// ---------------- ast_expr_str ----------------
// Generates a c-string given an AST expression
char* ast_expr_str(ast_expr_t *expr);

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

#endif // AST_EXPR_H
