
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

// ---------------- stack_t ----------------
// A list of stack variables for a function
typedef struct { LLVMValueRef *values; LLVMTypeRef *types; length_t length; } stack_t;

// ---------------- llvm_context_t ----------------
// A general container for the LLVM exporting context
typedef struct {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    value_catalog_t *catalog;
    stack_t *stack;
    LLVMValueRef *func_skeletons;
    LLVMValueRef *global_variables;
    LLVMTargetDataRef data_layout;
    LLVMValueRef memcpy_intrinsic;
} llvm_context_t;

// ---------------- ir_to_llvm_type ----------------
// Converts an IR type to an LLVM type
LLVMTypeRef ir_to_llvm_type(ir_type_t *ir_type);

// ---------------- ir_to_llvm_value ----------------
// Converts an IR value to an LLVM value
LLVMValueRef ir_to_llvm_value(llvm_context_t *llvm, ir_value_t *value);

// ---------------- ir_to_llvm_functions ----------------
// Generates LLVM function skeletons for IR functions
int ir_to_llvm_functions(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_function_bodies ----------------
// Generates LLVM function bodies for IR functions
int ir_to_llvm_function_bodies(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_globals ----------------
// Generates LLVM globals for IR globals
int ir_to_llvm_globals(llvm_context_t *llvm, object_t *object);

// ---------------- ir_to_llvm_config_optlvl ----------------
// Converts optimization level to LLVM optimization constant
LLVMCodeGenOptLevel ir_to_llvm_config_optlvl(compiler_t *compiler);

#endif // IR_TO_LLVM_H
