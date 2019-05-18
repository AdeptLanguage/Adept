
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "IRGEN/ir_builder.h"
#include "IRGEN/ir_gen_find.h"
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

ir_value_t* build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b){
    ir_instr_math_t *built_instr = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    ((ir_instr_math_t*) built_instr)->id = INSTRUCTION_EQUALS;
    ((ir_instr_math_t*) built_instr)->a = a;
    ((ir_instr_math_t*) built_instr)->b = b;
    ((ir_instr_math_t*) built_instr)->result_type = ir_builder_bool(builder);
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable){
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

ir_value_t* build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_STRUCT_CONSTRUCTION;
    value->type = type;

    ir_value_struct_construction_t *extra = ir_pool_alloc(pool, sizeof(ir_value_struct_construction_t));
    extra->values = values;
    extra->length = length;
    value->extra = extra;

    return value;
}

ir_value_t* build_static_array(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_ARRAY_LITERAL;
    value->type = ir_type_pointer_to(pool, type);
    ir_value_array_literal_t *extra = ir_pool_alloc(pool, sizeof(ir_value_array_literal_t));
    extra->values = values;
    extra->length = length;
    value->extra = extra;
    return value;
}

ir_value_t* build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant){
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

ir_value_t* build_literal_int(ir_pool_t *pool, long long literal_value){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));

    value->value_type = VALUE_TYPE_LITERAL;
    value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    value->type->kind = TYPE_KIND_S32;
    // neglect ir_value->type->extra
    
    value->extra = ir_pool_alloc(pool, sizeof(long long));
    *((long long*) value->extra) = literal_value;
    return value;
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

ir_value_t* build_literal_str(ir_builder_t *builder, char *array, length_t length){
    if(builder->object->ir_module.common.ir_string_struct == NULL){
        redprintf("Can't create string literal without String type present");
        printf("\nTry importing '2.1/String.adept'\n");
        return NULL;
    }

    ir_value_t **values = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 4);
    values[0] = build_literal_cstr_of_length(builder, array, length);
    values[1] = build_literal_usize(builder->pool, length);
    values[2] = build_literal_usize(builder->pool, length);

    // DANGEROUS: This is a hack to initialize String.ownership as a reference
    values[3] = build_literal_usize(builder->pool, 0);
    
    ir_value_t *value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_STRUCT_LITERAL;
    value->type = builder->object->ir_module.common.ir_string_struct;
    ir_value_struct_literal_t *extra = ir_pool_alloc(builder->pool, sizeof(ir_value_struct_literal_t));
    extra->values = values;
    extra->length = 4;
    value->extra = extra;
    return value;
}

ir_value_t* build_literal_cstr(ir_builder_t *builder, weak_cstr_t value){
    return build_literal_cstr_of_length(builder, value, strlen(value) + 1);
}

ir_value_t* build_literal_cstr_of_length(ir_builder_t *builder, char *array, length_t length){
    // NOTE: Builds a null-terminated string literal value
    ir_value_t *ir_value = ir_pool_alloc(builder->pool, sizeof(ir_value_t));
    ir_value->value_type = VALUE_TYPE_CSTR_OF_LEN;
    ir_type_t *ubyte_type;
    ir_type_map_find(builder->type_map, "ubyte", &ubyte_type);
    ir_value->type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
    ir_value->type->kind = TYPE_KIND_POINTER;
    ir_value->type->extra = ubyte_type;
    ir_value->extra = ir_pool_alloc(builder->pool, sizeof(ir_value_cstr_of_len_t));
    ((ir_value_cstr_of_len_t*) ir_value->extra)->array = array;
    ((ir_value_cstr_of_len_t*) ir_value->extra)->length = length;
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

ir_value_t* build_bitcast(ir_builder_t *builder, ir_value_t *from, ir_type_t* to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_BITCAST;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_const_bitcast(ir_pool_t *pool, ir_value_t *from, ir_type_t *to){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_CONST_BITCAST;
    value->type = to;
    value->extra = from;
    return value;
}

ir_value_t* build_zext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_ZEXT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_trunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_TRUNC;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_fext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FEXT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_ftrunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FTRUNC;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_inttoptr(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_INTTOPTR;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_ptrtoint(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_PTRTOINT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_fptoui(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FPTOUI;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_fptosi(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FPTOSI;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_uitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_UITOFP;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t* build_sitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_SITOFP;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result){
    ir_instr_math_t *instruction = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    instruction->id = instr_id;
    instruction->a = a;
    instruction->b = b;
    instruction->result_type = result;
    return build_value_from_prev_instruction(builder);
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
        builder->block_stack_scopes = malloc(sizeof(bridge_var_scope_t*) * 4);
        builder->block_stack_capacity = 4;
    } else if(builder->block_stack_length == builder->block_stack_capacity){
        builder->block_stack_capacity *= 2;
        char **new_block_stack_labels = malloc(sizeof(char*) * builder->block_stack_capacity);
        length_t *new_block_stack_break_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        length_t *new_block_stack_continue_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        bridge_var_scope_t **new_block_stack_scopes = malloc(sizeof(bridge_var_scope_t*) * builder->block_stack_capacity);
        memcpy(new_block_stack_labels, builder->block_stack_labels, sizeof(char*) * builder->block_stack_length);
        memcpy(new_block_stack_break_ids, builder->block_stack_break_ids, sizeof(length_t) * builder->block_stack_length);
        memcpy(new_block_stack_continue_ids, builder->block_stack_continue_ids, sizeof(length_t) * builder->block_stack_length);
        memcpy(new_block_stack_scopes, builder->block_stack_scopes, sizeof(bridge_var_scope_t*) * builder->block_stack_length);
        free(builder->block_stack_labels);
        free(builder->block_stack_break_ids);
        free(builder->block_stack_continue_ids);
        free(builder->block_stack_scopes);
        builder->block_stack_labels = new_block_stack_labels;
        builder->block_stack_break_ids = new_block_stack_break_ids;
        builder->block_stack_continue_ids = new_block_stack_continue_ids;
        builder->block_stack_scopes = new_block_stack_scopes;
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

errorcode_t handle_deference_for_variables(ir_builder_t *builder, bridge_var_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        // Don't perform defer management on POD variables or non-struct types
        if(list->variables[i].traits & BRIDGE_VAR_POD || list->variables[i].ir_type->kind != TYPE_KIND_STRUCTURE) continue;

        bridge_var_t *variable = &list->variables[i];

        // Ensure we're working with a single AST type element in the type
        if(variable->ast_type->elements_length != 1) continue;

        // Capture snapshot of current pool state (for if we need to revert allocations)
        ir_pool_snapshot_t snapshot;
        ir_pool_snapshot_capture(builder->pool, &snapshot);

        ir_value_t *ir_var_value = build_varptr(builder, ir_type_pointer_to(builder->pool, variable->ir_type), variable->id);

        errorcode_t failed = handle_single_deference(builder, variable->ast_type, ir_var_value);

        if(failed){
            // Remove VARPTR instruction
            builder->current_block->instructions_length--;

            // Revert recent pool allocations
            ir_pool_snapshot_restore(builder->pool, &snapshot);

            // Real failure if a compile time error occured
            if(failed == ALT_FAILURE) return FAILURE;
        }
    }

    return SUCCESS;
}

errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *value){
    // Calls __defer__ method on a value and it's children if the method exists
    // NOTE: Assumes (ast_type->elements_length == 1)
    // NOTE: Returns SUCCESS if value was utilized in deference
    //       Returns FAILURE if value was not utilized in deference
    //       Returns ALT_FAILURE if a compiler time error occured

    funcpair_t defer_func;

    ast_type_t ast_type_ptr;
    ast_elem_t *ast_type_ptr_elems[2];
    ast_elem_t ast_type_ptr_elem;

    ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*));
    arguments[0] = value;

    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE: {
            weak_cstr_t struct_name = ((ast_elem_base_t*) ast_type->elements[0])->base;

            // Create temporary AST pointer type
            ast_type_ptr_elem.id = AST_ELEM_POINTER;
            ast_type_ptr_elem.source = ast_type->source;
            ast_type_ptr_elems[0] = &ast_type_ptr_elem;
            ast_type_ptr_elems[1] = ast_type->elements[0];
            ast_type_ptr.elements = ast_type_ptr_elems;
            ast_type_ptr.elements_length = 2;
            ast_type_ptr.source = ast_type->source;

            if(ir_gen_find_method_conforming(builder, struct_name, "__defer__", arguments, &ast_type_ptr, 1, &defer_func)){
                return FAILURE;
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            weak_cstr_t struct_name = ((ast_elem_generic_base_t*) ast_type->elements[0])->name;

            // Create temporary AST pointer type
            ast_type_ptr_elem.id = AST_ELEM_POINTER;
            ast_type_ptr_elem.source = ast_type->source;
            ast_type_ptr_elems[0] = &ast_type_ptr_elem;
            ast_type_ptr_elems[1] = ast_type->elements[0];
            ast_type_ptr.elements = ast_type_ptr_elems;
            ast_type_ptr.elements_length = 2;
            ast_type_ptr.source = ast_type->source;

            if(ir_gen_find_generic_base_method_conforming(builder, struct_name, "__defer__", arguments, &ast_type_ptr, 1, &defer_func)){
                return FAILURE;
            }
        }
        break;
    default:
        return FAILURE;
    }

    // Call __defer__()
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
    instruction->id = INSTRUCTION_CALL;
    instruction->result_type = builder->object->ir_module.funcs[defer_func.ir_func_id].return_type;
    instruction->values = arguments;
    instruction->values_length = 1;
    instruction->ir_func_id = defer_func.ir_func_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction; 
    return SUCCESS;
}

errorcode_t handle_children_deference(ir_builder_t *builder){
    // Generates __defer__ calls for children of the parent type of a __defer__ function

    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *func = &builder->object->ast.funcs[builder->ast_func_id];
    ast_type_t *this_ast_type = func->arity == 1 ? &func->arg_types[0] : NULL;
    ir_pool_snapshot_t snapshot;

    if(this_ast_type == NULL || this_ast_type->elements_length != 2){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_dereference() encountered unrecognzied format of __defer__ management method");
        return FAILURE;
    }

    if(func->arg_type_traits[0] & BRIDGE_VAR_POD) return SUCCESS;

    ast_elem_t *concrete_elem = this_ast_type->elements[1];

    if(this_ast_type->elements[0]->id != AST_ELEM_POINTER || !(concrete_elem->id == AST_ELEM_BASE || concrete_elem->id == AST_ELEM_GENERIC_BASE)){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_dereference() encountered unrecognzied argument types of __defer__ management method");
        return FAILURE;
    }

    switch(concrete_elem->id){
    case AST_ELEM_BASE: {
            weak_cstr_t struct_name = ((ast_elem_base_t*) concrete_elem)->base;

            // Attempt to call __defer__ on members
            ast_struct_t *ast_struct = ast_struct_find(&builder->object->ast, struct_name);

            // Don't bother with structs that don't exist
            if(ast_struct == NULL) return FAILURE;

            for(length_t f = 0; f != ast_struct->field_count; f++){
                ast_type_t *ast_field_type = &ast_struct->field_types[f];

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);
                
                ir_type_t *ir_field_type;
                if(ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type)){
                    return ALT_FAILURE;
                }

                ir_type_t *this_ir_type;
                if(ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)){
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_varptr(builder, ir_type_pointer_to(builder->pool, this_ir_type), 0));

                ir_type_t *ir_field_ptr_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                ir_field_ptr_type->kind = TYPE_KIND_POINTER;
                ir_field_ptr_type->extra = ir_field_type;

                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_t *instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_member_t));
                ((ir_instr_member_t*) instruction)->id = INSTRUCTION_MEMBER;
                ((ir_instr_member_t*) instruction)->result_type = ir_field_ptr_type;
                ((ir_instr_member_t*) instruction)->value = this_ir_value;
                ((ir_instr_member_t*) instruction)->member = f;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                ir_value_t *ir_field_value = build_value_from_prev_instruction(builder);

                errorcode_t failed = handle_single_deference(builder, ast_field_type, ir_field_value);

                if(failed){
                    // Remove VARPTR, LOAD, and MEMBER instruction
                    builder->current_block->instructions_length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propogate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) concrete_elem;
            weak_cstr_t template_name = generic_base->name;

            // Attempt to call __defer__ on members
            ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&builder->object->ast, template_name);

            // Don't bother with polymorphic structs that don't exist
            if(template == NULL) return FAILURE;

            // Substitution Catalog
            ast_type_var_catalog_t catalog;
            ast_type_var_catalog_init(&catalog);

            if(template->generics_length != generic_base->generics_length){
                redprintf("INTERNAL ERROR: Polymorphic struct '%s' type parameter length mismatch when generating child deference!\n", generic_base->name);
                ast_type_var_catalog_free(&catalog);
                return ALT_FAILURE;
            }

            for(length_t i = 0; i != template->generics_length; i++){
                ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
            }

            for(length_t f = 0; f != template->field_count; f++){
                ast_type_t *ast_unresolved_field_type = &template->field_types[f];
                ast_type_t ast_field_type;

                if(resolve_type_polymorphics(builder->compiler, &catalog, ast_unresolved_field_type, &ast_field_type)){
                    ast_type_var_catalog_free(&catalog);
                    return ALT_FAILURE;
                }

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);
                
                ir_type_t *ir_field_type;
                if(ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &ir_field_type)){
                    ast_type_var_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                ir_type_t *this_ir_type;
                if(ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)){
                    ast_type_var_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_varptr(builder, ir_type_pointer_to(builder->pool, this_ir_type), 0));

                ir_type_t *ir_field_ptr_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
                ir_field_ptr_type->kind = TYPE_KIND_POINTER;
                ir_field_ptr_type->extra = ir_field_type;

                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_t *instruction = (ir_instr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_member_t));
                ((ir_instr_member_t*) instruction)->id = INSTRUCTION_MEMBER;
                ((ir_instr_member_t*) instruction)->result_type = ir_field_ptr_type;
                ((ir_instr_member_t*) instruction)->value = this_ir_value;
                ((ir_instr_member_t*) instruction)->member = f;
                builder->current_block->instructions[builder->current_block->instructions_length++] = instruction;
                ir_value_t *ir_field_value = build_value_from_prev_instruction(builder);

                errorcode_t failed = handle_single_deference(builder, &ast_field_type, ir_field_value);
                ast_type_free(&ast_field_type);

                if(failed){
                    // Remove VARPTR, LOAD, and MEMBER instruction
                    builder->current_block->instructions_length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propogate alternate failure cause
                    if(failed == ALT_FAILURE){
                        ast_type_var_catalog_free(&catalog);
                        return ALT_FAILURE;
                    }
                }
            }

            ast_type_var_catalog_free(&catalog);
        }
        break;
    default:
        return FAILURE;
    }

    return SUCCESS;
}

void handle_defer_management(ir_builder_t *builder, bridge_var_list_t *list){
    handle_deference_for_variables(builder, list);
}

void handle_pass_management(ir_builder_t *builder, ir_value_t **values, ast_type_t *types, trait_t *arg_type_traits, length_t arity){
    for(length_t i = 0; i != arity; i++){
        if(arg_type_traits != NULL && arg_type_traits[i] & AST_FUNC_ARG_TYPE_TRAIT_POD) continue;

        if(values[i]->type->kind == TYPE_KIND_STRUCTURE){
            ast_type_t *ast_type = &types[i];

            if(ast_type->elements_length == 1 && ast_type->elements[0]->id == AST_ELEM_BASE){
                funcpair_t result;
                if(ir_gen_find_func(builder->compiler, builder->object, "__pass__", &types[i], 1, &result) == FAILURE){
                    continue;
                }

                ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*));
                arguments[0] = values[i];
                
                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
                instruction->id = INSTRUCTION_CALL;
                instruction->result_type = result.ir_func->return_type;
                instruction->values = arguments;
                instruction->values_length = 1;
                instruction->ir_func_id = result.ir_func_id;
                builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
                values[i] = build_value_from_prev_instruction(builder);
            }
        }
    }
}

successful_t handle_assign_management(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, ast_type_t *type, bool zero_initialize){    
    if(value->type->kind == TYPE_KIND_STRUCTURE){
        if(type->elements_length == 1 && type->elements[0]->id == AST_ELEM_BASE){
            weak_cstr_t struct_name = ((ast_elem_base_t*) type->elements[0])->base;

            maybe_index_t index = find_beginning_of_method_group(builder->object->ir_module.methods, builder->object->ir_module.methods_length, struct_name, "__assign__");
            if(index == -1) return UNSUCCESSFUL;

            if(zero_initialize){
                // Zero initialize for declaration assignments
                ir_basicblock_new_instructions(builder->current_block, 1);
                ir_instr_varzeroinit_t *zero_instr = (ir_instr_varzeroinit_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varzeroinit_t));
                zero_instr->id = INSTRUCTION_VARZEROINIT;
                zero_instr->result_type = NULL;
                zero_instr->index = builder->next_var_id - 1;
                builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) zero_instr;
            }

            ir_method_t *method = &builder->object->ir_module.methods[index];

            ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
            arguments[0] = destination;
            arguments[1] = value;
            
            ir_basicblock_new_instructions(builder->current_block, 1);
            ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
            instruction->id = INSTRUCTION_CALL;
            instruction->result_type = builder->object->ir_module.funcs[method->ir_func_id].return_type;
            instruction->values = arguments;
            instruction->values_length = 2;
            instruction->ir_func_id = method->ir_func_id;
            builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
            return SUCCESSFUL;
        }
    }

    return UNSUCCESSFUL;
}

ir_value_t* handle_math_management(ir_builder_t *builder, ir_value_t *lhs, ir_value_t *rhs, ast_type_t *lhs_type, ast_type_t *rhs_type, ast_type_t *out_type, const char *overload_name){
    if(lhs->type->kind == TYPE_KIND_STRUCTURE){
        if(lhs_type->elements_length == 1 && lhs_type->elements[0]->id == AST_ELEM_BASE){
            funcpair_t result;

            ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
            arguments[0] = lhs;
            arguments[1] = rhs;

            ast_type_t types[2] = {*lhs_type, *rhs_type};

            if(ir_gen_find_func_conforming(builder, overload_name, arguments, types, 2, &result)){
                // Failed to find a suitable function
                return NULL;
            }

            handle_pass_management(builder, arguments, types, result.ast_func->arg_type_traits, 2);

            ir_basicblock_new_instructions(builder->current_block, 1);
            ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
            instruction->id = INSTRUCTION_CALL;
            instruction->result_type = result.ir_func->return_type;
            instruction->values = arguments;
            instruction->values_length = 2;
            instruction->ir_func_id = result.ir_func_id;
            builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;

            if(out_type != NULL) *out_type = ast_type_clone(&result.ast_func->return_type);
            return build_value_from_prev_instruction(builder);
        }
    }

    return NULL;
}

errorcode_t instantiate_polymorphic_func(ir_builder_t *builder, ast_func_t *poly_func, ast_type_t *types,
        length_t types_length, ast_type_var_catalog_t *catalog, ir_func_mapping_t *out_mapping){
    if(types_length < poly_func->arity) return FAILURE;

    ast_t *ast = &builder->object->ast;
    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    ast_func_create_template(func, strclone(poly_func->name), poly_func->traits & AST_FUNC_STDCALL, false, poly_func->source);
    if(poly_func->traits & AST_FUNC_VARARG) func->traits |= AST_FUNC_VARARG;
    func->traits |= AST_FUNC_GENERATED;

    func->arg_names = malloc(sizeof(weak_cstr_t) * poly_func->arity);
    func->arg_types = malloc(sizeof(ast_type_t) * poly_func->arity);
    func->arg_sources = malloc(sizeof(source_t) * poly_func->arity);
    func->arg_flows = malloc(sizeof(char) * poly_func->arity);
    func->arg_type_traits = malloc(sizeof(trait_t) * poly_func->arity);

    for(length_t i = 0; i != poly_func->arity; i++){
        func->arg_names[i] = strclone(poly_func->arg_names[i]);
        func->arg_types[i] = ast_type_clone(&types[i]);
    }

    memcpy(func->arg_sources, poly_func->arg_sources, sizeof(source_t) * poly_func->arity);
    memcpy(func->arg_flows, poly_func->arg_flows, sizeof(char) * poly_func->arity);
    memcpy(func->arg_type_traits, poly_func->arg_type_traits, sizeof(trait_t) * poly_func->arity);

    func->arity = poly_func->arity;
    if(resolve_type_polymorphics(builder->compiler, catalog, &poly_func->return_type, &func->return_type)){
        func->return_type.elements = NULL;
        func->return_type.elements_length = 0;
        func->return_type.source = NULL_SOURCE;
        return FAILURE;
    }

    func->statements_length = poly_func->statements_length;
    func->statements_capacity = poly_func->statements_length; // (statements_length is on purpose)
    func->statements = malloc(sizeof(ast_expr_t*) * poly_func->statements_length);

    for(length_t s = 0; s != poly_func->statements_length; s++){
        func->statements[s] = ast_expr_clone(poly_func->statements[s]);
        if(resolve_expr_polymorphics(builder->compiler, catalog, func->statements[s])) return FAILURE;
    }

    ir_func_mapping_t newest_mapping;
    if(ir_gen_func_head(builder->compiler, builder->object, func, ast_func_id, true, &newest_mapping)){
        return FAILURE;
    }

    // Add mapping to IR jobs
    ir_jobs_t *jobs = builder->jobs;
    expand((void**) &jobs->jobs, sizeof(ir_func_mapping_t), jobs->length, &jobs->capacity, 1, 4);
    jobs->jobs[jobs->length++] = newest_mapping;

    if(out_mapping) *out_mapping = newest_mapping;
    return SUCCESS;
}

errorcode_t attempt_autogen___defer__(ir_builder_t *builder, ir_value_t **arg_values, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result){
    if(type_list_length != 1) return FAILURE; // Require single argument ('this') for auto-generated __defer__

    bool is_base_ptr = ast_type_is_base_ptr(&arg_types[0]);
    bool is_generic_base_ptr = is_base_ptr ? false : ast_type_is_generic_base_ptr(&arg_types[0]);

    if(!(is_base_ptr || is_generic_base_ptr)){
        return FAILURE; // Require '*Type' or '*<...> Type' for 'this' type
    }

    ast_t *ast = &builder->object->ast;
    ir_module_t *module = &builder->object->ir_module;

    if(is_base_ptr){
        weak_cstr_t struct_name = ((ast_elem_base_t*) arg_types[0].elements[1])->base;
        if(ast_struct_find(ast, struct_name) == NULL) return FAILURE; // Require structure to exist
    } else if(is_generic_base_ptr){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[1];
        weak_cstr_t struct_name = generic_base->name;
        ast_polymorphic_struct_t *polymorphic_struct = ast_polymorphic_struct_find(ast, struct_name);
        if(polymorphic_struct == NULL) return FAILURE; // Require generic structure to exist
        if(polymorphic_struct->generics_length != generic_base->generics_length) return FAILURE; // Require generics count to match
    }

    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    ast_func_create_template(func, strclone("__defer__"), false, false, NULL_SOURCE);
    func->traits |= AST_FUNC_GENERATED;

    func->arg_names = malloc(sizeof(weak_cstr_t) * 1);
    func->arg_types = malloc(sizeof(ast_type_t));
    func->arg_sources = malloc(sizeof(source_t));
    func->arg_flows = malloc(sizeof(char));
    func->arg_type_traits = malloc(sizeof(trait_t));

    func->arg_names[0] = strclone("this");
    func->arg_types[0] = ast_type_clone(&arg_types[0]);
    func->arg_sources[0] = NULL_SOURCE;
    func->arg_flows[0] = FLOW_IN;
    func->arg_type_traits[0] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arity = 1;

    func->statements_length = 0;
    func->statements_capacity = 0;
    func->statements = NULL;

    ast_type_make_base(&func->return_type, strclone("void"));

    // Don't generate any statements because children will
    // be taken care of inside __defer__ function during IR generation
    // of the auto-generated function

    ir_func_mapping_t newest_mapping;
    if(ir_gen_func_head(builder->compiler, builder->object, func, ast_func_id, true, &newest_mapping)){
        return FAILURE;
    }

    // Add mapping to IR jobs
    ir_jobs_t *jobs = builder->jobs;
    expand((void**) &jobs->jobs, sizeof(ir_func_mapping_t), jobs->length, &jobs->capacity, 1, 4);
    jobs->jobs[jobs->length++] = newest_mapping;

    result->ast_func_id = ast_func_id;
    result->ir_func_id = newest_mapping.ir_func_id;
    result->ast_func = &ast->funcs[ast_func_id];
    result->ir_func = &module->funcs[newest_mapping.ir_func_id];
    return SUCCESS;
}

errorcode_t resolve_type_polymorphics(compiler_t *compiler, ast_type_var_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type){
    ast_elem_t **elements = NULL;
    length_t length = 0;
    length_t capacity = 0;

    for(length_t i = 0; i != in_type->elements_length; i++){
        expand((void**) &elements, sizeof(ast_elem_t*), length, &capacity, 1, 4);

        unsigned int elem_id = in_type->elements[i]->id;

        switch(elem_id){
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func = (ast_elem_func_t*) in_type->elements[i];
                
                // DANGEROUS: Manually creating/deleting ast_elem_func_t
                ast_elem_func_t *resolved = malloc(sizeof(ast_elem_func_t));
                resolved->id = AST_ELEM_FUNC;
                resolved->source = func->source;
                resolved->arg_types = malloc(sizeof(ast_type_t) * func->arity);
                resolved->arity = func->arity;
                resolved->return_type = malloc(sizeof(ast_type_t));
                resolved->traits = func->traits;
                resolved->ownership = true;

                for(length_t i = 0; i != func->arity; i++){
                    if(resolve_type_polymorphics(compiler, catalog, &func->arg_types[i], &resolved->arg_types[i])){
                        ast_types_free_fully(resolved->arg_types, i);
                        free(resolved->return_type);
                        free(resolved);
                        return FAILURE;
                    }
                }

                if(resolve_type_polymorphics(compiler, catalog, func->return_type, resolved->return_type)){
                    ast_types_free_fully(resolved->arg_types, i);
                    free(resolved->return_type);
                    free(resolved);
                    return FAILURE;
                }

                elements[length++] = (ast_elem_t*) resolved;
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) in_type->elements[i];

                if(generic_base_elem->name_is_polymorphic){
                    compiler_panic(compiler, generic_base_elem->source, "INTERNAL ERROR: Polymorphic names for polymorphic struct unimplemented for resolve_type_polymorphics");
                    return FAILURE;
                }

                ast_type_t *resolved = malloc(sizeof(ast_type_t) * generic_base_elem->generics_length);

                for(length_t i = 0; i != generic_base_elem->generics_length; i++){
                    if(resolve_type_polymorphics(compiler, catalog, &generic_base_elem->generics[i], &resolved[i])){
                        ast_types_free_fully(resolved, i);
                        return FAILURE;
                    }
                }

                ast_elem_generic_base_t *resolved_generic_base = malloc(sizeof(ast_elem_generic_base_t));
                resolved_generic_base->id = AST_ELEM_GENERIC_BASE;
                resolved_generic_base->source = generic_base_elem->source;
                resolved_generic_base->name = strclone(generic_base_elem->name);
                resolved_generic_base->generics = resolved;
                resolved_generic_base->generics_length = generic_base_elem->generics_length;
                resolved_generic_base->name_is_polymorphic = false;
                elements[length++] = (ast_elem_t*) resolved_generic_base;
            }
            break;
        case AST_ELEM_POLYMORPH: {
                // Find the determined type for the polymorphic type variable
                ast_elem_polymorph_t *polymorphic_element = (ast_elem_polymorph_t*) in_type->elements[i];
                ast_type_var_t *type_var = ast_type_var_catalog_find(catalog, polymorphic_element->name);

                if(type_var == NULL){
                    compiler_panicf(compiler, in_type->source, "Undetermined polymorphic type variable '$%s'", polymorphic_element->name);
                    return FAILURE;
                }

                // Replace the polymorphic type variable with the determined type
                expand((void**) &elements, sizeof(ast_elem_t*), length, &capacity, type_var->binding.elements_length, 4);
                for(length_t j = 0; j != type_var->binding.elements_length; j++){
                    elements[length++] = ast_elem_clone(type_var->binding.elements[j]);
                }
            }
            break;
        default:
            // Clone any non-polymorphic type elements
            elements[length++] = ast_elem_clone(in_type->elements[i]);
        }
    }

    if(out_type == NULL){
        out_type = in_type;
    }
    
    if(out_type == in_type){
        ast_type_free(in_type);
    }

    out_type->elements = elements;
    out_type->elements_length = length;
    out_type->source = in_type->source;
    return SUCCESS;
}

errorcode_t resolve_expr_polymorphics(compiler_t *compiler, ast_type_var_catalog_t *catalog, ast_expr_t *expr){
    switch(expr->id){
    case EXPR_RETURN: {
            ast_expr_return_t *return_stmt = (ast_expr_return_t*) expr;
            if(return_stmt->value != NULL && resolve_expr_polymorphics(compiler, catalog, return_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_CALL: {
            ast_expr_call_t *call_stmt = (ast_expr_call_t*) expr;
            for(length_t a = 0; a != call_stmt->arity; a++){
                if(resolve_expr_polymorphics(compiler, catalog, call_stmt->args[a])) return FAILURE;
            }
        }
        break;
    case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
            ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) expr;

            if(resolve_type_polymorphics(compiler, catalog, &declare_stmt->type, NULL)) return FAILURE;

            if(declare_stmt->value != NULL){
                if(resolve_expr_polymorphics(compiler, catalog, declare_stmt->value)) return FAILURE;
            }
        }
        break;
    case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
    case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN: {
            ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) expr;

            if(resolve_expr_polymorphics(compiler, catalog, assign_stmt->destination)
            || resolve_expr_polymorphics(compiler, catalog, assign_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
            ast_expr_if_t *conditional = (ast_expr_if_t*) expr;

            if(resolve_expr_polymorphics(compiler, catalog, conditional->value)) return FAILURE;

            for(length_t i = 0; i != conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, conditional->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_IFELSE: case EXPR_UNLESSELSE: {
            ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) expr;

            if(resolve_expr_polymorphics(compiler, catalog, complex_conditional->value)) return FAILURE;

            for(length_t i = 0; i != complex_conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, complex_conditional->statements[i])) return FAILURE;
            }

            for(length_t i = 0; i != complex_conditional->else_statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, complex_conditional->else_statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
            ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) expr;

            for(length_t i = 0; i != conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, conditional->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) expr;

            if(resolve_expr_polymorphics(compiler, catalog, call_stmt->value)) return FAILURE;

            for(length_t a = 0; a != call_stmt->arity; a++){
                if(resolve_expr_polymorphics(compiler, catalog, call_stmt->args[a])) return FAILURE;
            }
        }
        break;
    case EXPR_DELETE: {
            ast_expr_unary_t *delete_stmt = (ast_expr_unary_t*) expr;
            if(resolve_expr_polymorphics(compiler, catalog, delete_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_EACH_IN: {
            ast_expr_each_in_t *loop = (ast_expr_each_in_t*) expr;

            if(resolve_type_polymorphics(compiler, catalog, loop->it_type, NULL)
            || resolve_expr_polymorphics(compiler, catalog, loop->low_array)
            || resolve_expr_polymorphics(compiler, catalog, loop->length)) return FAILURE;

            for(length_t i = 0; i != loop->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, loop->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_REPEAT: {
            ast_expr_repeat_t *loop = (ast_expr_repeat_t*) expr;

            if(resolve_expr_polymorphics(compiler, catalog, loop->limit)) return FAILURE;

            for(length_t i = 0; i != loop->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, loop->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_ADD:
    case EXPR_SUBTRACT:
    case EXPR_MULTIPLY:
    case EXPR_DIVIDE:
    case EXPR_MODULUS:
    case EXPR_EQUALS:
    case EXPR_NOTEQUALS:
    case EXPR_GREATER:
    case EXPR_LESSER:
    case EXPR_GREATEREQ:
    case EXPR_LESSEREQ:
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
    case EXPR_BIT_LGC_LSHIFT:
    case EXPR_BIT_LGC_RSHIFT:
    case EXPR_AND:
    case EXPR_OR:
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_math_t*) expr)->a)
        || resolve_expr_polymorphics(compiler, catalog, ((ast_expr_math_t*) expr)->b)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_member_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) expr;

            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(resolve_type_polymorphics(compiler, catalog, &func_addr->match_args[a], NULL)) return FAILURE;
            }
        }
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_array_access_t*) expr)->index)) return FAILURE;
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_array_access_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_CAST:
        if(resolve_type_polymorphics(compiler, catalog, &((ast_expr_cast_t*) expr)->to, NULL)) return FAILURE;
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_cast_t*) expr)->from)) return FAILURE;
        break;
    case EXPR_SIZEOF:
        if(resolve_type_polymorphics(compiler, catalog, &((ast_expr_sizeof_t*) expr)->type, NULL)) return FAILURE;
        break;
    case EXPR_NEW:
        if(resolve_type_polymorphics(compiler, catalog, &((ast_expr_new_t*) expr)->type, NULL)) return FAILURE;
        if(((ast_expr_new_t*) expr)->amount != NULL && resolve_expr_polymorphics(compiler, catalog, ((ast_expr_new_t*) expr)->amount)) return FAILURE;
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
        if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_STATIC_ARRAY: {
            if(resolve_type_polymorphics(compiler, catalog, &((ast_expr_static_data_t*) expr)->type, NULL)) return FAILURE;

            for(length_t i = 0; i != ((ast_expr_static_data_t*) expr)->length; i++){
                if(resolve_expr_polymorphics(compiler, catalog, ((ast_expr_static_data_t*) expr)->values[i])) return FAILURE;
            }
        }
        break;
    case EXPR_STATIC_STRUCT: {
            if(resolve_type_polymorphics(compiler, catalog, &((ast_expr_static_data_t*) expr)->type, NULL)) return FAILURE;
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = (ast_expr_inline_declare_t*) expr;

            if(resolve_type_polymorphics(compiler, catalog, &def->type, NULL)
            ||(def->value != NULL && resolve_expr_polymorphics(compiler, catalog, def->value))){
                return FAILURE;
            }
        }
        break;
    default: break;
        // Ignore this statement, it doesn't contain any expressions that we need to worry about
    }
    
    return SUCCESS;
}
