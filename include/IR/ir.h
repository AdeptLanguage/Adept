
#ifndef _ISAAC_IR_H
#define _ISAAC_IR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    =================================== ir.h ===================================
    Module for creating and manipulating intermediate representation
    ----------------------------------------------------------------------------
*/

#include <stdbool.h>
#include <stdio.h>

#include "AST/ast_type_lean.h"
#include "BRIDGE/bridge.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_pool.h"
#include "IR/ir_proc_map.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_cache.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

// =============================================================
// ---------------- Possible IR instruction IDs ----------------
// =============================================================
enum {
    INSTRUCTION_NONE,           
    INSTRUCTION_ADD,             // ir_instr_math_t
    INSTRUCTION_FADD,            // ir_instr_math_t
    INSTRUCTION_SUBTRACT,        // ir_instr_math_t
    INSTRUCTION_FSUBTRACT,       // ir_instr_math_t
    INSTRUCTION_MULTIPLY,        // ir_instr_math_t
    INSTRUCTION_FMULTIPLY,       // ir_instr_math_t
    INSTRUCTION_UDIVIDE,         // ir_instr_math_t
    INSTRUCTION_SDIVIDE,         // ir_instr_math_t
    INSTRUCTION_FDIVIDE,         // ir_instr_math_t
    INSTRUCTION_UMODULUS,        // ir_instr_math_t
    INSTRUCTION_SMODULUS,        // ir_instr_math_t
    INSTRUCTION_FMODULUS,        // ir_instr_math_t
    INSTRUCTION_RET,            
    INSTRUCTION_CALL,           
    INSTRUCTION_CALL_ADDRESS,   
    INSTRUCTION_ALLOC,           // ir_instr_alloc_t
    INSTRUCTION_MALLOC,         
    INSTRUCTION_FREE,           
    INSTRUCTION_STORE,          
    INSTRUCTION_LOAD,           
    INSTRUCTION_VARPTR,          // ir_instr_varptr_t
    INSTRUCTION_GLOBALVARPTR,    // ir_instr_varptr_t
    INSTRUCTION_BREAK,          
    INSTRUCTION_CONDBREAK,      
    INSTRUCTION_EQUALS,          // ir_instr_math_t
    INSTRUCTION_FEQUALS,         // ir_instr_math_t
    INSTRUCTION_NOTEQUALS,       // ir_instr_math_t
    INSTRUCTION_FNOTEQUALS,      // ir_instr_math_t
    INSTRUCTION_UGREATER,        // ir_instr_math_t
    INSTRUCTION_SGREATER,        // ir_instr_math_t
    INSTRUCTION_FGREATER,        // ir_instr_math_t
    INSTRUCTION_ULESSER,         // ir_instr_math_t
    INSTRUCTION_SLESSER,         // ir_instr_math_t
    INSTRUCTION_FLESSER,         // ir_instr_math_t
    INSTRUCTION_UGREATEREQ,      // ir_instr_math_t
    INSTRUCTION_SGREATEREQ,      // ir_instr_math_t
    INSTRUCTION_FGREATEREQ,      // ir_instr_math_t
    INSTRUCTION_ULESSEREQ,       // ir_instr_math_t
    INSTRUCTION_SLESSEREQ,       // ir_instr_math_t
    INSTRUCTION_FLESSEREQ,       // ir_instr_math_t
    INSTRUCTION_MEMBER,         
    INSTRUCTION_ARRAY_ACCESS,   
    INSTRUCTION_BITCAST,         // ir_instr_cast_t
    INSTRUCTION_ZEXT,            // ir_instr_cast_t
    INSTRUCTION_SEXT,            // ir_instr_cast_t
    INSTRUCTION_TRUNC,           // ir_instr_cast_t
    INSTRUCTION_FEXT,            // ir_instr_cast_t
    INSTRUCTION_FTRUNC,          // ir_instr_cast_t
    INSTRUCTION_INTTOPTR,        // ir_instr_cast_t
    INSTRUCTION_PTRTOINT,        // ir_instr_cast_t
    INSTRUCTION_FPTOUI,          // ir_instr_cast_t
    INSTRUCTION_FPTOSI,          // ir_instr_cast_t
    INSTRUCTION_UITOFP,          // ir_instr_cast_t
    INSTRUCTION_SITOFP,          // ir_instr_cast_t
    INSTRUCTION_ISZERO,          // ir_instr_unary_t
    INSTRUCTION_ISNTZERO,        // ir_instr_unary_t
    INSTRUCTION_REINTERPRET,     // ir_instr_cast_t
    INSTRUCTION_AND,             // ir_instr_math_t
    INSTRUCTION_OR,              // ir_instr_math_t
    INSTRUCTION_SIZEOF,         
    INSTRUCTION_OFFSETOF,       
    INSTRUCTION_ZEROINIT,       
    INSTRUCTION_MEMCPY,         
    INSTRUCTION_BIT_AND,         // ir_instr_math_t
    INSTRUCTION_BIT_OR,          // ir_instr_math_t
    INSTRUCTION_BIT_XOR,         // ir_instr_math_t
    INSTRUCTION_BIT_COMPLEMENT,  // ir_instr_unary_t
    INSTRUCTION_BIT_LSHIFT,      // ir_instr_math_t
    INSTRUCTION_BIT_RSHIFT,      // ir_instr_math_t
    INSTRUCTION_BIT_LGC_RSHIFT,  // ir_instr_math_t
    INSTRUCTION_NEGATE,          // ir_instr_unary_t
    INSTRUCTION_FNEGATE,         // ir_instr_unary_t
    INSTRUCTION_SELECT,         
    INSTRUCTION_PHI2,           
    INSTRUCTION_SWITCH,         
    INSTRUCTION_STACK_SAVE,     
    INSTRUCTION_STACK_RESTORE,   // ir_instr_unary_t
    INSTRUCTION_VA_START,        // ir_instr_unary_t
    INSTRUCTION_VA_END,          // ir_instr_unary_t
    INSTRUCTION_VA_ARG,          // ir_instr_va_arg_t
    INSTRUCTION_VA_COPY,         // ir_instr_va_copy_t
    INSTRUCTION_STATICVARPTR,    // ir_instr_varptr_t
    INSTRUCTION_ASM,             // ir_instr_asm_t
    INSTRUCTION_DEINIT_SVARS,    // ir_instr_t
    INSTRUCTION_UNREACHABLE,     // ir_instr_t
};

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
    func_id_t ir_func_id;
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

// ---------------- ir_instr_asm_t ----------------
// An IR instruction for inline assembly
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

// ---------------- ir_instrs_t ----------------
// List of instructions
typedef listof(ir_instr_t*, instructions) ir_instrs_t;

// ---------------- ir_basicblock_t ----------------
// An intermediate representation basic block
typedef struct {
    ir_instrs_t instructions;
    trait_t traits;
} ir_basicblock_t;

// ---------------- ir_basicblocks_t ----------------
// A list of basicblocks
typedef listof(ir_basicblock_t, blocks) ir_basicblocks_t;
void ir_basicblocks_free(ir_basicblocks_t *basicblocks);

// ---------------- ir_vtable_init_t ----------------
// Used to keep track of store instructions that will
// need their value field filled in with a vtable value
// which will be generated later during IR generation
typedef struct {
    ir_instr_store_t *store_instr;
    ast_type_t subject_type;
} ir_vtable_init_t;

// ---------------- ir_vtable_init_free ----------------
// Frees a vtable initialization
void ir_vtable_init_free(ir_vtable_init_t *vtable_init);

// ---------------- ir_vtable_init_list_t ----------------
// A list of required vtable initializations
typedef listof(ir_vtable_init_t, initializations) ir_vtable_init_list_t;

// ---------------- ir_vtable_init_list_append ----------------
// Appends a vtable initialization to a vtable initialization list
#define ir_vtable_init_list_append(LIST, VALUE) list_append((LIST), (VALUE), ir_vtable_init_t)

// ---------------- ir_vtable_init_list_free ----------------
// Frees the vtable initializations in a vtable initialization list
// and then the list itself
void ir_vtable_init_list_free(ir_vtable_init_list_t *vtable_init_list);

// ---------------- ir_vtable_dispatch_t ----------------
// Used to keep track of where vtable index injection will be required
typedef struct {
    func_id_t ast_func_id;
    ir_value_t *index_value;
} ir_vtable_dispatch_t;

// ---------------- ir_vtable_dispatch_list_t ----------------
// A list of required vtable dispatches
typedef listof(ir_vtable_dispatch_t, dispatches) ir_vtable_dispatch_list_t;

// ---------------- ir_vtable_dispatch_list_append ----------------
// Appends a vtable dispatch to a vtable dispatch list
#define ir_vtable_dispatch_list_append(LIST, VALUE) list_append((LIST), (VALUE), ir_vtable_dispatch_t)

// ---------------- ir_vtable_dispatch_list_free ----------------
// Frees a vtable dispatch list
#define ir_vtable_dispatch_list_free(LIST) free((LIST)->dispatches)

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
    ir_basicblocks_t basicblocks;
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

// ---------------- ir_job_list_t ----------------
// List of jobs required during IR generation
typedef listof(ir_func_endpoint_t, jobs) ir_job_list_t;

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
    func_id_t variadic_ir_func_id;         // NOTE: Only exists if 'ir_variadic_array' isn't null
    bool has_main;
    func_id_t ast_main_id;
    func_id_t ir_main_id;
} ir_shared_common_t;

// ---------------- ir_static_variable_t ----------------
// A static variable in an IR module
typedef struct {
    ir_type_t *type;
} ir_static_variable_t;

// ---------------- ir_static_variables_t ----------------
// List of static variables
typedef listof(ir_static_variable_t, variables) ir_static_variables_t;
#define ir_static_variables_append(LIST, VALUE) list_append((LIST), (VALUE), ir_static_variable_t)
void ir_static_variables_free(ir_static_variables_t *static_variables);

// ---------------- free_list_t ----------------
// List of pointers to free
typedef listof(void*, pointers) free_list_t;
#define free_list_append(LIST, VALUE) list_append((LIST), (VALUE), void*)
void free_list_free(free_list_t *list);

// ---------------- rtti_relocation_t ----------------
// List of RTTI relocations
typedef listof(rtti_relocation_t, relocations) rtti_relocations_t;
#define rtti_relocations_append(LIST, VALUE) list_append((LIST), (VALUE), rtti_relocation_t)
void rtti_relocations_free(rtti_relocations_t *relocations);

// ---------------- ir_anon_globals_append ----------------
// List of anonymous global variables
typedef listof(ir_anon_global_t, globals) ir_anon_globals_t;
#define ir_anon_globals_append(LIST, VALUE) list_append((LIST), (VALUE), ir_anon_global_t)

// ---------------- ir_funcs_t ----------------
// List of functions
typedef listof(ir_func_t, funcs) ir_funcs_t;
#define ir_funcs_append(LIST, VALUE) list_append((LIST), (VALUE), ir_func_t)

// ---------------- ir_basicblock_new_instructions ----------------
// Ensures that there is enough room for 'amount' more instructions
void ir_basicblock_new_instructions(ir_basicblock_t *block, length_t amount);

// ---------------- ir_basicblock_free ----------------
// Frees an IR basicblock
void ir_basicblock_free(ir_basicblock_t *basicblock);

// ---------------- ir_funcs_free ----------------
// Frees a list of IR functions
void ir_funcs_free(ir_funcs_t funcs);

// ---------------- ir_implementation_setup ----------------
// Prepares for calls to ir_implementation()
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

// ---------------- ir_job_list_append ----------------
// Appends a mapping to an IR job list
#define ir_job_list_append(LIST, VALUE) list_append((LIST), (VALUE), ir_func_endpoint_t)

// ---------------- ir_job_list_free ----------------
// Frees an IR job list
void ir_job_list_free(ir_job_list_t *job_list);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_H
