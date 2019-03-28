
#ifndef IR_TYPE_VAR_H
#define IR_TYPE_VAR_H

#include "IR/ir_type.h"
#include "UTIL/ground.h"

// ---------------- ir_type_var_t ----------------
// Polymorphic type variable
typedef struct {
    weak_cstr_t name;
    ir_type_t *type;
} ir_type_var_t;

// ---------------- ir_type_var_frame_t ----------------
// Polymorphic type variable stack frame
typedef struct {
    ir_type_var_t *type_vars;
    length_t type_vars_length;
    length_t type_vars_capacity;
} ir_type_var_frame_t;

// ---------------- ir_type_var_stack_t ----------------
// Polymorphic type variable stack
typedef struct {
    ir_type_var_frame_t *frames;
    length_t frames_length;
    length_t frames_capacity;
} ir_type_var_stack_t;

// ---------------- ir_type_var_stack_init ----------------
// Initializes a type variable stack
void ir_type_var_stack_init(ir_type_var_stack_t *stack);

// ---------------- ir_type_var_stack_free ----------------
// Frees a type variable stack
void ir_type_var_stack_free(ir_type_var_stack_t *stack);

// ---------------- ir_type_var_stack_open ----------------
// Opens a new frame in a type variable stack
void ir_type_var_stack_open(ir_type_var_stack_t *stack);

// ---------------- ir_type_var_stack_close ----------------
// Closes the previous frame in a type variable stack
void ir_type_var_stack_close(ir_type_var_stack_t *stack);

// ---------------- ir_type_var_stack_add ----------------
// Adds a type variable to the current stack frame
// NOTE: Returns successful if there weren't any name collisions within
//       the current stack frame and the type was added successfully
successful_t ir_type_var_stack_add(ir_type_var_stack_t *stack, ir_type_var_t type_var);

// ---------------- ir_type_var_frame_find ----------------
// Finds a type variable within a single stack frame
// NOTE: Returns NULL if non-existent
ir_type_var_t *ir_type_var_frame_find(ir_type_var_frame_t *frame, weak_cstr_t name);

// ---------------- ir_type_var_stack_find ----------------
// Finds a type variable within a type variable stack
// NOTE: Returns NULL if non-existent
ir_type_var_t *ir_type_var_stack_find(ir_type_var_stack_t *stack, weak_cstr_t name);

#endif // IR_TYPE_VAR_H
