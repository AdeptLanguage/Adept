
#ifndef IRBUILDER_H
#define IRBUILDER_H

/*
    =============================== irbuilder.h ===============================
    Module for building intermediate representation
    ---------------------------------------------------------------------------
*/

#include "ir.h"
#include "ground.h"
#include "object.h"
#include "compiler.h"

// ---------------- ir_builder_t ----------------
// Container for storing general information
// about the building state
typedef struct {
    ir_basicblock_t *basicblocks;
    length_t basicblocks_length;
    length_t basicblocks_capacity;
    ir_basicblock_t *current_block;
    length_t current_block_id;
    length_t break_block_id; // 0 == none
    length_t continue_block_id; // 0 == none
    char **block_stack_labels;
    length_t *block_stack_break_ids;
    length_t *block_stack_continue_ids;
    length_t block_stack_length;
    length_t block_stack_capacity;
    ir_pool_t *pool;
    ir_type_map_t *type_map;
    compiler_t *compiler;
    object_t *object;
    ast_func_t *ast_func;
    ir_func_t *module_func;
} ir_builder_t;

// ---------------- build_basicblock ----------------
// Builds a new basic block in the current function
// NOTE: All basic block pointers should be recalculated
//       after calling this function
length_t build_basicblock(ir_builder_t *builder);

// ---------------- build_using_basicblock ----------------
// Changes the current basic block that new instructions are added into
void build_using_basicblock(ir_builder_t *builder, length_t basicblock_id);

// ---------------- build_instruction ----------------
// Builds a new undetermined instruction
ir_instr_t *build_instruction(ir_builder_t *builder, length_t size);

// ---------------- build_string_literal ----------------
// Builds a null-terminated string literal IR value
void build_string_literal(ir_builder_t *builder, char *value, ir_value_t **ir_value);

// ---------------- build_value_from_prev_instruction ----------------
// Builds an IR value from the result of the previsous instruction
ir_value_t *build_value_from_prev_instruction(ir_builder_t *builder);

// ---------------- ir_builder_prepare_for_new_label ----------------
// Ensures there's enough room for another label
void ir_builder_prepare_for_new_label(ir_builder_t *builder);

#endif // IRBUILDER_H
