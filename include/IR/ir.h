
#ifndef _ISAAC_IR_H
#define _ISAAC_IR_H

/*
    =================================== ir.h ===================================
    Module for creating and manipulating intermediate representation
    ----------------------------------------------------------------------------
*/

#include "AST/ast.h"
#include "UTIL/trait.h"
#include "UTIL/ground.h"
#include "UTIL/datatypes.h"
#include "BRIDGE/bridge.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_cache.h"

// =============================================================
// ---------------- Possible IR instruction IDs ----------------
// =============================================================
#define INSTRUCTION_NONE           0x00000000
#define INSTRUCTION_ADD            0x00000001 // ir_instr_math_t
#define INSTRUCTION_FADD           0x00000002 // ir_instr_math_t
#define INSTRUCTION_SUBTRACT       0x00000003 // ir_instr_math_t
#define INSTRUCTION_FSUBTRACT      0x00000004 // ir_instr_math_t
#define INSTRUCTION_MULTIPLY       0x00000005 // ir_instr_math_t
#define INSTRUCTION_FMULTIPLY      0x00000006 // ir_instr_math_t
#define INSTRUCTION_UDIVIDE        0x00000007 // ir_instr_math_t
#define INSTRUCTION_SDIVIDE        0x00000008 // ir_instr_math_t
#define INSTRUCTION_FDIVIDE        0x00000009 // ir_instr_math_t
#define INSTRUCTION_UMODULUS       0x0000000A // ir_instr_math_t
#define INSTRUCTION_SMODULUS       0x0000000B // ir_instr_math_t
#define INSTRUCTION_FMODULUS       0x0000000C // ir_instr_math_t
#define INSTRUCTION_RET            0x0000000D
#define INSTRUCTION_CALL           0x0000000E
#define INSTRUCTION_CALL_ADDRESS   0x0000000F
#define INSTRUCTION_ALLOC          0x00000010 // ir_instr_alloc_t
#define INSTRUCTION_MALLOC         0x00000011
#define INSTRUCTION_FREE           0x00000012
#define INSTRUCTION_STORE          0x00000013
#define INSTRUCTION_LOAD           0x00000014
#define INSTRUCTION_VARPTR         0x00000015
#define INSTRUCTION_GLOBALVARPTR   0x00000016
#define INSTRUCTION_BREAK          0x00000017
#define INSTRUCTION_CONDBREAK      0x00000018
#define INSTRUCTION_EQUALS         0x00000019 // ir_instr_math_t
#define INSTRUCTION_FEQUALS        0x0000001A // ir_instr_math_t
#define INSTRUCTION_NOTEQUALS      0x0000001B // ir_instr_math_t
#define INSTRUCTION_FNOTEQUALS     0x0000001C // ir_instr_math_t
#define INSTRUCTION_UGREATER       0x0000001D // ir_instr_math_t
#define INSTRUCTION_SGREATER       0x0000001E // ir_instr_math_t
#define INSTRUCTION_FGREATER       0x0000001F // ir_instr_math_t
#define INSTRUCTION_ULESSER        0x00000020 // ir_instr_math_t
#define INSTRUCTION_SLESSER        0x00000021 // ir_instr_math_t
#define INSTRUCTION_FLESSER        0x00000022 // ir_instr_math_t
#define INSTRUCTION_UGREATEREQ     0x00000023 // ir_instr_math_t
#define INSTRUCTION_SGREATEREQ     0x00000024 // ir_instr_math_t
#define INSTRUCTION_FGREATEREQ     0x00000025 // ir_instr_math_t
#define INSTRUCTION_ULESSEREQ      0x00000026 // ir_instr_math_t
#define INSTRUCTION_SLESSEREQ      0x00000027 // ir_instr_math_t
#define INSTRUCTION_FLESSEREQ      0x00000028 // ir_instr_math_t
#define INSTRUCTION_MEMBER         0x00000029
#define INSTRUCTION_ARRAY_ACCESS   0x0000002A
#define INSTRUCTION_FUNC_ADDRESS   0x0000002B // ir_instr_cast_t
#define INSTRUCTION_BITCAST        0x0000002C // ir_instr_cast_t
#define INSTRUCTION_ZEXT           0x0000002D // ir_instr_cast_t
#define INSTRUCTION_SEXT           0x0000002E // ir_instr_cast_t
#define INSTRUCTION_TRUNC          0x0000002F // ir_instr_cast_t
#define INSTRUCTION_FEXT           0x00000030 // ir_instr_cast_t
#define INSTRUCTION_FTRUNC         0x00000031 // ir_instr_cast_t
#define INSTRUCTION_INTTOPTR       0x00000032 // ir_instr_cast_t
#define INSTRUCTION_PTRTOINT       0x00000033 // ir_instr_cast_t
#define INSTRUCTION_FPTOUI         0x00000034 // ir_instr_cast_t
#define INSTRUCTION_FPTOSI         0x00000035 // ir_instr_cast_t
#define INSTRUCTION_UITOFP         0x00000036 // ir_instr_cast_t
#define INSTRUCTION_SITOFP         0x00000037 // ir_instr_cast_t
#define INSTRUCTION_ISZERO         0x00000038 // ir_unary_t
#define INSTRUCTION_ISNTZERO       0x00000039 // ir_unary_t
#define INSTRUCTION_REINTERPRET    0x00000040 // ir_instr_cast_t
#define INSTRUCTION_AND            0x00000041 // ir_instr_math_t
#define INSTRUCTION_OR             0x00000042 // ir_instr_math_t
#define INSTRUCTION_SIZEOF         0x00000043
#define INSTRUCTION_OFFSETOF       0x00000044
#define INSTRUCTION_ZEROINIT       0x00000045
#define INSTRUCTION_MEMCPY         0x00000046
#define INSTRUCTION_BIT_AND        0x00000047 // ir_instr_math_t
#define INSTRUCTION_BIT_OR         0x00000048 // ir_instr_math_t
#define INSTRUCTION_BIT_XOR        0x00000049 // ir_instr_math_t
#define INSTRUCTION_BIT_COMPLEMENT 0x0000004A // ir_instr_unary_t
#define INSTRUCTION_BIT_LSHIFT     0x0000004B // ir_instr_math_t
#define INSTRUCTION_BIT_RSHIFT     0x0000004C // ir_instr_math_t
#define INSTRUCTION_BIT_LGC_RSHIFT 0x0000004D // ir_instr_math_t
#define INSTRUCTION_NEGATE         0x0000004E // ir_instr_unary_t
#define INSTRUCTION_FNEGATE        0x0000004F // ir_instr_unary_t
#define INSTRUCTION_SELECT         0x00000050
#define INSTRUCTION_PHI2           0x00000051
#define INSTRUCTION_SWITCH         0x00000052
#define INSTRUCTION_STACK_SAVE     0x00000053
#define INSTRUCTION_STACK_RESTORE  0x00000054 // ir_instr_stack_restore_t
#define INSTRUCTION_VA_START       0x00000055 // ir_instr_unary_t
#define INSTRUCTION_VA_END         0x00000056 // ir_instr_unary_t
#define INSTRUCTION_VA_ARG         0x00000057 // ir_instr_va_arg_t
#define INSTRUCTION_VA_COPY        0x00000058 // ir_instr_va_copy_t
#define INSTRUCTION_STATICVARPTR   0x00000059 // ir_instr_varptr_t
#define INSTRUCTION_ASM            0x0000005A // ir_instr_asm_t
#define INSTRUCTION_DEINIT_SVARS   0x0000005B // ir_instr_t

// ---------------- ir_type_mapping_t ----------------
// Mapping for a name to an IR type
typedef struct {
    weak_cstr_t name;
    ir_type_t type;
} ir_type_mapping_t;

// ---------------- ir_type_map_t ----------------
// A list of mappings from names to IR types
typedef struct {
    ir_type_mapping_t *mappings;
    length_t mappings_length;
} ir_type_map_t;

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

// ---------------- ir_instr_unary_t ----------------
// General structure for an IR instruction that
// takes two operands
// (Used for: bit complement, negate, fnegate, iszero, isntzero)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
} ir_instr_unary_t;

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
    funcid_t ir_func_id;
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
    unsigned int alignment;
    ir_value_t *count;
} ir_instr_alloc_t;

// ---------------- ir_instr_malloc_t ----------------
// An IR instruction for dynamic allocation
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_type_t *type;
    ir_value_t *amount;
    bool is_undef;
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
    int maybe_line_number;
    int maybe_column_number;
} ir_instr_store_t;

// ---------------- ir_instr_load_t ----------------
// An IR instruction for loading a value into memory
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    int maybe_line_number;
    int maybe_column_number;
} ir_instr_load_t;

// ---------------- ir_instr_varptr_t ----------------
// An IR pseudo-instruction for getting a pointer to a
// statically allocated variable
// Used for (INSTRUCTION_VARPTR and INSTRUCTION_GLOBALVARPTR and INSTRUCTION_STATICVARPTR)
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
    int maybe_line_number;
    int maybe_column_number;
} ir_instr_member_t;

// ---------------- ir_instr_array_access_t ----------------
// An IR instruction for accessing an element in an array
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *value;
    ir_value_t *index;
    int maybe_line_number;
    int maybe_column_number;
} ir_instr_array_access_t;

// ---------------- ir_instr_func_address_t ----------------
// An IR instruction for getting the address of a function
// ('name' is only used for foreign implementations)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    const char *name;
    funcid_t ir_func_id;
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

// ---------------- ir_instr_offsetof_t ----------------
// An IR instruction for getting the offset of an element within an IR struct
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_type_t *type;
    length_t index;
} ir_instr_offsetof_t;

// ---------------- ir_instr_zeroinit_t ----------------
// An IR instruction for zero-initializing a value
typedef struct {
    unsigned int id;
    ir_value_t *destination;
} ir_instr_zeroinit_t;

// ---------------- ir_instr_memcpy_t ----------------
// An IR instruction for copying chunks of memory from one place to another
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *destination;
    ir_value_t *value;
    ir_value_t *bytes;
    bool is_volatile;
} ir_instr_memcpy_t;

// ---------------- ir_instr_stack_restore_t ----------------
// An IR instruction for restoring the stack pointer
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *stack_pointer;
} ir_instr_stack_restore_t;

// ---------------- ir_instr_stack_restore_t ----------------
// An IR instruction for invoking 'va_arg'
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *va_list;
} ir_instr_va_arg_t;

// ---------------- ir_instr_select_t ----------------
// An IR instruction for conditionally selecting between two values
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *condition;
    ir_value_t *if_true;
    ir_value_t *if_false;
} ir_instr_select_t;

// ---------------- ir_instr_phi2_t ----------------
// IR instruction for general two point phi
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *a;
    ir_value_t *b;
    length_t block_id_a;
    length_t block_id_b;
} ir_instr_phi2_t;

// ---------------- ir_instr_switch_t ----------------
// IR instruction for general switch statement
// NOTE: Default case exists if (default_block_id != resume_block_id)
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *condition;
    ir_value_t **case_values;
    length_t *case_block_ids;
    length_t cases_length;
    length_t default_block_id;
    length_t resume_block_id;
} ir_instr_switch_t;

// ---------------- ir_instr_va_copy_t ----------------
// An IR instruction for invoking 'va_copy'
typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    ir_value_t *dest_value;
    ir_value_t *src_value;
} ir_instr_va_copy_t;

typedef struct {
    unsigned int id;
    ir_type_t *result_type;
    weak_cstr_t assembly;
    weak_cstr_t constraints;
    ir_value_t **args;
    length_t arity;
    bool is_intel;
    bool has_side_effects;
    bool is_stack_align;
} ir_instr_asm_t;

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
    weak_cstr_t name;
    weak_cstr_t maybe_filename;
    weak_cstr_t maybe_definition_string;
    length_t maybe_line_number;
    length_t maybe_column_number;
    trait_t traits;
    ir_type_t *return_type;
    ir_type_t **argument_types;
    length_t arity;
    ir_basicblock_t *basicblocks;
    length_t basicblocks_length;
    bridge_scope_t *scope;
    length_t variable_count;
    weak_cstr_t export_as;
} ir_func_t;

// Possible traits for ir_func_t
#define IR_FUNC_FOREIGN     TRAIT_1
#define IR_FUNC_VARARG      TRAIT_2
#define IR_FUNC_MAIN        TRAIT_3
#define IR_FUNC_STDCALL     TRAIT_4
#define IR_FUNC_POLYMORPHIC TRAIT_5

// ---------------- ir_func_mapping_t ----------------
// Mapping for a name or id to AST & IR function
// ('name' is only used for foreign implementations)
// ('ast_func_id' is used for domestic implementations)
typedef struct {
    const char *name;
    funcid_t ir_func_id;
    funcid_t ast_func_id;
    signed char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ir_func_mapping_t;

// ---------------- ir_method_t ----------------
// Mapping for a method to an actual function
typedef struct {
    const char *struct_name;
    const char *name;
    funcid_t ir_func_id;
    funcid_t ast_func_id;
    signed char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ir_method_t;

// ---------------- ir_generic_base_method_t ----------------
// Mapping for a generic base method to an actual function
typedef struct {
    weak_cstr_t generic_base;
    ast_type_t *generics;
    length_t generics_length;
    weak_cstr_t name;
    funcid_t ir_func_id;
    funcid_t ast_func_id;
    signed char is_beginning_of_group; // 1 == yes, 0 == no, -1 == uncalculated
} ir_generic_base_method_t;

// ---------------- ir_job_list_t ----------------
// List of jobs required during IR generation
typedef struct {
    ir_func_mapping_t *jobs;
    length_t length;
    length_t capacity;
} ir_job_list_t;

// ---------------- ir_global_t ----------------
// An intermediate representation global variable
typedef struct {
    weak_cstr_t name;
    ir_type_t *type;
    trait_t traits;

    // Non-user static value initializer
    // (Used for __types__ and __types_length__)
    ir_value_t *trusted_static_initializer;
} ir_global_t;

#define IR_GLOBAL_EXTERNAL     TRAIT_1
#define IR_GLOBAL_THREAD_LOCAL TRAIT_2

// ---------------- ir_anon_global_t ----------------
// An intermediate representation anonymous global variable
typedef struct {
    ir_type_t *type;
    ir_value_t *initializer;
    trait_t traits;
} ir_anon_global_t;

// Possible traits for 'ir_anon_global_t'
#define IR_ANON_GLOBAL_CONSTANT TRAIT_1

// ---------------- rtti_relocation_t ----------------
// Represents an RTTI index that requires resolution
typedef struct {
    strong_cstr_t human_notation;
    adept_usize *id_ref;
    source_t source_on_failure;
} rtti_relocation_t;

// ---------------- ir_metadata_t ----------------
// General global IR metadata
typedef struct {

} ir_metadata_t;

// ---------------- ir_shared_common_t ----------------
// General data that can be directly accessed by the
// entire IR module
typedef struct {
    ir_type_t *ir_usize;
    ir_type_t *ir_usize_ptr;
    ir_type_t *ir_ptr;
    ir_type_t *ir_bool;
    ir_type_t *ir_string_struct;
    ir_type_t *ir_ubyte;
    length_t rtti_array_index;
    troolean has_rtti_array;
    ir_type_t *ir_variadic_array;         // NOTE: Can be NULL
    funcid_t variadic_ir_func_id;         // NOTE: Only exists if 'ir_variadic_array' isn't null
    bool has_main;
    funcid_t ast_main_id;
    funcid_t ir_main_id;
    length_t next_static_variable_id;
} ir_shared_common_t;

// ---------------- ir_static_variable_t ----------------
// A static variable in an IR module
typedef struct {
    ir_type_t *type;
} ir_static_variable_t;

struct ir_builder;

// ---------------- ir_module_t ----------------
// An intermediate representation module
typedef struct {
    ir_shared_common_t common;
    ir_metadata_t metadata;
    ir_pool_t pool;
    ir_type_map_t type_map;
    ir_func_t *funcs;
    length_t funcs_length;
    length_t funcs_capacity;
    ir_func_mapping_t *func_mappings;
    length_t func_mappings_length;
    length_t func_mappings_capacity;
    ir_method_t *methods;
    length_t methods_length;
    length_t methods_capacity;
    ir_generic_base_method_t *generic_base_methods;
    length_t generic_base_methods_length;
    length_t generic_base_methods_capacity;
    ir_global_t *globals;
    length_t globals_length;
    ir_anon_global_t *anon_globals;
    length_t anon_globals_length;
    length_t anon_globals_capacity;
    ir_gen_sf_cache_t sf_cache;
    rtti_relocation_t *rtti_relocations;
    length_t rtti_relocations_length;
    length_t rtti_relocations_capacity;
    struct ir_builder *init_builder;
    struct ir_builder *deinit_builder;
    ir_static_variable_t *static_variables;
    length_t static_variables_length;
    length_t static_variables_capacity;
} ir_module_t;

// ---------------- ir_value_str ----------------
// Generates a c-string representation from
// an intermediate representation value
strong_cstr_t ir_value_str(ir_value_t *value);

// ---------------- ir_type_map_find ----------------
// Finds a type inside an IR type map by name
successful_t ir_type_map_find(ir_type_map_t *type_map, char *name, ir_type_t **type_ptr);

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
void ir_dump_basicsblocks(FILE *file, ir_basicblock_t *basicblocks, length_t basicblocks_length, ir_func_t *functions);
void ir_dump_math_instruction(FILE *file, ir_instr_math_t *instruction, int i, const char *instruction_name);
void ir_dump_call_instruction(FILE *file, ir_instr_call_t *instruction, int i, const char *real_name);
void ir_dump_call_address_instruction(FILE *file, ir_instr_call_address_t *instruction, int i);
void ir_dump_var_scope_layout(FILE *file, bridge_scope_t *scope);

// ---------------- ir_module_free ----------------
// Initializes an IR module for use
void ir_module_init(ir_module_t *ir_module, length_t funcs_length, length_t globals_length, length_t func_mappings_length_guess);

// ---------------- ir_module_free ----------------
// Frees data within an IR module
void ir_module_free(ir_module_t *ir_module);

// ---------------- ir_basicblock_free ----------------
// Frees an IR basicblock
void ir_basicblock_free(ir_basicblock_t *basicblock);

// ---------------- ir_module_free_funcs ----------------
// Frees data within each IR function in a list
void ir_module_free_funcs(ir_func_t *funcs, length_t funcs_length);

// ---------------- ir_implementation_setup ----------------
// Preprares for calls to ir_implementation()
void ir_implementation_setup();

// ---------------- ir_implementation ----------------
// Encodes an ID for an implementation name
// NOTE: output_buffer is assumed to be able to hold 32 characters
void ir_implementation(length_t id, char prefix, char *output_buffer);

// ---------------- ir_value_uniqueness_value ----------------
// Maps a literal IR value to a uniqueness value.
// If two IR values of the same IR type have the same uniqueness value, then
// they also have the same literal value.
// If two IR values of the same IR type have different uniqueness values, then
// they do not share the same literal value.
unsigned long long ir_value_uniqueness_value(ir_pool_t *pool, ir_value_t **value);

// ---------------- ir_print_value ----------------
// Prints a value to stdout
void ir_print_value(ir_value_t *value);

// ---------------- ir_print_type ----------------
// Prints a type to stdout
void ir_print_type(ir_type_t *type);

// ---------------- ir_module_insert_method ----------------
// Inserts a method into a module's method list
void ir_module_insert_method(ir_module_t *module, weak_cstr_t struct_name, weak_cstr_t method_name, funcid_t ir_func_id, funcid_t ast_func_id, bool preserve_sortedness);

// ---------------- ir_module_insert_generic_method ----------------
// Inserts a generic method into a module's method list
// NOTE: Memory for 'weak_generics' should persist at least as long as
//       the generic method exists
void ir_module_insert_generic_method(ir_module_t *module, 
    weak_cstr_t generic_base,
    ast_type_t *weak_generics,
    length_t generics_length,
    weak_cstr_t name,
    funcid_t ir_func_id,
    funcid_t ast_func_id,
bool preserve_sortedness);

// ---------------- ir_module_insert_generic_method ----------------
// Inserts a new function mapping into a module's function mappings list
ir_func_mapping_t *ir_module_insert_func_mapping(ir_module_t *module, weak_cstr_t name, funcid_t ir_func_id, funcid_t ast_func_id, bool preserve_sortedness);

// ---------------- ir_module_find_insert_method_position ----------------
// Finds the position to insert a method into a module's method list
#define ir_module_find_insert_method_position(module, weak_method_reference) \
    find_insert_position(module->methods, module->methods_length, ir_method_cmp, weak_method_reference, sizeof(ir_method_t));

// ---------------- ir_module_find_insert_generic_method_position ----------------
// Finds the position to insert a method into a module's method list
#define ir_module_find_insert_generic_method_position(module, weak_method_reference) \
    find_insert_position(module->generic_base_methods, module->generic_base_methods_length, ir_generic_base_method_cmp, weak_method_reference, sizeof(ir_generic_base_method_t));

// ---------------- ir_module_find_insert_mapping_position ----------------
// Finds the position to insert a mapping into a module's mappings list
#define ir_module_find_insert_mapping_position(module, weak_mapping_reference) \
    find_insert_position(module->func_mappings, module->func_mappings_length, ir_func_mapping_cmp, weak_mapping_reference, sizeof(ir_func_mapping_t));

// ---------------- ir_func_mapping_cmp ----------------
// Compares two 'ir_func_mapping_t' structures.
// Used for qsort()
int ir_func_mapping_cmp(const void *a, const void *b);

// ---------------- ir_method_cmp ----------------
// Compares two 'ir_method_t' structures.
// Used for qsort()
int ir_method_cmp(const void *a, const void *b);

// ---------------- ir_generic_base_method_cmp ----------------
// Compares two 'ir_generic_base_method_t' structures.
// Used for qsort()
int ir_generic_base_method_cmp(const void *a, const void *b);

#endif // _ISAAC_IR_H
