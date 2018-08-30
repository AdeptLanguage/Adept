

#include "UTIL/util.h"
#include "UTIL/color.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_type.h"

length_t build_basicblock(ir_builder_t *builder){
    // NOTE: Returns new id
    // NOTE: All basicblock pointers should be recalculated after calling this function
    //           (Except for 'builder->current_block' which is automatically recalculated if necessary)

    if(builder->basicblocks_length == builder->basicblocks_capacity){
        builder->basicblocks_capacity *= 2;
        ir_basicblock_t *new_basicblocks = malloc(sizeof(ir_basicblock_t) * builder->basicblocks_capacity);
        memcpy(new_basicblocks, builder->basicblocks, sizeof(ir_basicblock_t) * builder->basicblocks_length);
        free(builder->basicblocks);
        builder->basicblocks = new_basicblocks;
        builder->current_block = &builder->basicblocks[builder->current_block_id];
    }

    ir_basicblock_t *block = &builder->basicblocks[builder->basicblocks_length++];
    block->instructions = malloc(sizeof(ir_instr_t*) * 16);
    block->instructions_length = 0;
    block->instructions_capacity = 16;
    block->traits = TRAIT_NONE;
    return builder->basicblocks_length - 1;
}

void build_using_basicblock(ir_builder_t *builder, length_t basicblock_id){
    // NOTE: Sets basicblock that instructions will be inserted into
    builder->current_block = &builder->basicblocks[basicblock_id];
    builder->current_block_id = basicblock_id;
}

ir_instr_t* build_instruction(ir_builder_t *builder, length_t size){
    // NOTE: Generates an instruction of the size 'size'
    ir_basicblock_new_instructions(builder->current_block, 1);
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) ir_pool_alloc(builder->pool, size);
    return builder->current_block->instructions[builder->current_block->instructions_length - 1];
}

void build_string_literal(ir_builder_t *builder, char *value, ir_value_t **ir_value){
    // NOTE: Builds a null-terminated string literal value
    *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    (*ir_value)->value_type = VALUE_TYPE_LITERAL;
    ir_type_t *ubyte_type;
    ir_type_map_find(builder->type_map, "ubyte", &ubyte_type);
    (*ir_value)->type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
    (*ir_value)->type->kind = TYPE_KIND_POINTER;
    (*ir_value)->type->extra = ubyte_type;
    (*ir_value)->extra = value;
}

ir_value_t *build_value_from_prev_instruction(ir_builder_t *builder){
    // NOTE: Builds an ir_value_t for the result of the last instruction in the current block

    ir_value_result_t *result = ir_pool_alloc(builder->pool, sizeof(ir_value_result_t));
    result->block_id = builder->current_block_id;
    result->instruction_id = builder->current_block->instructions_length - 1;

    ir_value_t *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    ir_value->value_type = VALUE_TYPE_RESULT;
    ir_value->type = builder->current_block->instructions[result->instruction_id]->result_type;
    ir_value->extra = result;

    // Little test to make sure ir_value->type is valid
    if(ir_value->type == NULL){
        redprintf("INTERNAL ERROR: build_value_from_prev_instruction() tried to create value from instruction without return_type set\n");
        redprintf("Previous instruction id: 0x%08X\n", (int) result->instruction_id);
        redprintf("Returning NULL, a crash will probably follow...\n");
        return NULL;
    }

    return ir_value;
}

ir_value_t* build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
    instruction->id = INSTRUCTION_VARPTR;
    instruction->result_type = ptr_type;
    instruction->index = variable_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_load(ir_builder_t *builder, ir_value_t *value){
    ir_type_t *dereferenced_type = ir_type_dereference(value->type);
    if(dereferenced_type == NULL) return NULL;

    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_load_t *instruction = (ir_instr_load_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_load_t));
    instruction->id = INSTRUCTION_LOAD;
    instruction->result_type = dereferenced_type;
    instruction->value = value;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return build_value_from_prev_instruction(builder);
}

void prepare_for_new_label(ir_builder_t *builder){
    if(builder->block_stack_capacity == 0){
        builder->block_stack_labels = malloc(sizeof(char*) * 4);
        builder->block_stack_break_ids = malloc(sizeof(length_t) * 4);
        builder->block_stack_continue_ids = malloc(sizeof(length_t) * 4);
        builder->block_stack_capacity = 4;
    } else if(builder->block_stack_length == builder->block_stack_capacity){
        builder->block_stack_capacity *= 2;
        char **new_block_stack_labels = malloc(sizeof(char*) * builder->block_stack_capacity);
        length_t *new_block_stack_break_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        length_t *new_block_stack_continue_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        memcpy(new_block_stack_labels, builder->block_stack_labels, sizeof(char*) * builder->block_stack_length);
        memcpy(new_block_stack_break_ids, builder->block_stack_break_ids, sizeof(length_t) * builder->block_stack_length);
        memcpy(new_block_stack_continue_ids, builder->block_stack_continue_ids, sizeof(length_t) * builder->block_stack_length);
        free(builder->block_stack_labels);
        free(builder->block_stack_break_ids);
        free(builder->block_stack_continue_ids);
        builder->block_stack_labels = new_block_stack_labels;
        builder->block_stack_break_ids = new_block_stack_break_ids;
        builder->block_stack_continue_ids = new_block_stack_continue_ids;
    }
}

void open_var_scope(ir_builder_t *builder){
    bridge_var_scope_t *var_scope = builder->var_scope;
    bridge_var_scope_t *scope = malloc(sizeof(bridge_var_scope_t));
    bridge_var_scope_init(scope, builder->var_scope);
    scope->first_var_id = builder->next_var_id;
    scope->parent = var_scope;

    expand((void**) &var_scope->children, sizeof(bridge_var_scope_t*), var_scope->children_length, &var_scope->children_capacity, 1, 4);
    var_scope->children[var_scope->children_length++] = scope;
    builder->var_scope = scope;
}

void close_var_scope(ir_builder_t *builder){
    if(builder->var_scope->parent == NULL) redprintf("INTERNAL ERROR: TRIED TO CLOSE BRIDGE VARIABLE SCOPE WITH NO PARENT, probably will crash...\n");

    bridge_var_scope_t *old_scope = builder->var_scope;
    old_scope->following_var_id = builder->next_var_id;
    builder->var_scope = old_scope->parent;
}

int add_variable(ir_builder_t *builder, char *name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits){
    bridge_var_list_t *list = &builder->var_scope->list;
    expand((void**) &list->variables, sizeof(bridge_var_t), list->length, &list->capacity, 1, 4);

    list->variables[list->length].name = name;
    list->variables[list->length].ast_type = ast_type;
    list->variables[list->length].ir_type = ir_type;
    list->variables[list->length].id = builder->next_var_id;
    list->variables[list->length].traits = traits;
    builder->next_var_id++;
    list->length++;
    return 0;
}
