
#ifndef _ISAAC_IR_TYPE_H
#define _ISAAC_IR_TYPE_H

#include <stdbool.h>

#include "IR/ir_pool.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

// ============================================================
// ------------------ Possible IR type kinds ------------------
// ============================================================
#define TYPE_KIND_NONE              0x00000000 // extra = NULL
#define TYPE_KIND_POINTER           0x00000001 // extra = ir_type_extra_pointer_t*
#define TYPE_KIND_S8                0x00000002 // extra = NULL
#define TYPE_KIND_S16               0x00000003 // extra = NULL
#define TYPE_KIND_S32               0x00000004 // extra = NULL
#define TYPE_KIND_S64               0x00000005 // extra = NULL
#define TYPE_KIND_U8                0x00000006 // extra = NULL
#define TYPE_KIND_U16               0x00000007 // extra = NULL
#define TYPE_KIND_U32               0x00000008 // extra = NULL
#define TYPE_KIND_U64               0x00000009 // extra = NULL
#define TYPE_KIND_HALF              0x0000000A // extra = NULL
#define TYPE_KIND_FLOAT             0x0000000B // extra = NULL
#define TYPE_KIND_DOUBLE            0x0000000C // extra = NULL
#define TYPE_KIND_BOOLEAN           0x0000000D // extra = NULL
#define TYPE_KIND_STRUCTURE         0x0000000E // extra = ir_type_extra_composite_t*
#define TYPE_KIND_UNION             0x0000000F // extra = ir_type_extra_composite_t*
#define TYPE_KIND_VOID              0x00000010 // extra = NULL
#define TYPE_KIND_FUNCPTR           0x00000011 // extra = NULL
#define TYPE_KIND_FIXED_ARRAY       0x00000012 // extra = ir_type_extra_fixed_array_t*
#define TYPE_KIND_UNKNOWN_ENUM      0x00000013 // extra = ir_type_extra_unknown_enum_t*
#define TYPE_KIND_UNBUILT_COMPOSITE 0x00000014 // extra = ast_composite_t* (only used during processing)

#define IS_TYPE_KIND_SIGNED(a) global_type_kind_signs[a]

// ---------------- ir_type_t ----------------
// An intermediate representation type
// 'extra' can optionally contain one of the following for example:
// 'ir_type_t', 'ir_type_extra_composite_t', 'ir_type_extra_fixed_array_t', 'ir_type_extra_unknown_enum_t'
typedef struct {
    unsigned int kind;
    void *extra;
} ir_type_t;

// ---------------- ir_type_extra_pointer_t ----------------
// Structure for 'extra' field of 'ir_type_t' for pointer types
typedef struct {
    ir_type_t *inner;
    bool is_volatile;
} ir_type_extra_pointer_t;

// ---------------- ir_type_extra_composite_t ----------------
// Structure for 'extra' field of 'ir_type_t' for composite
// types such as structures
typedef struct {
    ir_type_t **subtypes;
    length_t subtypes_length;
    trait_t traits;
} ir_type_extra_composite_t;

// Possible traits for ir_type_extra_composite_t
#define TYPE_KIND_COMPOSITE_PACKED TRAIT_1

// ---------------- ir_type_extra_fixed_array_t ----------------
// Structure for 'extra' field of 'ir_type_t' for fixed arrays
typedef struct {
    ir_type_t *subtype;
    length_t length;
} ir_type_extra_fixed_array_t;

// ---------------- ir_type_extra_unknown_enum_t ----------------
// Structure for 'extra' field of 'ir_type_t' for unknown enum types
typedef struct {
    source_t source;
    weak_cstr_t kind_name;
} ir_type_extra_unknown_enum_t;

// ---------------- ir_type_str ----------------
// Generates a c-string representation from
// an intermediate representation type
strong_cstr_t ir_type_str(ir_type_t *type);

// ---------------- ir_types_identical ----------------
// Returns whether two IR types are identical
bool ir_types_identical(ir_type_t *a, ir_type_t *b);

// ---------------- ir_type_make ----------------
// Makes an IR type
ir_type_t *ir_type_make(ir_pool_t *pool, unsigned int kind, void *extra_data);

// ---------------- ir_type_make_pointer_to ----------------
// Gets the type of a pointer to a type
ir_type_t *ir_type_make_pointer_to(ir_pool_t *pool, ir_type_t *base, bool is_volatile);

// ---------------- ir_type_make_fixed_array_of ----------------
// Gets the type of a fixed array of a type
ir_type_t *ir_type_make_fixed_array_of(ir_pool_t *pool, length_t length, ir_type_t *base);

// ---------------- ir_type_make_function_pointer ----------------
// Gets the type for a function pointer
ir_type_t *ir_type_make_function_pointer(ir_pool_t *pool);

// ---------------- ir_type_make_unknown_enum ----------------
// Gets the type for a value of an unknown enum
ir_type_t *ir_type_make_unknown_enum(ir_pool_t *pool, source_t source, weak_cstr_t kind_name);

// ---------------- ir_type_dereference ----------------
// Gets the type pointed to by a pointer type
ir_type_t *ir_type_dereference(ir_type_t *type);

// ---------------- ir_type_unwrap ----------------
// Gets the inner type for a wrapped type
// Returns the original type if not a wrapped type
ir_type_t *ir_type_unwrap(ir_type_t *type);

// ---------------- ir_type_is_pointer_to ----------------
// Returns whether an IR type is a pointer to a type of a specific kind
bool ir_type_is_pointer_to(ir_type_t *type, unsigned int child_type_kind);

// ---------------- global_type_kind_sizes_in_bits_64 ----------------
// Contains the general sizes of each TYPE_KIND_*
// (For 64 bit systems only)
extern unsigned int global_type_kind_sizes_in_bits_64[];

// ---------------- global_type_kind_signs ----------------
// Contains whether each TYPE_KIND_* is generally signed
extern bool global_type_kind_signs[];

// ---------------- global_type_kind_is_integer ----------------
// Contains whether each TYPE_KIND_* is an integer value
// Does not include booleans
extern bool global_type_kind_is_integer[];

// ---------------- global_type_kind_is_float ----------------
// Contains whether each TYPE_KIND_* is a floating point value
extern bool global_type_kind_is_float[];

#endif // _ISAAC_IR_TYPE_H
