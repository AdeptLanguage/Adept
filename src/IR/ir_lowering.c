
#include "UTIL/color.h"
#include "IR/ir_lowering.h"
#include "IR/ir_type_spec.h"

errorcode_t ir_lower_const_cast(ir_pool_t *pool, ir_value_t **inout_value){    
    unsigned int value_type = (*inout_value)->value_type;
    if(!VALUE_TYPE_IS_CONSTANT_CAST(value_type)) return SUCCESS;

    // Lower any descandant casts
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);
    if(VALUE_TYPE_IS_CONSTANT_CAST((*child)->value_type)) ir_lower_const_cast(pool, child);

    switch(value_type){
    case VALUE_TYPE_CONST_BITCAST:
        if(ir_lower_const_bitcast(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_ZEXT:
        break;
    case VALUE_TYPE_CONST_SEXT:
        break;
    case VALUE_TYPE_CONST_FEXT:
        break;
    case VALUE_TYPE_CONST_TRUNC:
        break;
    case VALUE_TYPE_CONST_FTRUNC:
        break;
    case VALUE_TYPE_CONST_INTTOPTR:
        break;
    case VALUE_TYPE_CONST_PTRTOINT:
        break;
    case VALUE_TYPE_CONST_FPTOUI:
        break;
    case VALUE_TYPE_CONST_FPTOSI:
        break;
    case VALUE_TYPE_CONST_UITOFP:
        break;
    case VALUE_TYPE_CONST_SITOFP:
        break;
    case VALUE_TYPE_CONST_REINTERPRET:
        break;
    }

    return SUCCESS;
}

errorcode_t ir_lower_const_bitcast(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value given is not a constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bits != from_spec.bits){
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
