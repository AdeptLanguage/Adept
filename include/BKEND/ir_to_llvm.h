
#ifndef IR_TO_LLVM_H
#define IR_TO_LLVM_H

/*
    =============================== ir_to_llvm.h ==============================
    Module for exporting intermediate representation to LLVM
    ---------------------------------------------------------------------------
*/

#include "DRVR/compiler.h"

// ---------------- value_catalog_block_t ----------------
// Contains a list of resulting values from every
// instruction within a basic block
typedef struct { LLVMValueRef *value_references; } value_catalog_block_t;

// ---------------- value_catalog_t ----------------
// Contains a 'value_catalog_block_t' list that
// holds the resulting values from every instruction
// of every block in a function
typedef struct { value_catalog_block_t *blocks; length_t blocks_length; } value_catalog_t;

// ---------------- varstack_t ----------------
// A list of stack variables for a function
typedef struct { LLVMValueRef *values; LLVMTypeRef *types; length_t length; } varstack_t;

// ---------------- llvm_context_t ----------------
// A general container for the LLVM exporting context
typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    value_catalog_t *catalog;
    varstack_t *stack;
    LLVMTypeRef *indexed_types;
    LLVMValueRef *func_skeletons;
    LLVMValueRef *global_variables;
    LLVMValueRef *anon_global_variables;
    LLVMTargetDataRef data_layout;
    LLVMValueRef memcpy_intrinsic;
    LLVMValueRef memset_intrinsic;
    LLVMValueRef stacksave_intrinsic;
    LLVMValueRef stackrestore_intrinsic;
    compiler_t *compiler;
    object_t *object;
    
    // Variables only used for compilation with null checks
    LLVMBasicBlockRef null_check_on_fail_block;
    LLVMValueRef line_phi;
    LLVMValueRef column_phi;
    LLVMValueRef null_check_failure_message_bytes;
    bool has_null_check_failure_message_bytes;

    // Variables only used for generating type map
    bool should_name_structs;
    length_t struct_index;
} llvm_context_t;

typedef struct {
    LLVMValueRef phi;
    LLVMValueRef a;
    LLVMValueRef b;
    length_t basicblock_a;
    length_t basicblock_b;
} llvm_phi2_relocation_t;

// ---------------- ir_to_llvm_type ----------------
// Converts an IR type to an LLVM type
LLVMTypeRef ir_to_llvm_type(llvm_context_t *llvm, ir_type_t *ir_type);

// ---------------- ir_to_llvm_value ----------------
// Converts an IR value to an LLVM value
LLVMValueRef ir_to_llvm_value(llvm_context_t *llvm, ir_value_t *value);

// ---------------- ir_to_llvm_functions ----------------
// Generates LLVM function skeletons for IR functions
errorcode_t ir_to_llvm_functions(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_function_bodies ----------------
// Generates LLVM function bodies for IR functions
errorcode_t ir_to_llvm_function_bodies(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_globals ----------------
// Generates LLVM globals for IR globals
errorcode_t ir_to_llvm_globals(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_null_check ----------------
// Generates LLVM instructions to check for null pointer
void ir_to_llvm_null_check(llvm_context_t *llvm, length_t func_skeleton_index, LLVMValueRef pointer, int line, int column, LLVMBasicBlockRef *out_landing_basicblock);

// ---------------- ir_to_llvm_config_optlvl ----------------
// Converts optimization level to LLVM optimization constant
LLVMCodeGenOptLevel ir_to_llvm_config_optlvl(compiler_t *compiler);

#endif // IR_TO_LLVM_H
