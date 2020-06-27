
#ifndef IR_TYPE_H
#define IR_TYPE_H

#include "UTIL/trait.h"
#include "UTIL/ground.h"
#include "IR/ir_pool.h"

// ============================================================
// ------------------ Possible IR type kinds ------------------
// ============================================================
#define TYPE_KIND_NONE        0x00000000 // extra = NULL
#define TYPE_KIND_POINTER     0x00000001 // extra = *ir_type_t
#define TYPE_KIND_S8          0x00000002 // extra = NULL
#define TYPE_KIND_S16         0x00000003 // extra = NULL
#define TYPE_KIND_S32         0x00000004 // extra = NULL
#define TYPE_KIND_S64         0x00000005 // extra = NULL
#define TYPE_KIND_U8          0x00000006 // extra = NULL
#define TYPE_KIND_U16         0x00000007 // extra = NULL
#define TYPE_KIND_U32         0x00000008 // extra = NULL
#define TYPE_KIND_U64         0x00000009 // extra = NULL
#define TYPE_KIND_HALF        0x0000000A // extra = NULL
#define TYPE_KIND_FLOAT       0x0000000B // extra = NULL
#define TYPE_KIND_DOUBLE      0x0000000C // extra = NULL
#define TYPE_KIND_BOOLEAN     0x0000000D // extra = NULL
#define TYPE_KIND_UNION       0x0000000E // extra = *ir_type_extra_composite_t
#define TYPE_KIND_STRUCTURE   0x0000000F // extra = *ir_type_extra_composite_t
#define TYPE_KIND_VOID        0x00000010 // extra = NULL
#define TYPE_KIND_FUNCPTR     0x00000011 // extra = *ir_type_extra_function_t
#define TYPE_KIND_FIXED_ARRAY 0x00000012 // extra = *ir_type_extra_fixed_array_t;
#define TYPE_KIND_INDEXED     0x00000013 // extra = ir_type_index_t

// ---------------- ir_type_id_t ----------------
// An ID used to reference an existing IR type that is in the module's type map.
// NOTE: This is equal to the index inside 'ir_module->type_map.mappings' array
//       which the father type being referenced is stored
typedef length_t ir_type_index_t;

// ---------------- ir_type_t ----------------
// An intermediate representation type
// 'extra' can optionally contain one of the following:
// 'ir_type_t', 'ir_type_extra_composite_t', or 'ir_type_extra_function_t'
typedef struct ir_type_t ir_type_t;

// NOTE: '_kind' and '_extra' may not be indicative of the
// actual type represented because type indexing. In order to find
// the real type kind and real extra data, you must call
// 'ir_type_kind()' or 'ir_type_extra()' respectively
struct ir_type_t {
    unsigned int _kind;
    void *_extra;
};

// ---------------- ir_type_indexed_t ----------------
// An intermediate representation type, which has kind TYPE_KIND_INDEXED
// NOTE: This type is made to overlap with ir_type_t, so that an ir_type_t
// that has kind TYPE_KIND_INDEXED can be treated as this structure.
// NOTE: 'father' is used to reference the "real" existing type that this type
// stand in place of
typedef struct ir_type_indexed_t ir_type_indexed_t;

struct ir_type_indexed_t {
    unsigned int _kind;
    ir_type_index_t father;
};

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

// ---------------- ir_type_extra_function_t ----------------
// Structure for 'extra' field of 'ir_type_t' for function
// pointer types
typedef struct {
    ir_type_t **arg_types;
    length_t arity;
    ir_type_t *return_type;
    trait_t traits;
} ir_type_extra_function_t;

// Possible traits for ir_type_extra_function_t
#define TYPE_KIND_FUNC_VARARG  TRAIT_1
#define TYPE_KIND_FUNC_STDCALL TRAIT_2

// ---------------- ir_type_extra_fixed_array_t ----------------
// Structure for 'extra' field of 'ir_type_t' for fixed arrays
typedef struct {
    ir_type_t *subtype;
    length_t length;
} ir_type_extra_fixed_array_t;

// ---------------- ir_type_pointer_to ----------------
// Gets the type of a pointer to a type
ir_type_t* ir_type_pointer_to(ir_pool_t *pool, ir_type_t *base);

// ---------------- global_type_kind_sizes_64 ----------------
// Contains the general sizes of each TYPE_KIND_*
// (For 64 bit systems only)
extern unsigned int global_type_kind_sizes_64[];

// ---------------- global_type_kind_signs ----------------
// Contains whether each TYPE_KIND_* is generally signed
extern bool global_type_kind_signs[];

#endif // IR_TYPE_H
