
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "IR/ir_type_var.h"

void ir_type_var_stack_init(ir_type_var_stack_t *stack){
    stack->frames = NULL;
    stack->frames_length = 0;
    stack->frames_capacity = 0;
}

void ir_type_var_stack_free(ir_type_var_stack_t *stack){
    for(length_t i = 0; i != stack->frames_length; i++)
        free(stack->frames[i].type_vars);
    free(stack->frames);
}

void ir_type_var_stack_open(ir_type_var_stack_t *stack){
    expand((void**) &stack->frames, sizeof(ir_type_var_frame_t), stack->frames_length, &stack->frames_capacity, 1, 4);
    ir_type_var_frame_t *frame = &stack->frames[stack->frames_length++];
    frame->type_vars = NULL;
    frame->type_vars_length = 0;
    frame->type_vars_capacity = 0;
}

void ir_type_var_stack_close(ir_type_var_stack_t *stack){
    if(stack->frames_length == 0){
        redprintf("INTERNAL ERROR: ir_type_var_stack_close when no frame present\n");
        return;
    }

    free(stack->frames[--stack->frames_length].type_vars);
}

successful_t ir_type_var_stack_add(ir_type_var_stack_t *stack, ir_type_var_t type_var){
    if(stack->frames_length == 0){
        redprintf("INTERNAL ERROR: ir_type_var_stack_add when no frame present\n");
        return false;
    }

    ir_type_var_frame_t *frame = &stack->frames[stack->frames_length - 1];

    if(ir_type_var_frame_find(frame, type_var.name) != NULL){
        return false;
    }

    expand((void**) &frame->type_vars, sizeof(ir_type_var_t), frame->type_vars_length, &frame->type_vars_capacity, 1, 4);
    frame->type_vars[frame->type_vars_length++] = type_var;
    return true;
}

ir_type_var_t *ir_type_var_frame_find(ir_type_var_frame_t *frame, weak_cstr_t name){
    // TODO: SPEED: Improve searching (maybe not worth it?)

    for(length_t i = 0; i != frame->type_vars_length; i++){
        if(strcmp(frame->type_vars[i].name, name) == 0) return &frame->type_vars[i];
    }

    return NULL;
}

ir_type_var_t *ir_type_var_stack_find(ir_type_var_stack_t *stack, weak_cstr_t name){
    for(length_t i = stack->frames_length + 1; i != 0; i--){
        ir_type_var_t *maybe = ir_type_var_frame_find(&stack->frames[i - 1], name);
        if(maybe) return maybe;
    }
    
    return NULL;
}
