
#ifndef IR_BUILDER_H
#define IR_BUILDER_H

/*
    =============================== ir_builder.h ===============================
    Module for building intermediate representation
    ----------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"
#include "BRIDGE/bridge.h"

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
    bridge_var_scope_t *var_scope;
    length_t next_var_id;
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

// ---------------- build_varptr ----------------
// Builds a varptr instruction
ir_value_t* build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_load ----------------
// Builds a load instruction
ir_value_t* build_load(ir_builder_t *builder, ir_value_t *value);


// ---------------- prepare_for_new_label ----------------
// Ensures there's enough room for another label
void prepare_for_new_label(ir_builder_t *builder);

// ---------------- open_var_scope ----------------
// Opens a new variable scope
void open_var_scope(ir_builder_t *builder);

// ---------------- close_var_scope ----------------
// Closes the current variable scope
void close_var_scope(ir_builder_t *builder);

// ---------------- add_variable ----------------
// Adds a variable to the current bridge_var_scope_t
int add_variable(ir_builder_t *builder, char *name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits);

#endif // IR_BUILDER_H
