
#include "UTIL/color.h"
#include "IR/ir_lowering.h"
#include "IR/ir_type_spec.h"

errorcode_t ir_lower_const_cast(ir_pool_t *pool, ir_value_t **inout_value){
    unsigned int value_type = (*inout_value)->value_type;
    if(!VALUE_TYPE_IS_CONSTANT_CAST(value_type)) return SUCCESS;
    
    // Lower any descandant casts
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);
    if(VALUE_TYPE_IS_CONSTANT_CAST((*child)->value_type) && ir_lower_const_cast(pool, child)) return FAILURE;

    switch(value_type){
    case VALUE_TYPE_CONST_BITCAST:
        if(ir_lower_const_bitcast(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_ZEXT:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_ZEXT is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_SEXT:
        if(ir_lower_const_sext(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FEXT:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_FEXT is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_TRUNC:
        if(ir_lower_const_trunc(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FTRUNC:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_FTRUNC is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_INTTOPTR:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_INTTOPTR is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_PTRTOINT:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_PTRTOINT is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_FPTOUI:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_FPTOUI is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_FPTOSI:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_FPTOSI is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_UITOFP:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_UITOFP is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_SITOFP:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_SITOFP is unimplemented!\n");
        return FAILURE;
        break;
    case VALUE_TYPE_CONST_REINTERPRET:
        redprintf("ir_lower_const_cast/VALUE_TYPE_CONST_REINTERPRET is unimplemented!\n");
        return FAILURE;
        break;
    }

    return SUCCESS;
}

errorcode_t ir_lower_const_bitcast(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes != from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_bitcast() called for types of different sizes!\n");
        return FAILURE;
    }

    if(to_spec.traits & IR_TYPE_TRAIT_POINTER || from_spec.traits & IR_TYPE_TRAIT_POINTER){
        redprintf("INTERNAL ERROR: ir_lower_const_bitcast() called for pointer type!\n");
        return FAILURE;
    }

    // Since size is that same, we'll just treat the bits differently by changing the literal type
    // We'll just replace the cast with the value being casted and change the literal's type to match the cast type
    // NOTE: This is ok since 'ir_value_t->type' lives in an 'ir_pool_t'
    *inout_value = *child;
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_trunc(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes >= from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_trunc() called when target type is equal size of larger!\n");
        return FAILURE;
    }

    unsigned short x = 0xEEFF;
    bool is_little_endian = *((unsigned char*) &x) == 0xFF;
    char *pointer = (*child)->extra;

    if(!is_little_endian){
        // Only have to move bytes if big endian
        memmove(pointer, &pointer[from_spec.bytes - to_spec.bytes], to_spec.bytes);
    }

    // Promote the altered child literal value
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_sext(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast


    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_sext() called when target type is smaller!\n");
        return FAILURE;
    }

    redprintf("INTERNAL ERROR: ir_lower_const_sext() is unimplemented!\n");
    return FAILURE;
}
