
#include "IR/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IR/ir_lowering.h"
#include "IR/ir_value_str.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/datatypes.h"

void ir_basicblock_new_instructions(ir_basicblock_t *block, length_t amount){
    // NOTE: Ensures that there is enough room for 'amount' more instructions
    // NOTE: If there isn't, more memory will be allocated so they can be generated
    if(block->instructions.length + amount >= block->instructions.capacity){
        ir_instr_t **new_instructions = malloc(sizeof(ir_instr_t*) * block->instructions.capacity * 2);
        memcpy(new_instructions, block->instructions.instructions, sizeof(ir_instr_t*) * block->instructions.length);
        block->instructions.capacity *= 2;
        free(block->instructions.instructions);
        block->instructions.instructions = new_instructions;
    }
}

void ir_basicblocks_free(ir_basicblocks_t *basicblocks){
    for(length_t i = 0; i != basicblocks->length; i++){
        ir_basicblock_free(&basicblocks->blocks[i]);
    }
    free(basicblocks->blocks);
}

void ir_vtable_init_free(ir_vtable_init_t *vtable_init){
    ast_type_free(&vtable_init->subject_type);
}

void ir_vtable_init_list_free(ir_vtable_init_list_t *vtable_init_list){
    for(length_t i = 0; i != vtable_init_list->length; i++){
        ir_vtable_init_free(&vtable_init_list->initializations[i]);
    }
    free(vtable_init_list->initializations);
}

void ir_static_variables_free(ir_static_variables_t *static_variables){
    free(static_variables->variables);
}

void rtti_relocations_free(rtti_relocations_t *relocations){
    for(length_t i = 0; i != relocations->length; i++){
        free(relocations->relocations[i].human_notation);
    }
    free(relocations->relocations);
}

void free_list_free(free_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        free(list->pointers[i]);
    }
    free(list->pointers);
}

void ir_funcs_free(ir_funcs_t ir_funcs_list){
    for(length_t f = 0; f != ir_funcs_list.length; f++){
        ir_func_t *func = &ir_funcs_list.funcs[f];

        ir_basicblocks_free(&func->basicblocks);
        free(func->argument_types);

        if(func->scope != NULL){
            bridge_scope_free(func->scope);
            free(func->scope);
        }
    }
    free(ir_funcs_list.funcs);
}

void ir_basicblock_free(ir_basicblock_t *basicblock){
    free(basicblock->instructions.instructions);
}

static char *ir_implementation_encoding = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static length_t ir_implementation_encoding_base;

void ir_implementation_setup(){
    ir_implementation_encoding_base = strlen(ir_implementation_encoding);
}

void ir_implementation(length_t id, char prefix, char *output_buffer){
    // NOTE: output_buffer is assumed to be able to hold 32 characters
    length_t length = 0;

    if(prefix != 0x00){
        output_buffer[length++] = prefix;
    }
    
    do {
        length_t digit = id % ir_implementation_encoding_base;
        output_buffer[length++] = ir_implementation_encoding[digit];
        id /= ir_implementation_encoding_base;

        if(length == 31){
            printf("INTERNAL ERROR: ir_implementation received too large of an id\n");
            break;
        }
    } while(id != 0);

    output_buffer[length] = 0x00;
}

unsigned long long ir_value_uniqueness_value(ir_pool_t *pool, ir_value_t **value){
    if(ir_lower_const_cast(pool, value) || (*value)->value_type != VALUE_TYPE_LITERAL){
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    };
    
    switch((*value)->type->kind){
    case TYPE_KIND_BOOLEAN: return *((adept_bool*) (*value)->extra);
    case TYPE_KIND_S8:      return *((adept_byte*) (*value)->extra);
    case TYPE_KIND_U8:      return *((adept_ubyte*) (*value)->extra);
    case TYPE_KIND_S16:     return *((adept_short*) (*value)->extra);
    case TYPE_KIND_U16:     return *((adept_ushort*) (*value)->extra);
    case TYPE_KIND_S32:     return *((adept_int*) (*value)->extra);
    case TYPE_KIND_U32:     return *((adept_uint*) (*value)->extra);
    case TYPE_KIND_S64:     return *((adept_long*) (*value)->extra);
    case TYPE_KIND_U64:     return *((adept_ulong*) (*value)->extra);
    default:
        printf("INTERNAL ERROR: ir_value_uniqueness_value received a value that isn't a constant literal\n");
        return 0xFFFFFFFF;
    }
}

void ir_print_value(ir_value_t *value){
    strong_cstr_t s = ir_value_str(value);
    printf("%s\n", s);
    free(s);
}

void ir_print_type(ir_type_t *type){
    strong_cstr_t s = ir_type_str(type);
    printf("%s\n", s);
    free(s);
}

void ir_job_list_free(ir_job_list_t *job_list){
    free(job_list->jobs);
}
