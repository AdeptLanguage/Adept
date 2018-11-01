
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

ir_value_t* build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
    instruction->id = INSTRUCTION_GLOBALVARPTR;
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

void build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination){
    ir_instr_store_t *built_instr = (ir_instr_store_t*) build_instruction(builder, sizeof(ir_instr_store_t));
    built_instr->id = INSTRUCTION_STORE;
    built_instr->result_type = NULL;
    built_instr->value = value;
    built_instr->destination = destination;
}

void build_break(ir_builder_t *builder, length_t basicblock_id){
    ir_instr_break_t *built_instr = (ir_instr_break_t*) build_instruction(builder, sizeof(ir_instr_break_t));
    built_instr->id = INSTRUCTION_BREAK;
    built_instr->result_type = NULL;
    built_instr->block_id = basicblock_id;
}

ir_value_t *build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable){
    ir_value_t *value = ir_pool_alloc(&module->pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_STRUCT_LITERAL;
    value->type = type;
    ir_value_struct_literal_t *extra = ir_pool_alloc(&module->pool, sizeof(ir_value_struct_literal_t));
    extra->values = values;
    extra->length = length;
    value->extra = extra;

    if(make_mutable){
        ir_value_t *anon_global = build_anon_global(module, type, true);
        build_anon_global_initializer(module, anon_global, value);
        value = anon_global;
    }

    return value;
}

ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_ARRAY_LITERAL;
    value->type = ir_type_pointer_to(pool, type);
    ir_value_array_literal_t *extra = ir_pool_alloc(pool, sizeof(ir_value_array_literal_t));
    extra->values = values;
    extra->length = length;
    value->extra = extra;
    return value;
}

ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant){
    expand((void**) &module->anon_globals, sizeof(ir_anon_global_t), module->anon_globals_length, &module->anon_globals_capacity, 1, 4);

    ir_anon_global_t *anon_global = &module->anon_globals[module->anon_globals_length++];
    anon_global->type = type;
    anon_global->traits = is_constant ? IR_ANON_GLOBAL_CONSTANT : TRAIT_NONE;
    anon_global->initializer = NULL;

    ir_value_t *reference = ir_pool_alloc(&module->pool, sizeof(ir_value_t));
    reference->value_type = is_constant ? VALUE_TYPE_CONST_ANON_GLOBAL : VALUE_TYPE_ANON_GLOBAL;
    reference->type = ir_type_pointer_to(&module->pool, type);

    reference->extra = ir_pool_alloc(&module->pool, sizeof(ir_value_anon_global_t));
    ((ir_value_anon_global_t*) reference->extra)->anon_global_id = module->anon_globals_length - 1;
    return reference;
}

void build_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer){
    if(anon_global->value_type != VALUE_TYPE_ANON_GLOBAL && anon_global->value_type != VALUE_TYPE_CONST_ANON_GLOBAL){
        redprintf("INTERNAL ERROR: Failed to set initializer on value that isn't an anonymous global reference\n");
        return;
    }

    length_t index = ((ir_value_anon_global_t*) anon_global->extra)->anon_global_id;
    module->anon_globals[index].initializer = initializer;
}

ir_type_t* ir_builder_funcptr(ir_builder_t *builder){
    ir_type_t **shared_type = &builder->object->ir_module.common.ir_funcptr;

    if(*shared_type == NULL){
        (*shared_type) = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*shared_type)->kind = TYPE_KIND_FUNCPTR;
        // 'ir_funcptr_type->extra' not set because never used
    }
    
    return *shared_type;
}

ir_type_t* ir_builder_usize(ir_builder_t *builder){
    ir_type_t **shared_type = &builder->object->ir_module.common.ir_usize;

    if(*shared_type == NULL){
        (*shared_type) = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*shared_type)->kind = TYPE_KIND_U64;
    }

    return *shared_type;
}

ir_type_t* ir_builder_usize_ptr(ir_builder_t *builder){
    ir_type_t **shared_type = &builder->object->ir_module.common.ir_usize_ptr;

    if(*shared_type == NULL){
        (*shared_type) = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*shared_type)->kind = TYPE_KIND_POINTER;
        (*shared_type)->extra = ir_builder_usize(builder);
    }

    return *shared_type;
}

ir_type_t* ir_builder_bool(ir_builder_t *builder){
    ir_type_t **shared_type = &builder->object->ir_module.common.ir_bool;

    if(*shared_type == NULL){
        (*shared_type) = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*shared_type)->kind = TYPE_KIND_U64;
    }

    return *shared_type;
}

ir_value_t* build_literal_usize(ir_pool_t *pool, length_t literal_value){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));

    value->value_type = VALUE_TYPE_LITERAL;
    value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    value->type->kind = TYPE_KIND_U64;
    // neglect ir_value->type->extra
    
    value->extra = ir_pool_alloc(pool, sizeof(unsigned long long));
    *((unsigned long long*) value->extra) = literal_value;
    return value;
}

ir_value_t* build_literal_cstr(ir_builder_t *builder, weak_cstr_t value){
    // NOTE: Builds a null-terminated string literal value
    ir_value_t *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    ir_value->value_type = VALUE_TYPE_LITERAL;
    ir_type_t *ubyte_type;
    ir_type_map_find(builder->type_map, "ubyte", &ubyte_type);
    ir_value->type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
    ir_value->type->kind = TYPE_KIND_POINTER;
    ir_value->type->extra = ubyte_type;
    ir_value->extra = value;
    return ir_value;
}

ir_value_t* build_null_pointer(ir_pool_t *pool){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_NULLPTR;
    // neglect value->extra

    value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    value->type->kind = TYPE_KIND_POINTER;
    value->type->extra = ir_pool_alloc(pool, sizeof(ir_type_t));
    ((ir_type_t*) value->type->extra)->kind = TYPE_KIND_S8;
    // neglect ((ir_type_t*) value->type->extra)->extra

    return value;
}

ir_value_t* build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_NULLPTR_OF_TYPE;
    value->type = type;
    // neglect value->extra
    return value;
}

ir_value_t *build_bool(ir_pool_t *pool, bool value){
    ir_value_t *ir_value = ir_pool_alloc(pool, sizeof(ir_value_t));
    ir_value->value_type = VALUE_TYPE_LITERAL;

    ir_value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    ir_value->type->kind = TYPE_KIND_BOOLEAN;
    // neglect ir_value->type->extra

    ir_value->extra = ir_pool_alloc(pool, sizeof(bool));
    *((bool*) ir_value->extra) = value;
    return ir_value;
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

void add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits){
    bridge_var_list_t *list = &builder->var_scope->list;
    expand((void**) &list->variables, sizeof(bridge_var_t), list->length, &list->capacity, 1, 4);

    list->variables[list->length].name = name;
    list->variables[list->length].ast_type = ast_type;
    list->variables[list->length].ir_type = ir_type;
    list->variables[list->length].id = builder->next_var_id;
    list->variables[list->length].traits = traits;
    builder->next_var_id++;
    list->length++;
}
