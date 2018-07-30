
#ifndef IR_H
#define IR_H

/*
    =================================== ir.h ===================================
    Module for creating and manipulating intermediate representation
    ----------------------------------------------------------------------------
*/

#include "ast.h"
#include "trait.h"
#include "ground.h"

// =============================================================
// ---------------- Possible IR instruction IDs ----------------
// =============================================================
#define INSTRUCTION_NONE          0x00000000
#define INSTRUCTION_ADD           0x00000001 // ir_instr_math_t
#define INSTRUCTION_FADD          0x00000002 // ir_instr_math_t
#define INSTRUCTION_SUBTRACT      0x00000003 // ir_instr_math_t
#define INSTRUCTION_FSUBTRACT     0x00000004 // ir_instr_math_t
#define INSTRUCTION_MULTIPLY      0x00000005 // ir_instr_math_t
#define INSTRUCTION_FMULTIPLY     0x00000006 // ir_instr_math_t
#define INSTRUCTION_UDIVIDE       0x00000007 // ir_instr_math_t
#define INSTRUCTION_SDIVIDE       0x00000008 // ir_instr_math_t
#define INSTRUCTION_FDIVIDE       0x00000009 // ir_instr_math_t
#define INSTRUCTION_UMODULUS      0x0000000A // ir_instr_math_t
#define INSTRUCTION_SMODULUS      0x0000000B // ir_instr_math_t
#define INSTRUCTION_FMODULUS      0x0000000C // ir_instr_math_t
#define INSTRUCTION_RET           0x0000000D
#define INSTRUCTION_CALL          0x0000000E
#define INSTRUCTION_CALL_ADDRESS  0x0000000F
#define INSTRUCTION_ALLOC         0x00000010
#define INSTRUCTION_MALLOC        0x00000011
#define INSTRUCTION_FREE          0x00000012
#define INSTRUCTION_STORE         0x00000013
#define INSTRUCTION_LOAD          0x00000014
#define INSTRUCTION_VARPTR        0x00000015
#define INSTRUCTION_GLOBALVARPTR  0x00000016
#define INSTRUCTION_BREAK         0x00000017
#define INSTRUCTION_CONDBREAK     0x00000018
#define INSTRUCTION_EQUALS        0x00000019 // ir_instr_math_t
#define INSTRUCTION_FEQUALS       0x0000001A // ir_instr_math_t
#define INSTRUCTION_NOTEQUALS     0x0000001B // ir_instr_math_t
#define INSTRUCTION_FNOTEQUALS    0x0000001C // ir_instr_math_t
#define INSTRUCTION_UGREATER      0x0000001D // ir_instr_math_t
#define INSTRUCTION_SGREATER      0x0000001E // ir_instr_math_t
#define INSTRUCTION_FGREATER      0x0000001F // ir_instr_math_t
#define INSTRUCTION_ULESSER       0x00000020 // ir_instr_math_t
#define INSTRUCTION_SLESSER       0x00000021 // ir_instr_math_t
#define INSTRUCTION_FLESSER       0x00000022 // ir_instr_math_t
#define INSTRUCTION_UGREATEREQ    0x00000023 // ir_instr_math_t
#define INSTRUCTION_SGREATEREQ    0x00000024 // ir_instr_math_t
#define INSTRUCTION_FGREATEREQ    0x00000025 // ir_instr_math_t
#define INSTRUCTION_ULESSEREQ     0x00000026 // ir_instr_math_t
#define INSTRUCTION_SLESSEREQ     0x00000027 // ir_instr_math_t
#define INSTRUCTION_FLESSEREQ     0x00000028 // ir_instr_math_t
#define INSTRUCTION_MEMBER        0x00000029
#define INSTRUCTION_ARRAY_ACCESS  0x0000002A
#define INSTRUCTION_FUNC_ADDRESS  0x0000002B // ir_instr_cast_t
#define INSTRUCTION_BITCAST       0x0000002C // ir_instr_cast_t
#define INSTRUCTION_ZEXT          0x0000002D // ir_instr_cast_t
#define INSTRUCTION_TRUNC         0x0000002E // ir_instr_cast_t
#define INSTRUCTION_FEXT          0x0000002F // ir_instr_cast_t
#define INSTRUCTION_FTRUNC        0x00000030 // ir_instr_cast_t
#define INSTRUCTION_INTTOPTR      0x00000031 // ir_instr_cast_t
#define INSTRUCTION_PTRTOINT      0x00000032 // ir_instr_cast_t
#define INSTRUCTION_FPTOUI        0x00000033 // ir_instr_cast_t
#define INSTRUCTION_FPTOSI        0x00000034 // ir_instr_cast_t
#define INSTRUCTION_UITOFP        0x00000035 // ir_instr_cast_t
#define INSTRUCTION_SITOFP        0x00000036 // ir_instr_cast_t
#define INSTRUCTION_ISZERO        0x00000037 // ir_instr_cast_t
#define INSTRUCTION_ISNTZERO      0x00000038 // ir_instr_cast_t
#define INSTRUCTION_REINTERPRET   0x00000039 // ir_instr_cast_t
#define INSTRUCTION_AND           0x00000040 // ir_instr_math_t
#define INSTRUCTION_OR            0x00000041 // ir_instr_math_t
#define INSTRUCTION_SIZEOF        0x00000042

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

// =============================================================
// ------------------ Possible IR value types ------------------
// =============================================================
#define VALUE_TYPE_NONE    0x00000000
#define VALUE_TYPE_LITERAL 0x00000001 // data = pointer to literal value
#define VALUE_TYPE_RESULT  0x00000002 // data = pointer to an 'ir_value_result_t'
#define VALUE_TYPE_NULLPTR 0x00000003

// ---------------- ir_type_t ----------------
// An intermediate representation type
// 'extra' can optionally contain one of the following:
// 'ir_type_t', 'ir_type_extra_composite_t', or 'ir_type_extra_function_t'
typedef struct ir_type_t ir_type_t;

struct ir_type_t {
    unsigned int kind;
    void *extra; //
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

// ---------------- ir_type_mapping_t ----------------
// Mapping for a name to an IR type
typedef struct {
    char *name;
    ir_type_t type;
} ir_type_mapping_t;

// ---------------- ir_type_map_t ----------------
// A list of mappings from names to IR types
typedef struct {
    ir_type_mapping_t *mappings;
    length_t mappings_length;
} ir_type_map_t;

// ---------------- ir_var_t ----------------
// An intermediate representation variable
typedef struct {
    ir_type_t *type;
    trait_t traits;
} ir_var_t;

// Possible traits for ir_var_t
#define IR_VAR_UNDEF TRAIT_1

// ---------------- ir_var_mapping_t ----------------
// Mapping for a name to an IR variable
typedef struct {
    const char *name;
    ir_var_t var;
} ir_var_mapping_t;

// ---------------- ir_var_map_t ----------------
// A list of mappings from names to IR variables
typedef struct {
    ir_var_mapping_t *mappings;
    length_t mappings_length;
    length_t mappings_capacity;
} ir_var_map_t;

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

// ---------------- ir_instr_t ----------------
// General structure for intermediate
// representation instructions
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
} ir_instr_t;

// ---------------- ir_instr_math_t ----------------
// General structure for an IR instruction that
// takes two operands
// (Used for: add, subtract, multiply, divide, modulus, etc.)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *a, *b;
} ir_instr_math_t;

// ---------------- ir_instr_ret_t ----------------
// An IR return instruction
// 'value' can be NULL to return void
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
} ir_instr_ret_t;

// ---------------- ir_instr_call_t ----------------
// An IR call function instruction
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    length_t func_id;
    ir_value_t **values;
    length_t values_length;
} ir_instr_call_t;

// ---------------- ir_instr_call_address_t ----------------
// An IR call function via address instruction
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *address;
    ir_value_t **values;
    length_t values_length;
} ir_instr_call_address_t;

// ---------------- ir_instr_alloc_t ----------------
// An IR instruction for static allocation
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_type_t *type;
    unsigned int amount;
} ir_instr_alloc_t;

// ---------------- ir_instr_malloc_t ----------------
// An IR instruction for dynamic allocation
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_type_t *type;
    ir_value_t *amount;
} ir_instr_malloc_t;

// ---------------- ir_instr_free_t ----------------
// An IR instruction for dynamic deallocation
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
} ir_instr_free_t;

// ---------------- ir_instr_store_t ----------------
// An IR instruction for storing a value into memory
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    ir_value_t *destination;
} ir_instr_store_t;

// ---------------- ir_instr_load_t ----------------
// An IR instruction for loading a value into memory
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
} ir_instr_load_t;

// ---------------- ir_instr_varptr_t ----------------
// An IR pseudo-instruction for getting a pointer to a
// statically allocated variable
// Used for (INSTRUCTION_VARPTR and INSTRUCTION_GLOBALVARPTR)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    length_t index;
} ir_instr_varptr_t;

// ---------------- ir_instr_break_t ----------------
// An IR instruction for breaking/branching to
// another basic block
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    length_t block_id;
} ir_instr_break_t;

// ---------------- ir_instr_cond_break_t ----------------
// An IR instruction for conditionally breaking/branching
// to other basic blocks
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    length_t true_block_id;
    length_t false_block_id;
} ir_instr_cond_break_t;

// ---------------- ir_instr_member_t ----------------
// An IR instruction for accessing a member of
// a data structure via index
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    length_t member;
} ir_instr_member_t;

// ---------------- ir_instr_array_access_t ----------------
// An IR instruction for accessing an element in an array
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    ir_value_t *index;
} ir_instr_array_access_t;

// ---------------- ir_instr_func_address_t ----------------
// An IR instruction for getting the address of a function
// ('name' is only used for foreign implementations)
// ('func_id' is used for domestic implementations)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    const char *name;
    length_t func_id;
} ir_instr_func_address_t;

// ---------------- ir_instr_cast_t ----------------
// An IR instruction for casting values to different types
// (used for: _BITCAST, _ZEXT, _TRUC, _FEXT, _FTRUC etc.)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
} ir_instr_cast_t;

// ---------------- ir_instr_sizeof_t ----------------
// An IR instruction for getting the size of an IR type
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_type_t *type;
} ir_instr_sizeof_t;

// ---------------- ir_basicblock_t ----------------
// An intermediate representation basic block
typedef struct {
    ir_instr_t **instructions;
    length_t instructions_length;
    length_t instructions_capacity;
    trait_t traits;
} ir_basicblock_t;

// ---------------- ir_func_t ----------------
// An intermediate representation function
typedef struct {
    const char *name;
    trait_t traits;
    ir_type_t *return_type;
    ir_type_t **argument_types;
    length_t arity;
    ir_basicblock_t *basicblocks;
    length_t basicblocks_length;
    ir_var_map_t var_map;
} ir_func_t;

// Possible traits for ir_func_t
#define IR_FUNC_FOREIGN TRAIT_1
#define IR_FUNC_VARARG  TRAIT_2
#define IR_FUNC_STDCALL TRAIT_3

// ---------------- ir_func_mapping_t ----------------
// Mapping for a name or id to AST & IR function
// ('name' is only used for foreign implementations)
// ('func_id' is used for domestic implementations)
typedef struct {
    const char *name;
    ast_func_t *ast_func;
    ir_func_t *module_func;
    length_t func_id;
    char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ir_func_mapping_t;

// ---------------- ir_method_t ----------------
// Mapping for a method to an actual function
typedef struct {
    const char *struct_name;
    const char *name;
    ast_func_t *ast_func;
    ir_func_t *module_func;
    length_t func_id;
    char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ir_method_t;

// ---------------- ir_global_t ----------------
// An intermediate representation global variable
typedef struct {
    const char *name;
    ir_type_t *type;
} ir_global_t;

// ---------------- ir_metadata_t ----------------
// General global IR metadata
typedef struct {

} ir_metadata_t;

// ---------------- ir_shared_common_t ----------------
// General data that can be directly accessed by the
// entire IR module
// 'ir_funcptr_type' -> type used for function pointer implementation
typedef struct {
    ir_type_t *ir_funcptr_type;
} ir_shared_common_t;

// ---------------- ir_pool_fragment_t ----------------
// A memory fragment within an 'ir_pool_t'
typedef struct {
    void *memory; // Pointer to memory
    length_t used; // Memory being used (from 0 to n bytes)
    length_t capacity; // Memory block size in bytes
} ir_pool_fragment_t;

// ---------------- ir_pool_t ----------------
// A memory pool containing all allocated
// IR structures for an IR module
typedef struct {
    ir_pool_fragment_t *fragments; // Blocks of memory in which small allocations stored
    length_t fragments_length;
    length_t fragments_capacity;
} ir_pool_t;

// ---------------- ir_pool_snapshot_t ----------------
// A snapshot in time of the current memory usage of
// an 'ir_pool_t'
// Pool snapshots can be used to revert to a previous
// pool allocation state without much overhead. They
// are the only way to partially deallocate memory from a pool.
typedef struct {
    length_t used; // Bytes being used by current fragment
    length_t fragments_length; // Current fragment count
} ir_pool_snapshot_t;

// ---------------- ir_module_t ----------------
// An intermediate representation module
typedef struct {
    ir_shared_common_t common;
    ir_metadata_t metadata;
    ir_pool_t pool;
    ir_type_map_t type_map;
    ir_func_t *funcs;
    ir_func_mapping_t *func_mappings;
    length_t funcs_length;
    ir_method_t *methods;
    length_t methods_length;
    length_t methods_capacity;
    ir_global_t *globals;
    length_t globals_length;
} ir_module_t;

// ---------------- ir_type_str ----------------
// Generates a c-string representation from
// an intermediate representation type
char* ir_type_str(ir_type_t *type);

// ---------------- ir_value_str ----------------
// Generates a c-string representation from
// an intermediate representation value
char* ir_value_str(ir_value_t *value);

// ---------------- ir_type_map_find ----------------
// Finds a type inside an IR type map by name
bool ir_type_map_find(ir_type_map_t *type_map, char *name, ir_type_t **type_ptr);

// ---------------- ir_var_map_find ----------------
// Finds a variable inside an IR variable map by name
bool ir_var_map_find(ir_var_map_t *var_map, char *name, length_t *out_index);

// ---------------- ir_types_identical ----------------
// Returns whether two IR types are identical
bool ir_types_identical(ir_type_t *a, ir_type_t *b);

// ---------------- ir_basicblock_new_instructions ----------------
// Ensures that there is enough room for 'amount' more instructions
void ir_basicblock_new_instructions(ir_basicblock_t *block, length_t amount);

// ---------------- ir_module_dump ----------------
// Generates a string representation from an IR
// module and writes it to a file
void ir_module_dump(ir_module_t *ir_module, const char *filename);

// ---------------- ir_dump_functions (and friends) ----------------
// Dumps a specific part of an IR module
void ir_dump_functions(FILE *file, ir_func_t *functions, length_t functions_length);
void ir_dump_math_instruction(FILE *file, ir_instr_math_t *instruction, int i, const char *instruction_name);
void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, int i);
void ir_dump_call_address_instruction(FILE *file, ir_instr_call_address_t *instruction, int i);

// ---------------- ir_module_free ----------------
// Frees data within an IR module
void ir_module_free(ir_module_t *ir_module);

// ---------------- ir_module_free_funcs ----------------
// Frees data within each IR function in a list
void ir_module_free_funcs(ir_func_t *funcs, length_t funcs_length);

// ---------------- ir_pool_init ----------------
// Initializes an IR memory pool
void ir_pool_init(ir_pool_t *pool);

// ---------------- ir_pool_alloc ----------------
// Allocates memory in an IR memory pool
void* ir_pool_alloc(ir_pool_t *pool, length_t bytes);

// ---------------- ir_pool_free ----------------
// Frees all memory allocated by an IR memory pool
void ir_pool_free(ir_pool_t *pool);

// ---------------- ir_pool_snapshot_capture ----------------
// Captures a snapshot of the current memory used of an IR pool
void ir_pool_snapshot_capture(ir_pool_t *pool, ir_pool_snapshot_t *snapshot);

// ---------------- ir_pool_snapshot_restore ----------------
// Restores an IR pool to a previous memory usage snapshot
void ir_pool_snapshot_restore(ir_pool_t *pool, ir_pool_snapshot_t *snapshot);

// ---------------- global_type_kind_sizes_64 ----------------
// Contains the general sizes of each TYPE_KIND_*
// (For 64 bit systems only)
extern unsigned int global_type_kind_sizes_64[];

// ---------------- global_type_kind_signs ----------------
// Contains whether each TYPE_KIND_* is generally signed
extern bool global_type_kind_signs[];

#endif // IR_H
