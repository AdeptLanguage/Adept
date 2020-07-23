
#include "LEX/lex.h"
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

ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
    instruction->id = INSTRUCTION_VARPTR;
    instruction->result_type = ptr_type;
    instruction->index = variable_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id){
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_varptr_t *instruction = (ir_instr_varptr_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varptr_t));
    instruction->id = INSTRUCTION_GLOBALVARPTR;
    instruction->result_type = ptr_type;
    instruction->index = variable_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source){
    ir_type_t *dereferenced_type = ir_type_dereference(value->type);
    if(dereferenced_type == NULL) return NULL;

    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_load_t *instruction = (ir_instr_load_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_load_t));
    instruction->id = INSTRUCTION_LOAD;
    instruction->result_type = dereferenced_type;
    instruction->value = value;
    
    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        // If we're doing null checks, then give line/column to IR load instruction.
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &instruction->maybe_line_number, &instruction->maybe_column_number);
    }

    // Otherwise, we just ignore line/column fields
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return build_value_from_prev_instruction(builder);
}

void build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source){
    ir_instr_store_t *built_instr = (ir_instr_store_t*) build_instruction(builder, sizeof(ir_instr_store_t));
    built_instr->id = INSTRUCTION_STORE;
    built_instr->result_type = NULL;
    built_instr->value = value;
    built_instr->destination = destination;

    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        // If we're doing null checks, then give line/column to IR load instruction.
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &built_instr->maybe_line_number, &built_instr->maybe_column_number);
    }

    // Otherwise, we just ignore line/column fields
    return;
}

void build_break(ir_builder_t *builder, length_t basicblock_id){
    ir_instr_break_t *built_instr = (ir_instr_break_t*) build_instruction(builder, sizeof(ir_instr_break_t));
    built_instr->id = INSTRUCTION_BREAK;
    built_instr->result_type = NULL;
    built_instr->block_id = basicblock_id;
}

void build_cond_break(ir_builder_t *builder, ir_value_t *condition, length_t true_block_id, length_t false_block_id){
    ir_instr_cond_break_t *built_instr = (ir_instr_cond_break_t*) build_instruction(builder, sizeof(ir_instr_cond_break_t));
    built_instr->id = INSTRUCTION_CONDBREAK;
    built_instr->result_type = NULL;
    built_instr->value = condition;
    built_instr->true_block_id = true_block_id;
    built_instr->false_block_id = false_block_id;
}

ir_value_t *build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b){
    ir_instr_math_t *built_instr = (ir_instr_math_t*) build_instruction(builder, sizeof(ir_instr_math_t));
    built_instr->id = INSTRUCTION_EQUALS;
    built_instr->a = a;
    built_instr->b = b;
    built_instr->result_type = ir_builder_bool(builder);
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_array_access(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, source_t code_source){
    // NOTE: Array access is only for pointer values
    if(value->type->kind != TYPE_KIND_POINTER){
        redprintf("INTERNAL ERROR: build_array_access called on non-pointer value\n");
        redprintf("                (value left unmodified)");
        return value;
    }

    return build_array_access_ex(builder, value, index, value->type, code_source);
}

ir_value_t *build_array_access_ex(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, ir_type_t *result_type, source_t code_source){
    ir_instr_array_access_t *built_instr = (ir_instr_array_access_t*) build_instruction(builder, sizeof(ir_instr_array_access_t));
    built_instr->id = INSTRUCTION_ARRAY_ACCESS;
    built_instr->result_type = result_type;
    built_instr->value = value;
    built_instr->index = index;

    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        // If we're doing null checks, then give line/column to IR load instruction.
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &built_instr->maybe_line_number, &built_instr->maybe_column_number);
    }

    // Otherwise, we just ignore line/column fields
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_member(ir_builder_t *builder, ir_value_t *value, length_t member, ir_type_t *result_type, source_t code_source){
    ir_instr_member_t *built_instr = (ir_instr_member_t*) build_instruction(builder, sizeof(ir_instr_member_t));
    built_instr->id = INSTRUCTION_MEMBER;
    built_instr->result_type = result_type;
    built_instr->value = value;
    built_instr->member = member;

    if(builder->compiler->checks & COMPILER_NULL_CHECKS) {
        // If we're doing null checks, then give line/column to IR load instruction.
        lex_get_location(builder->compiler->objects[code_source.object_index]->buffer, code_source.index, &built_instr->maybe_line_number, &built_instr->maybe_column_number);
    }

    // Otherwise, we just ignore line/column fields
    return build_value_from_prev_instruction(builder);
}

void build_return(ir_builder_t *builder, ir_value_t *value){
    ir_instr_ret_t *built_instr = (ir_instr_ret_t*) build_instruction(builder, sizeof(ir_instr_ret_t));
    built_instr->id = INSTRUCTION_RET;
    built_instr->result_type = NULL;
    built_instr->value = value;
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

ir_value_t *build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_STRUCT_CONSTRUCTION;
    value->type = type;

    ir_value_struct_construction_t *extra = ir_pool_alloc(pool, sizeof(ir_value_struct_construction_t));
    extra->values = values;
    extra->length = length;
    value->extra = extra;

    return value;
}

ir_value_t *build_offsetof(ir_builder_t *builder, ir_type_t *type, length_t index){
    return build_offsetof_ex(builder->pool, ir_builder_usize(builder), type, index);
}

ir_value_t *build_offsetof_ex(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type, length_t index){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_OFFSETOF;
    value->type = usize_type;

    if(type->kind != TYPE_KIND_STRUCTURE){
        redprintf("INTENRAL ERROR: build_offsetof got non-struct type as type\n");
    }

    ir_value_offsetof_t *extra = ir_pool_alloc(pool, sizeof(ir_value_offsetof_t));
    extra->type = type;
    extra->index = index;
    value->extra = extra;
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
        *shared_type = ir_type_pointer_to(builder->pool, ir_builder_usize(builder));
    }

    return *shared_type;
}

ir_type_t* ir_builder_bool(ir_builder_t *builder){
    ir_type_t **shared_type = &builder->object->ir_module.common.ir_bool;

    if(*shared_type == NULL){
        (*shared_type) = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        (*shared_type)->kind = TYPE_KIND_BOOLEAN;
    }

    return *shared_type;
}

maybe_index_t ir_builder___types__(ir_builder_t *builder, source_t source_on_failure){
    if(builder->compiler->traits & COMPILER_NO_TYPEINFO){
        compiler_panic(builder->compiler, source_on_failure, "Runtime type information cannot be disabled because of this");
        return -1;
    }

    ir_module_t *ir_module = &builder->object->ir_module;

    switch(ir_module->common.has_rtti_array){
    case TROOLEAN_TRUE:
        return ir_module->common.rtti_array_index;
    case TROOLEAN_FALSE:
        compiler_panic(builder->compiler, source_on_failure, "Failed to find __types__ global variable");
        return -1;
    case TROOLEAN_UNKNOWN:
        // TODO: SPEED: It works, but the global variable lookup could be faster
        for(length_t index = 0; index != ir_module->globals_length; index++){
            if(strcmp("__types__", ir_module->globals[index].name) == 0){
                ir_module->common.rtti_array_index = index;
                ir_module->common.has_rtti_array = TROOLEAN_TRUE;
                return index;
            }
        }
        break;
    }

    ir_module->common.has_rtti_array = TROOLEAN_FALSE;
    compiler_panic(builder->compiler, source_on_failure, "Failed to find __types__ global variable");
    return -1;
}

ir_value_t *build_literal_int(ir_pool_t *pool, long long literal_value){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));

    value->value_type = VALUE_TYPE_LITERAL;
    value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    value->type->kind = TYPE_KIND_S32;
    // neglect ir_value->type->extra
    
    value->extra = ir_pool_alloc(pool, sizeof(long long));
    *((long long*) value->extra) = literal_value;
    return value;
}

ir_value_t *build_literal_usize(ir_pool_t *pool, length_t literal_value){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));

    value->value_type = VALUE_TYPE_LITERAL;
    value->type = ir_pool_alloc(pool, sizeof(ir_type_t));
    value->type->kind = TYPE_KIND_U64;
    // neglect ir_value->type->extra
    
    value->extra = ir_pool_alloc(pool, sizeof(unsigned long long));
    *((unsigned long long*) value->extra) = literal_value;
    return value;
}

ir_value_t *build_literal_str(ir_builder_t *builder, char *array, length_t length){
    if(builder->object->ir_module.common.ir_string_struct == NULL){
        redprintf("Can't create string literal without String type present");
        printf("\nTry importing '%s/String.adept'\n", ADEPT_VERSION_STRING);
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

ir_value_t *build_literal_cstr(ir_builder_t *builder, weak_cstr_t value){
    return build_literal_cstr_of_length(builder, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value){
    return build_literal_cstr_of_length_ex(pool, type_map, value, strlen(value) + 1);
}

ir_value_t *build_literal_cstr_of_length(ir_builder_t *builder, char *array, length_t length){
    return build_literal_cstr_of_length_ex(builder->pool, builder->type_map, array, length);
}

ir_value_t *build_literal_cstr_of_length_ex(ir_pool_t *pool, ir_type_map_t *type_map, char *array, length_t length){
    // NOTE: Builds a null-terminated string literal value
    ir_value_t *ir_value = ir_pool_alloc(pool, sizeof(ir_value_t));
    ir_value->value_type = VALUE_TYPE_CSTR_OF_LEN;
    ir_type_t *ubyte_type;

    if(!ir_type_map_find(type_map, "ubyte", &ubyte_type)){
        redprintf("INTERNAL ERROR: ir_type_map_find failed to find mapping for ubyte for build_literal_cstr_of_length_ex!\n");
        return NULL;
    }

    ir_value->type = ir_type_pointer_to(pool, ubyte_type);
    ir_value->extra = ir_pool_alloc(pool, sizeof(ir_value_cstr_of_len_t));
    ((ir_value_cstr_of_len_t*) ir_value->extra)->array = array;
    ((ir_value_cstr_of_len_t*) ir_value->extra)->length = length;
    return ir_value;
}

ir_value_t *build_null_pointer(ir_pool_t *pool){
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

ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_NULLPTR_OF_TYPE;
    value->type = type;
    // neglect value->extra
    return value;
}

ir_value_t *build_bitcast(ir_builder_t *builder, ir_value_t *from, ir_type_t* to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_BITCAST;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_const_bitcast(ir_pool_t *pool, ir_value_t *from, ir_type_t *to){
    ir_value_t *value = ir_pool_alloc(pool, sizeof(ir_value_t));
    value->value_type = VALUE_TYPE_CONST_BITCAST;
    value->type = to;
    value->extra = from;
    return value;
}

ir_value_t *build_zext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_ZEXT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_trunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_TRUNC;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_fext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FEXT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_ftrunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FTRUNC;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_inttoptr(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_INTTOPTR;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_ptrtoint(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_PTRTOINT;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_fptoui(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FPTOUI;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_fptosi(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_FPTOSI;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_uitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_UITOFP;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_sitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to){
    ir_instr_cast_t *instr = (ir_instr_cast_t*) build_instruction(builder, sizeof(ir_instr_cast_t));
    instr->id = INSTRUCTION_SITOFP;
    instr->result_type = to;
    instr->value = from;
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type){
    ir_instr_alloc_t *instr = (ir_instr_alloc_t*) build_instruction(builder, sizeof(ir_instr_alloc_t));
    instr->id = INSTRUCTION_ALLOC;
    instr->result_type = ir_type_pointer_to(builder->pool, type);
    return build_value_from_prev_instruction(builder);
}

ir_value_t *build_stack_save(ir_builder_t *builder){
    ir_instr_t *instr = (ir_instr_t*) build_instruction(builder, sizeof(ir_instr_t));
    instr->id = INSTRUCTION_STACK_SAVE;

    if(builder->stack_pointer_type == NULL){
        ir_type_t *i8_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        i8_type->kind = TYPE_KIND_S8;
        i8_type->extra = NULL;
        builder->stack_pointer_type = ir_pool_alloc(builder->pool, sizeof(ir_type_t));
        builder->stack_pointer_type->kind = TYPE_KIND_POINTER;
        builder->stack_pointer_type->extra = i8_type;
    }

    instr->result_type = builder->stack_pointer_type;
    return build_value_from_prev_instruction(builder);
}

void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer){
    ir_instr_stack_restore_t *instr = (ir_instr_stack_restore_t*) build_instruction(builder, sizeof(ir_instr_stack_restore_t));
    instr->id = INSTRUCTION_STACK_RESTORE;
    instr->stack_pointer = stack_pointer;
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

errorcode_t build_rtti_relocation(ir_builder_t *builder, strong_cstr_t human_notation, unsigned long long *id_ref, source_t source_on_failure){
    ir_module_t *ir_module = &builder->object->ir_module;
    expand((void**) &ir_module->rtti_relocations, sizeof(rtti_relocation_t), ir_module->rtti_relocations_length, &ir_module->rtti_relocations_capacity, 1, 4);

    rtti_relocation_t *relocation = &ir_module->rtti_relocations[ir_module->rtti_relocations_length++];
    relocation->human_notation = human_notation;
    relocation->id_ref = id_ref;
    relocation->source_on_failure = source_on_failure;
    return SUCCESS;
}

void prepare_for_new_label(ir_builder_t *builder){
    if(builder->block_stack_capacity == 0){
        builder->block_stack_labels = malloc(sizeof(char*) * 4);
        builder->block_stack_break_ids = malloc(sizeof(length_t) * 4);
        builder->block_stack_continue_ids = malloc(sizeof(length_t) * 4);
        builder->block_stack_scopes = malloc(sizeof(bridge_scope_t*) * 4);
        builder->block_stack_capacity = 4;
    } else if(builder->block_stack_length == builder->block_stack_capacity){
        builder->block_stack_capacity *= 2;
        char **new_block_stack_labels = malloc(sizeof(char*) * builder->block_stack_capacity);
        length_t *new_block_stack_break_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        length_t *new_block_stack_continue_ids = malloc(sizeof(length_t) * builder->block_stack_capacity);
        bridge_scope_t **new_block_stack_scopes = malloc(sizeof(bridge_scope_t*) * builder->block_stack_capacity);
        memcpy(new_block_stack_labels, builder->block_stack_labels, sizeof(char*) * builder->block_stack_length);
        memcpy(new_block_stack_break_ids, builder->block_stack_break_ids, sizeof(length_t) * builder->block_stack_length);
        memcpy(new_block_stack_continue_ids, builder->block_stack_continue_ids, sizeof(length_t) * builder->block_stack_length);
        memcpy(new_block_stack_scopes, builder->block_stack_scopes, sizeof(bridge_scope_t*) * builder->block_stack_length);
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

void open_scope(ir_builder_t *builder){
    bridge_scope_t *old_scope = builder->scope;
    bridge_scope_t *new_scope = malloc(sizeof(bridge_scope_t));
    bridge_scope_init(new_scope, old_scope);
    new_scope->first_var_id = builder->next_var_id;

    expand((void**) &old_scope->children, sizeof(bridge_scope_t*), old_scope->children_length, &old_scope->children_capacity, 1, 4);
    old_scope->children[old_scope->children_length++] = new_scope;
    builder->scope = new_scope;
}

void close_scope(ir_builder_t *builder){
    builder->scope->following_var_id = builder->next_var_id;
    builder->scope = builder->scope->parent;
    
    if(builder->scope == NULL)
        redprintf("INTERNAL ERROR: TRIED TO CLOSE BRIDGE SCOPE WITH NO PARENT, probably will crash...\n");
}

void add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits){
    bridge_var_list_t *list = &builder->scope->list;
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
        bridge_var_t *variable = &list->variables[i];

        // Don't perform defer management on POD variables or non-struct types
        trait_t traits = variable->traits;
        if(traits & BRIDGE_VAR_POD || traits & BRIDGE_VAR_REFERENCE ||
            !(variable->ir_type->kind == TYPE_KIND_STRUCTURE || variable->ir_type->kind == TYPE_KIND_FIXED_ARRAY)) continue;

        // Ensure we're working with a single AST type element in the type for structs
        if(variable->ir_type->kind == TYPE_KIND_STRUCTURE && variable->ast_type->elements_length != 1) continue;

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

errorcode_t handle_deference_for_globals(ir_builder_t *builder){
    ast_global_t *globals = builder->object->ast.globals;
    length_t globals_length = builder->object->ast.globals_length;

    for(length_t i = 0; i != globals_length; i++){
        ast_global_t *global = &globals[i];

        // Don't perform defer management on POD variables or non-struct types or special globals
        trait_t traits = global->traits;
        
        // Faster way of doing: (traits & AST_GLOBAL_EXTERNAL || traits & AST_GLOBAL_POD || traits & AST_GLOBAL_SPECIAL)
        if(traits != TRAIT_NONE) continue;

        // Ensure we're working with a single AST type element in the type
        if(global->type.elements_length != 1) continue;

        // Capture snapshot of current pool state (for if we need to revert allocations)
        ir_pool_snapshot_t snapshot;
        ir_pool_snapshot_capture(builder->pool, &snapshot);

        ir_type_t *ir_type;
        if(ir_gen_resolve_type(builder->compiler, builder->object, &global->type, &ir_type)){
            // Revert recent pool allocations
            ir_pool_snapshot_restore(builder->pool, &snapshot);
            return FAILURE;
        }

        // WARNING: Assuming i is the same as the global variable id
        ir_value_t *ir_value = build_gvarptr(builder, ir_type, i);
        errorcode_t failed = handle_single_deference(builder, &global->type, ir_value);

        if(failed){
            // Remove GVARPTR instruction
            builder->current_block->instructions_length--;

            // Revert recent pool allocations
            ir_pool_snapshot_restore(builder->pool, &snapshot);

            // Real failure if a compile time error occured
            if(failed == ALT_FAILURE) return FAILURE;
        }
    }
    return SUCCESS;
}

errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value){
    // Calls __defer__ method on a mutable value and it's children if the method exists
    // NOTE: Assumes (ast_type->elements_length >= 1)
    // NOTE: Returns SUCCESS if mutable_value was utilized in deference
    //       Returns FAILURE if mutable_value was not utilized in deference
    //       Returns ALT_FAILURE if a compiler time error occured
    // NOTE: This function is not allowed to generate or switch basicblocks!

    funcpair_t defer_func;
    ir_pool_snapshot_t pool_snapshot;
    ir_pool_snapshot_capture(builder->pool, &pool_snapshot);

    ir_value_t **arguments = NULL;

    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE: case AST_ELEM_GENERIC_BASE: {
            arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*));
            arguments[0] = mutable_value;

            if(ir_gen_find_defer_func(builder, arguments, ast_type, &defer_func)){
                ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
                return FAILURE;
            }
        }
        break;
    case AST_ELEM_FIXED_ARRAY: {
            ast_type_t temporary_rest_of_type;
            temporary_rest_of_type.elements = &ast_type->elements[1];
            temporary_rest_of_type.elements_length = ast_type->elements_length - 1;
            temporary_rest_of_type.source = ast_type->source;

            if(!could_have_deference(&temporary_rest_of_type)){
                ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
                return FAILURE;
            }

            // Record the number of items of the fixed array
            length_t count = ((ast_elem_fixed_array_t*) ast_type->elements[0])->length;

            // Take snapshot of instruction count
            length_t before_modification_instructions_length = builder->current_block->instructions_length;

            // Bitcast to '*T' from '*n T'
            // NOTE: Assumes that the IR type of 'mutable_value' is a pointer to a fixed array
            assert(mutable_value->type->kind == TYPE_KIND_POINTER);
            assert(((ir_type_t*) mutable_value->type->extra)->kind == TYPE_KIND_FIXED_ARRAY);

            ir_type_t *casted_ir_type = ir_type_pointer_to(builder->pool, ((ir_type_extra_fixed_array_t*) ((ir_type_t*) mutable_value->type->extra)->extra)->subtype);
            mutable_value = build_bitcast(builder, mutable_value, casted_ir_type);

            for(length_t i = 0; i != count; i++){
                // Call handle_single_dereference() on each item else restore snapshots

                // Access at 'i'
                ir_value_t *mutable_item_value = build_array_access(builder, mutable_value, build_literal_usize(builder->pool, i), ast_type->source);
                
                // Handle deference for that single item
                errorcode_t res = handle_single_deference(builder, &temporary_rest_of_type, mutable_item_value);

                if(res){
                    ir_pool_snapshot_restore(builder->pool, &pool_snapshot);

                    // Restore basicblock to previous amount of instructions
                    // NOTE: This is only okay because this function 'handle_single_dereference' cannot generate new basicblocks
                    builder->current_block->instructions_length = before_modification_instructions_length;
                    return res;
                }
            }
        }
        return SUCCESS;
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
                
                ir_type_t *ir_field_type, *this_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)
                ){
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_varptr(builder, ir_type_pointer_to(builder->pool, this_ir_type), 0), this_ast_type->source);

                ir_value_t *ir_field_value = build_member(builder, this_ir_value, f, ir_type_pointer_to(builder->pool, ir_field_type), this_ast_type->source);
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

                if(resolve_type_polymorphics(builder->compiler, builder->type_table, &catalog, ast_unresolved_field_type, &ast_field_type)){
                    ast_type_var_catalog_free(&catalog);
                    return ALT_FAILURE;
                }

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);
                
                ir_type_t *ir_field_type, *this_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, this_ast_type, &this_ir_type)
                ){
                    ast_type_var_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                ir_value_t *this_ir_value = build_load(builder, build_varptr(builder, ir_type_pointer_to(builder->pool, this_ir_type), 0), this_ast_type->source);

                ir_value_t *ir_field_value = build_member(builder, this_ir_value, f, ir_type_pointer_to(builder->pool, ir_field_type), this_ast_type->source);
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

bool could_have_deference(ast_type_t *ast_type){
    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE:
    case AST_ELEM_FIXED_ARRAY:
        return true;
    }
    return false;
}

errorcode_t handle_pass_management(ir_builder_t *builder, ir_value_t **values, ast_type_t *types, trait_t *arg_type_traits, length_t arity){
    for(length_t i = 0; i != arity; i++){
        if(arg_type_traits != NULL && arg_type_traits[i] & AST_FUNC_ARG_TYPE_TRAIT_POD) continue;
        
        unsigned int value_type_kind = values[i]->type->kind;

        if(value_type_kind == TYPE_KIND_STRUCTURE || value_type_kind == TYPE_KIND_FIXED_ARRAY){
            ast_type_t *ast_type = &types[i];
            unsigned int elem_id = ast_type->elements[0]->id;

            if((ast_type->elements_length == 1 && (elem_id == AST_ELEM_BASE || elem_id == AST_ELEM_GENERIC_BASE)) || elem_id == AST_ELEM_FIXED_ARRAY){
                ir_pool_snapshot_t snapshot;
                ir_pool_snapshot_capture(builder->pool, &snapshot);

                funcpair_t result;
                ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*));
                arguments[0] = values[i];

                errorcode_t errorcode = ir_gen_find_pass_func(builder, arguments, ast_type, &result);
                if(errorcode == ALT_FAILURE) return FAILURE;

                if(errorcode == FAILURE){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    continue;
                }

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

    return SUCCESS;
}

errorcode_t handle_single_pass(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value){
    // Calls pass__ method on a mutable value and it's children if the method exists
    // NOTE: Assumes (ast_type->elements_length >= 1)
    // NOTE: Returns SUCCESS if mutable_value was utilized in deference
    //       Returns FAILURE if mutable_value was not utilized in deference
    //       Returns ALT_FAILURE if a compiler time error occured
    // NOTE: This function is not allowed to generate or switch basicblocks!

    funcpair_t pass_func;

    ir_pool_snapshot_t pool_snapshot;
    ir_pool_snapshot_capture(builder->pool, &pool_snapshot);

    ir_value_t **arguments = NULL;
    unsigned int elem_id = ast_type->elements[0]->id;

    switch(elem_id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE: {
            arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*));
            arguments[0] = build_load(builder, mutable_value, ast_type->source);

            if(ir_gen_find_func_conforming(builder, "__pass__", arguments, ast_type, 1, &pass_func)){
                ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
                return FAILURE;
            }
        }
        break;
    case AST_ELEM_FIXED_ARRAY: {
            ast_type_t temporary_rest_of_type;
            temporary_rest_of_type.elements = &ast_type->elements[1];
            temporary_rest_of_type.elements_length = ast_type->elements_length - 1;
            temporary_rest_of_type.source = ast_type->source;

            if(!could_have_pass(&temporary_rest_of_type)){
                ir_pool_snapshot_restore(builder->pool, &pool_snapshot);
                return FAILURE;
            }

            // Record the number of items of the fixed array
            length_t count = ((ast_elem_fixed_array_t*) ast_type->elements[0])->length;

            // Take snapshot of instruction count
            length_t before_modification_instructions_length = builder->current_block->instructions_length;

            // Bitcast to '*T' from '*n T'
            // NOTE: Assumes that the IR type of 'mutable_value' is a pointer to a fixed array
            assert(mutable_value->type->kind == TYPE_KIND_POINTER);
            assert(((ir_type_t*) mutable_value->type->extra)->kind == TYPE_KIND_FIXED_ARRAY);

            ir_type_t *casted_ir_type = ir_type_pointer_to(builder->pool, ((ir_type_extra_fixed_array_t*) ((ir_type_t*) mutable_value->type->extra)->extra)->subtype);
            mutable_value = build_bitcast(builder, mutable_value, casted_ir_type);

            for(length_t i = 0; i != count; i++){
                // Call handle_single_pass() on each item else restore snapshots

                // Access at 'i'
                ir_value_t *mutable_item_value = build_array_access(builder, mutable_value, build_literal_usize(builder->pool, i), ast_type->source);
                
                // Handle passing for that single item
                errorcode_t res = handle_single_pass(builder, &temporary_rest_of_type, mutable_item_value);

                if(res){
                    ir_pool_snapshot_restore(builder->pool, &pool_snapshot);

                    // Restore basicblock to previous amount of instructions
                    // NOTE: This is only okay because this function 'handle_single_dereference' cannot generate new basicblocks
                    builder->current_block->instructions_length = before_modification_instructions_length;
                    return res;
                }
            }
        }
        return SUCCESS;
    default:
        return FAILURE;
    }

    // Call __pass__()
    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
    instruction->id = INSTRUCTION_CALL;
    instruction->result_type = builder->object->ir_module.funcs[pass_func.ir_func_id].return_type;
    instruction->values = arguments;
    instruction->values_length = 1;
    instruction->ir_func_id = pass_func.ir_func_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction; 
    ir_value_t *passed = build_value_from_prev_instruction(builder);

    // Store result back into mutable value
    build_store(builder, passed, mutable_value, ast_type->source);
    return SUCCESS;
}


errorcode_t handle_children_pass_root(ir_builder_t *builder, bool already_has_return){
    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *autogen_func = &builder->object->ast.funcs[builder->ast_func_id];

    errorcode_t res = handle_children_pass(builder);
    if(res) return res;

    ast_type_t *passed_ast_type = autogen_func->arity == 1 ? &autogen_func->arg_types[0] : NULL;
    if(passed_ast_type == NULL) return FAILURE;

    ir_type_t *passed_ir_type;
    if(ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)){
        return FAILURE;
    }

    ir_value_t *variable_reference = build_varptr(builder, ir_type_pointer_to(builder->pool, passed_ir_type), 0);
    ir_value_t *return_value = build_load(builder, variable_reference, passed_ast_type->source);

    // Return modified value
    if(!already_has_return){
        build_return(builder, return_value);
    }
    return SUCCESS;
}

errorcode_t handle_children_pass(ir_builder_t *builder){
    // Generates __pass__ calls for children of the parent type of a __pass__ function

    // DANGEROUS: 'func' could be invalidated by generation of new functions
    ast_func_t *func = &builder->object->ast.funcs[builder->ast_func_id];
    ast_type_t *passed_ast_type = func->arity == 1 ? &func->arg_types[0] : NULL;
    ir_pool_snapshot_t snapshot;

    if(passed_ast_type == NULL || passed_ast_type->elements_length < 1){
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_pass() encountered unrecognzied format of __pass__ management method");
        return FAILURE;
    }

    // Don't call __pass__ for parameters marked as plain-old data
    if(func->arg_type_traits[0] & BRIDGE_VAR_POD) return SUCCESS;

    ast_elem_t *first_elem = passed_ast_type->elements[0];

    // Generate code for __pass__ function
    switch(first_elem->id){
    case AST_ELEM_BASE: {
            weak_cstr_t struct_name = ((ast_elem_base_t*) first_elem)->base;

            // Attempt to call __pass__ on members
            ast_struct_t *ast_struct = ast_struct_find(&builder->object->ast, struct_name);

            // Don't bother with structs that don't exist
            if(ast_struct == NULL) return FAILURE;

            for(length_t f = 0; f != ast_struct->field_count; f++){
                ast_type_t *ast_field_type = &ast_struct->field_types[f];

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);
                
                ir_type_t *ir_field_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    return ALT_FAILURE;
                }

                if(ir_field_type->kind != TYPE_KIND_STRUCTURE && ir_field_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    continue;
                }

                ir_value_t *mutable_passed_ir_value = build_varptr(builder, ir_type_pointer_to(builder->pool, passed_ir_type), 0);

                ir_value_t *ir_field_reference = build_member(builder, mutable_passed_ir_value, f, ir_type_pointer_to(builder->pool, ir_field_type), ast_field_type->source);
                errorcode_t failed = handle_single_pass(builder, ast_field_type, ir_field_reference);

                if(failed){
                    // Remove VARPTR and MEMBER instruction
                    builder->current_block->instructions_length -= 2;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propogate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) first_elem;
            weak_cstr_t template_name = generic_base->name;

            // Attempt to call __pass__ on members
            ast_polymorphic_struct_t *template = ast_polymorphic_struct_find(&builder->object->ast, template_name);

            // Don't bother with polymorphic structs that don't exist
            if(template == NULL) return FAILURE;

            // Substitution Catalog
            ast_type_var_catalog_t catalog;
            ast_type_var_catalog_init(&catalog);

            if(template->generics_length != generic_base->generics_length){
                redprintf("INTERNAL ERROR: Polymorphic struct '%s' type parameter length mismatch when generating child passing!\n", generic_base->name);
                ast_type_var_catalog_free(&catalog);
                return ALT_FAILURE;
            }

            for(length_t i = 0; i != template->generics_length; i++){
                ast_type_var_catalog_add(&catalog, template->generics[i], &generic_base->generics[i]);
            }

            for(length_t f = 0; f != template->field_count; f++){
                ast_type_t *ast_unresolved_field_type = &template->field_types[f];
                ast_type_t ast_field_type;

                if(resolve_type_polymorphics(builder->compiler, builder->type_table, &catalog, ast_unresolved_field_type, &ast_field_type)){
                    ast_type_var_catalog_free(&catalog);
                    return ALT_FAILURE;
                }

                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);

                ir_type_t *ir_field_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_field_type, &ir_field_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    ast_type_var_catalog_free(&catalog);
                    ast_type_free(&ast_field_type);
                    return ALT_FAILURE;
                }

                if(ir_field_type->kind != TYPE_KIND_STRUCTURE && ir_field_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    ast_type_free(&ast_field_type);
                    continue;
                }

                ir_value_t *mutable_passed_ir_value = build_varptr(builder, ir_type_pointer_to(builder->pool, passed_ir_type), 0);

                ir_value_t *ir_field_reference = build_member(builder, mutable_passed_ir_value, f, ir_type_pointer_to(builder->pool, ir_field_type), ast_field_type.source);
                errorcode_t failed = handle_single_pass(builder, &ast_field_type, ir_field_reference);
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
    case AST_ELEM_FIXED_ARRAY: {
            if(passed_ast_type->elements_length < 2){
                compiler_panic(builder->compiler, passed_ast_type->source, "INTERNAL ERROR: AST_ELEM_FIXED_ARRAY of handle_children_pass only got one element in type");
                return ALT_FAILURE;
            }
            
            // Attempt to call __pass__ on elements
            length_t fixed_count = ((ast_elem_fixed_array_t*) first_elem)->length;

            ast_type_t ast_element_type;
            ast_element_type.elements = &passed_ast_type->elements[1];
            ast_element_type.elements_length = passed_ast_type->elements_length - 1;
            ast_element_type.source = first_elem->source;

            // OPTIMIZATION: SPEED: CLEANUP: Reduce the number of generated instructions for this
            for(length_t e = 0; e != fixed_count; e++){
                // Capture snapshot for if we need to backtrack
                ir_pool_snapshot_capture(builder->pool, &snapshot);

                ir_type_t *ir_element_type, *passed_ir_type;
                if(
                    ir_gen_resolve_type(builder->compiler, builder->object, &ast_element_type, &ir_element_type) ||
                    ir_gen_resolve_type(builder->compiler, builder->object, passed_ast_type, &passed_ir_type)
                ){
                    return ALT_FAILURE;
                }

                if(ir_element_type->kind != TYPE_KIND_STRUCTURE && ir_element_type->kind != TYPE_KIND_FIXED_ARRAY){
                    ir_pool_snapshot_restore(builder->pool, &snapshot);
                    continue;
                }

                // Get type of pointer to element
                ir_type_t *ir_element_ptr_type = ir_type_pointer_to(builder->pool, ir_element_type);

                // Get pointer to elements of fixed array variable
                ir_value_t *mutable_passed_ir_value = build_varptr(builder, ir_type_pointer_to(builder->pool, passed_ir_type), 0);
                mutable_passed_ir_value = build_bitcast(builder, mutable_passed_ir_value, ir_element_ptr_type);

                // Access 'e'th element
                ir_value_t *ir_element_reference = build_array_access(builder, mutable_passed_ir_value, build_literal_usize(builder->pool, e), ast_element_type.source);

                // Handle passing for that element
                errorcode_t failed = handle_single_pass(builder, &ast_element_type, ir_element_reference);

                if(failed){
                    // Remove VARPTR and BITCAST and ARRAY_ACCESS instructions
                    builder->current_block->instructions_length -= 3;

                    // Revert recent pool allocations
                    ir_pool_snapshot_restore(builder->pool, &snapshot);

                    // Propogate alternate failure cause
                    if(failed == ALT_FAILURE) return ALT_FAILURE;
                }
            }
        }
        break;
    default:
        compiler_panicf(builder->compiler, func->source, "INTERNAL ERROR: handle_children_pass() encountered unrecognzied argument types of __pass__ management method");
        return FAILURE;
    }

    return SUCCESS;
}

bool could_have_pass(ast_type_t *ast_type){
    switch(ast_type->elements[0]->id){
    case AST_ELEM_BASE:
    case AST_ELEM_GENERIC_BASE:
    case AST_ELEM_FIXED_ARRAY:
        return true;
    }
    return false;
}

successful_t handle_assign_management(ir_builder_t *builder, ir_value_t *value, ast_type_t *ast_value_type, ir_value_t *destination,
        ast_type_t *ast_destination_type, bool zero_initialize){

    if(value->type->kind != TYPE_KIND_STRUCTURE || ast_destination_type->elements_length != 1) return UNSUCCESSFUL;

    funcpair_t result;
    weak_cstr_t struct_name;

    ir_pool_snapshot_t snapshot;
    ir_pool_snapshot_capture(builder->pool, &snapshot);
    ir_value_t **arguments = ir_pool_alloc(builder->pool, sizeof(ir_value_t*) * 2);
    arguments[0] = destination;
    arguments[1] = value;

    ast_type_t arg_types[2];
    arg_types[0] = ast_type_clone(ast_destination_type);
    arg_types[1] = *ast_value_type;
    ast_type_prepend_ptr(&arg_types[0]);

    switch(ast_destination_type->elements[0]->id){
    case AST_ELEM_BASE:
        struct_name = ((ast_elem_base_t*) ast_destination_type->elements[0])->base;

        if(ir_gen_find_method_conforming(builder, struct_name, "__assign__", arguments, arg_types, 2, &result) == FAILURE){
            ir_pool_snapshot_restore(builder->pool, &snapshot);
            ast_type_free(&arg_types[0]);
            // NOTE: Don't free arg_types[1] because we don't have ownership
            return UNSUCCESSFUL;
        }
        break;
    case AST_ELEM_GENERIC_BASE:
        struct_name = ((ast_elem_generic_base_t*) ast_destination_type->elements[0])->name;

        if(ir_gen_find_generic_base_method_conforming(builder, struct_name, "__assign__", arguments, arg_types, 2, &result) == FAILURE){
            ir_pool_snapshot_restore(builder->pool, &snapshot);
            ast_type_free(&arg_types[0]);
            // NOTE: Don't free arg_types[1] because we don't have ownership
            return UNSUCCESSFUL;
        }
        break;
    default:
        ir_pool_snapshot_restore(builder->pool, &snapshot);
        ast_type_free(&arg_types[0]);
        // NOTE: Don't free arg_types[1] because we don't have ownership
        return UNSUCCESSFUL;
    }

    if(zero_initialize){
        // Zero initialize for declaration assignments
        ir_basicblock_new_instructions(builder->current_block, 1);
        ir_instr_varzeroinit_t *zero_instr = (ir_instr_varzeroinit_t*) ir_pool_alloc(builder->pool, sizeof(ir_instr_varzeroinit_t));
        zero_instr->id = INSTRUCTION_VARZEROINIT;
        zero_instr->result_type = NULL;
        zero_instr->index = builder->next_var_id - 1;
        builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) zero_instr;
    }

    if(handle_pass_management(builder, arguments, arg_types, result.ast_func->arg_type_traits, 2)){
        ast_type_free(&arg_types[0]);
        // NOTE: Don't free arg_types[1] because we don't have ownership
        return NULL;
    }

    ast_type_free(&arg_types[0]);
    // NOTE: Don't free arg_types[1] because we don't have ownership

    ir_basicblock_new_instructions(builder->current_block, 1);
    ir_instr_call_t *instruction = ir_pool_alloc(builder->pool, sizeof(ir_instr_call_t));
    instruction->id = INSTRUCTION_CALL;
    instruction->result_type = builder->object->ir_module.funcs[result.ir_func_id].return_type;
    instruction->values = arguments;
    instruction->values_length = 2;
    instruction->ir_func_id = result.ir_func_id;
    builder->current_block->instructions[builder->current_block->instructions_length++] = (ir_instr_t*) instruction;
    return SUCCESSFUL;
}

ir_value_t *handle_math_management(ir_builder_t *builder, ir_value_t *lhs, ir_value_t *rhs, ast_type_t *lhs_type, ast_type_t *rhs_type, ast_type_t *out_type, const char *overload_name){
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

            if(handle_pass_management(builder, arguments, types, result.ast_func->arg_type_traits, 2)){
                return NULL;
            }

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
    
    if(types_length != poly_func->arity && !(poly_func->traits & AST_FUNC_VARARG)) return FAILURE;

    ast_t *ast = &builder->object->ast;
    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    ast_func_create_template(func, strclone(poly_func->name), poly_func->traits & AST_FUNC_STDCALL, false, !(poly_func->traits & AST_FUNC_AUTOGEN), poly_func->source);
    if(poly_func->traits & AST_FUNC_VARARG) func->traits |= AST_FUNC_VARARG;
    func->traits |= AST_FUNC_GENERATED;

    func->arg_names = malloc(sizeof(weak_cstr_t) * poly_func->arity);
    func->arg_types = malloc(sizeof(ast_type_t) * poly_func->arity);
    func->arg_sources = malloc(sizeof(source_t) * poly_func->arity);
    func->arg_flows = malloc(sizeof(char) * poly_func->arity);
    func->arg_type_traits = malloc(sizeof(trait_t) * poly_func->arity);

    for(length_t i = 0; i != poly_func->arity; i++){
        ast_type_t *template_ast_type = &poly_func->arg_types[i];
        func->arg_names[i] = strclone(poly_func->arg_names[i]);
        func->arg_types[i] = ast_type_has_polymorph(template_ast_type) ? ast_type_clone(&types[i]) : ast_type_clone(template_ast_type);
    }

    memcpy(func->arg_sources, poly_func->arg_sources, sizeof(source_t) * poly_func->arity);
    memcpy(func->arg_flows, poly_func->arg_flows, sizeof(char) * poly_func->arity);
    memcpy(func->arg_type_traits, poly_func->arg_type_traits, sizeof(trait_t) * poly_func->arity);

    func->arity = poly_func->arity;
    if(resolve_type_polymorphics(builder->compiler, builder->type_table, catalog, &poly_func->return_type, &func->return_type)){
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
        if(resolve_expr_polymorphics(builder->compiler, builder->type_table, catalog, func->statements[s])) return FAILURE;
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

errorcode_t attempt_autogen___defer__(ir_builder_t *builder, ast_type_t *arg_types,
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
    ast_func_create_template(func, strclone("__defer__"), false, false, false, NULL_SOURCE);
    func->traits |= AST_FUNC_AUTOGEN | AST_FUNC_GENERATED;

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

errorcode_t attempt_autogen___pass__(ir_builder_t *builder, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result){
    if(type_list_length != 1) return FAILURE; // Require single argument for auto-generated __pass__

    bool is_base = ast_type_is_base(&arg_types[0]);
    bool is_generic_base = is_base ? false : ast_type_is_generic_base(&arg_types[0]);
    bool is_fixed_array = ast_type_is_fixed_array(&arg_types[0]);

    if(!(is_base || is_generic_base || is_fixed_array)){
        return FAILURE; // Require 'Type' or '<...> Type' for 'this' type
    }

    ast_t *ast = &builder->object->ast;
    ir_module_t *module = &builder->object->ir_module;

    if(is_base){
        weak_cstr_t struct_name = ((ast_elem_base_t*) arg_types[0].elements[0])->base;
        if(ast_struct_find(ast, struct_name) == NULL) return FAILURE; // Require structure to exist
    } else if(is_generic_base){
        ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) arg_types[0].elements[0];
        weak_cstr_t struct_name = generic_base->name;
        ast_polymorphic_struct_t *polymorphic_struct = ast_polymorphic_struct_find(ast, struct_name);
        if(polymorphic_struct == NULL) return FAILURE; // Require generic structure to exist
        if(polymorphic_struct->generics_length != generic_base->generics_length) return FAILURE; // Require generics count to match
    }

    // Create function for autogen'd __pass__ function
    /*
    func __pass__(passed POD Passed) Passed {
        // ... to be generated ...
    }
    */

    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    ast_func_create_template(func, strclone("__pass__"), false, false, false, NULL_SOURCE);
    func->traits |= AST_FUNC_AUTOGEN | AST_FUNC_GENERATED;

    func->arg_names = malloc(sizeof(weak_cstr_t) * 1);
    func->arg_types = malloc(sizeof(ast_type_t));
    func->arg_sources = malloc(sizeof(source_t));
    func->arg_flows = malloc(sizeof(char));
    func->arg_type_traits = malloc(sizeof(trait_t));

    func->arg_names[0] = strclone("passed");
    func->arg_types[0] = ast_type_clone(&arg_types[0]);
    func->arg_sources[0] = NULL_SOURCE;
    func->arg_flows[0] = FLOW_IN;
    func->arg_type_traits[0] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    func->arity = 1;

    func->statements_length = 0;
    func->statements_capacity = 0;
    func->statements = NULL;

    func->return_type = ast_type_clone(&arg_types[0]);

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

errorcode_t resolve_type_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_type_var_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type){
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
                    if(resolve_type_polymorphics(compiler, type_table, catalog, &func->arg_types[i], &resolved->arg_types[i])){
                        ast_types_free_fully(resolved->arg_types, i);
                        free(resolved->return_type);
                        free(resolved);
                        return FAILURE;
                    }
                }

                if(resolve_type_polymorphics(compiler, type_table, catalog, func->return_type, resolved->return_type)){
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
                    if(resolve_type_polymorphics(compiler, type_table, catalog, &generic_base_elem->generics[i], &resolved[i])){
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

    // If output destination is the same place as the input location, clear input before writing
    if(out_type == NULL)    out_type = in_type;
    if(out_type == in_type) ast_type_free(in_type);

    out_type->elements = elements;
    out_type->elements_length = length;
    out_type->source = in_type->source;

    // Since we are past the parsing phase, we can check that RTTI isn't disabled
    if(type_table && !(compiler->traits & COMPILER_NO_TYPEINFO)){
        type_table_give(type_table, out_type, NULL);
    }
    return SUCCESS;
}

errorcode_t resolve_expr_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_type_var_catalog_t *catalog, ast_expr_t *expr){
    switch(expr->id){
    case EXPR_RETURN: {
            ast_expr_return_t *return_stmt = (ast_expr_return_t*) expr;
            if(return_stmt->value != NULL && resolve_expr_polymorphics(compiler, type_table, catalog, return_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_CALL: {
            ast_expr_call_t *call_stmt = (ast_expr_call_t*) expr;
            for(length_t a = 0; a != call_stmt->arity; a++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, call_stmt->args[a])) return FAILURE;
            }
        }
        break;
    case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
            ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) expr;

            if(resolve_type_polymorphics(compiler, type_table, catalog, &declare_stmt->type, NULL)) return FAILURE;

            if(declare_stmt->value != NULL){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, declare_stmt->value)) return FAILURE;
            }
        }
        break;
    case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
    case EXPR_LS_ASSIGN: case EXPR_RS_ASSIGN:
    case EXPR_LGC_LS_ASSIGN: case EXPR_LGC_RS_ASSIGN: {
            ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, assign_stmt->destination)
            || resolve_expr_polymorphics(compiler, type_table, catalog, assign_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
            ast_expr_if_t *conditional = (ast_expr_if_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, conditional->value)) return FAILURE;

            for(length_t i = 0; i != conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, conditional->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_IFELSE: case EXPR_UNLESSELSE: {
            ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, complex_conditional->value)) return FAILURE;

            for(length_t i = 0; i != complex_conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, complex_conditional->statements[i])) return FAILURE;
            }

            for(length_t i = 0; i != complex_conditional->else_statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, complex_conditional->else_statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
            ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) expr;

            for(length_t i = 0; i != conditional->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, conditional->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, call_stmt->value)) return FAILURE;

            for(length_t a = 0; a != call_stmt->arity; a++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, call_stmt->args[a])) return FAILURE;
            }
        }
        break;
    case EXPR_DELETE: {
            ast_expr_unary_t *delete_stmt = (ast_expr_unary_t*) expr;
            if(resolve_expr_polymorphics(compiler, type_table, catalog, delete_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_EACH_IN: {
            ast_expr_each_in_t *loop = (ast_expr_each_in_t*) expr;

            if(resolve_type_polymorphics(compiler, type_table, catalog, loop->it_type, NULL)
            || (loop->low_array && resolve_expr_polymorphics(compiler, type_table, catalog, loop->low_array))
            || (loop->length && resolve_expr_polymorphics(compiler, type_table, catalog, loop->length))
            || (loop->list && resolve_expr_polymorphics(compiler, type_table, catalog, loop->list))) return FAILURE;

            for(length_t i = 0; i != loop->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, loop->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_REPEAT: {
            ast_expr_repeat_t *loop = (ast_expr_repeat_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, loop->limit)) return FAILURE;

            for(length_t i = 0; i != loop->statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, loop->statements[i])) return FAILURE;
            }
        }
        break;
    case EXPR_SWITCH: {
            ast_expr_switch_t *switch_stmt = (ast_expr_switch_t*) expr;

            if(resolve_expr_polymorphics(compiler, type_table, catalog, switch_stmt->value)) return FAILURE;

            for(length_t i = 0; i != switch_stmt->default_statements_length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, switch_stmt->default_statements[i])) return FAILURE;
            }

            for(length_t c = 0; c != switch_stmt->cases_length; c++){
                ast_case_t *expr_case = &switch_stmt->cases[c];

                if(resolve_expr_polymorphics(compiler, type_table, catalog, expr_case->condition)) return FAILURE;

                for(length_t i = 0; i != expr_case->statements_length; i++){
                    if(resolve_expr_polymorphics(compiler, type_table, catalog, expr_case->statements[i])) return FAILURE;
                }
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
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_math_t*) expr)->a)
        || resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_math_t*) expr)->b)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_member_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) expr;

            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(resolve_type_polymorphics(compiler, type_table, catalog, &func_addr->match_args[a], NULL)) return FAILURE;
            }
        }
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_array_access_t*) expr)->index)) return FAILURE;
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_array_access_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_CAST:
        if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_cast_t*) expr)->to, NULL)) return FAILURE;
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_cast_t*) expr)->from)) return FAILURE;
        break;
    case EXPR_SIZEOF:
        if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_sizeof_t*) expr)->type, NULL)) return FAILURE;
        break;
    case EXPR_NEW:
        if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_new_t*) expr)->type, NULL)) return FAILURE;
        if(((ast_expr_new_t*) expr)->amount != NULL && resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_new_t*) expr)->amount)) return FAILURE;
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
        if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_STATIC_ARRAY: {
            if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_static_data_t*) expr)->type, NULL)) return FAILURE;

            for(length_t i = 0; i != ((ast_expr_static_data_t*) expr)->length; i++){
                if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_static_data_t*) expr)->values[i])) return FAILURE;
            }
        }
        break;
    case EXPR_STATIC_STRUCT: {
            if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_static_data_t*) expr)->type, NULL)) return FAILURE;
        }
        break;
    case EXPR_TYPEINFO: {
            if(resolve_type_polymorphics(compiler, type_table, catalog, &((ast_expr_typeinfo_t*) expr)->target, NULL)) return FAILURE;
        }
        break;
    case EXPR_TERNARY: {
            if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_ternary_t*) expr)->condition)) return FAILURE;
            if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_ternary_t*) expr)->if_true))   return FAILURE;
            if(resolve_expr_polymorphics(compiler, type_table, catalog, ((ast_expr_ternary_t*) expr)->if_false))  return FAILURE;
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = (ast_expr_inline_declare_t*) expr;

            if(resolve_type_polymorphics(compiler, type_table, catalog, &def->type, NULL)
            ||(def->value != NULL && resolve_expr_polymorphics(compiler, type_table, catalog, def->value))){
                return FAILURE;
            }
        }
        break;
    default: break;
        // Ignore this statement, it doesn't contain any expressions that we need to worry about
    }
    
    return SUCCESS;
}
