
#ifndef _ISAAC_IR_VALUE_H
#define _ISAAC_IR_VALUE_H

/*
    ================================ ir_value.h ================================
    Module for creating and manipulating intermediate representation values
    ----------------------------------------------------------------------------
*/

#include "IR/ir_type.h"

// =============================================================
// ------------------ Possible IR value types ------------------
// =============================================================
#define VALUE_TYPE_NONE                0x00000000

// Non-const values
#define VALUE_TYPE_RESULT              0x00000001 // data = pointer to an 'ir_value_result_t'
#define VALUE_TYPE_ANON_GLOBAL         0x00000002 // data = pointer to an 'ir_value_anon_global_t'
#define VALUE_TYPE_STRUCT_CONSTRUCTION 0x00000003 // data = pointer to an 'ir_value_struct_construction_t'
#define VALUE_TYPE_OFFSETOF            0x00000004 // data = pointer to an 'ir_value_offsetof_t'
#define VALUE_TYPE_LAST_NON_CONST VALUE_TYPE_OFFSETOF

// Const values
#define VALUE_TYPE_LITERAL             0x00000005 // data = pointer to literal value
#define VALUE_TYPE_NULLPTR             0x00000006
#define VALUE_TYPE_NULLPTR_OF_TYPE     0x00000007
#define VALUE_TYPE_ARRAY_LITERAL       0x00000008 // data = pointer to an 'ir_value_array_literal_t'
#define VALUE_TYPE_STRUCT_LITERAL      0x00000009 // data = pointer to an 'ir_value_struct_literal_t'
#define VALUE_TYPE_CONST_ANON_GLOBAL   0x0000000A // data = pointer to an 'ir_value_anon_global_t'
#define VALUE_TYPE_CSTR_OF_LEN         0x0000000B // data = pointer to an 'ir_value_cstr_of_len_t'
#define VALUE_TYPE_CONST_SIZEOF        0x0000000C // data = pointer to an 'ir_value_const_sizeof_t'
#define VALUE_TYPE_CONST_ALIGNOF       0x0000000D // data = pointer to an 'ir_value_const_alignof_t'
#define VALUE_TYPE_CONST_ADD           0x0000000E // data = pointer to an 'ir_value_const_math_t'
#define VALUE_TYPE_LAST_CONST_NON_CAST VALUE_TYPE_CSTR_OF_LEN

// Const cast values
#define VALUE_TYPE_CONST_BITCAST       0x0000000F // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_ZEXT          0x00000010 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_SEXT          0x00000011 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_FEXT          0x00000012 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_TRUNC         0x00000013 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_FTRUNC        0x00000014 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_INTTOPTR      0x00000015 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_PTRTOINT      0x00000016 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_FPTOUI        0x00000017 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_FPTOSI        0x00000018 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_UITOFP        0x00000019 // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_SITOFP        0x0000001A // data = pointer to an 'ir_value_t'
#define VALUE_TYPE_CONST_REINTERPRET   0x0000001B // data = pointer to an 'ir_value_t'

#define VALUE_TYPE_IS_CONSTANT(a)      (a > VALUE_TYPE_LAST_NON_CONST)
#define VALUE_TYPE_IS_CONSTANT_CAST(a) (a > VALUE_TYPE_LAST_CONST_NON_CAST)

// ---------------- ir_value_t ----------------
// An intermediate representation value
typedef struct {
    unsigned int value_type;
    ir_type_t *type;
    void *extra;
} ir_value_t;

// ---------------- ir_value_result_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is the result of an instruction
typedef struct {
    length_t block_id;
    length_t instruction_id;
} ir_value_result_t;

// ---------------- ir_value_array_literal_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is an array literal
// NOTE: Type should be a pointer to the element type (since *element_type is the result type)
typedef struct {
    ir_value_t **values;
    length_t length;
} ir_value_array_literal_t;

// ---------------- ir_value_struct_literal_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is a struct literal
// NOTE: Type should be a pointer to the struct type (since *struct is the result type)
typedef struct {
    ir_value_t **values;
    length_t length;
} ir_value_struct_literal_t;

// ---------------- ir_value_anon_global_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is a reference to an anonymous global variable
typedef struct {
    length_t anon_global_id;
} ir_value_anon_global_t;

// ---------------- ir_value_cstr_of_len_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is a reference to a literal c-string of a length
// NOTE: 'size' includes null terminating character
typedef struct {
    char *array;
    length_t size;
} ir_value_cstr_of_len_t;

// ---------------- ir_value_struct_construction_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value is a construction of a structure
typedef struct {
    ir_value_t **values;
    length_t length;
} ir_value_struct_construction_t;

// ---------------- ir_value_offsetof_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value type is VALUE_TYPE_OFFSETOF
typedef struct {
    ir_type_t *type;
    length_t index;
} ir_value_offsetof_t;

// ---------------- ir_value_const_sizeof_t, ir_value_const_alignof_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value type is VALUE_TYPE_CONST_SIZEOF, VALUE_TYPE_CONST_ALIGNOF
typedef struct {
    ir_type_t *type;
} ir_value_const_sizeof_t, ir_value_const_alignof_t;

// ---------------- ir_value_const_math_t ----------------
// Structure for 'extra' field of 'ir_value_t' if
// the value type is VALUE_TYPE_CONST_ADD
typedef struct {
    ir_value_t *a;
    ir_value_t *b;
} ir_value_const_math_t;

#endif // _ISAAC_IR_VALUE_H
