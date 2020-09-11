
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
        if(ir_lower_const_zext(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_SEXT:
        if(ir_lower_const_sext(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FEXT:
        if(ir_lower_const_fext(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_TRUNC:
        if(ir_lower_const_trunc(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FTRUNC:
        if(ir_lower_const_ftrunc(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_INTTOPTR:
        // Operate using 'zext' operation
        if(ir_lower_const_zext(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_PTRTOINT:
        // Operate using 'trunc' operation
        if(ir_lower_const_trunc(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FPTOUI:
        if(ir_lower_const_fptoui(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_FPTOSI:
        if(ir_lower_const_fptosi(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_UITOFP:
        if(ir_lower_const_uitofp(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_SITOFP:
        if(ir_lower_const_sitofp(pool, inout_value)) return FAILURE;
        break;
    case VALUE_TYPE_CONST_REINTERPRET:
        if(ir_lower_const_reinterpret(pool, inout_value)) return FAILURE;
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

    if((to_spec.traits & IR_TYPE_TRAIT_POINTER) ^ (from_spec.traits & IR_TYPE_TRAIT_POINTER)){
        redprintf("INTERNAL ERROR: ir_lower_const_bitcast() called for a single pointer type!\n");
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

errorcode_t ir_lower_const_zext(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_zext() called when target type is smaller!\n");
        return FAILURE;
    }

    unsigned short x = 0xEEFF;
    bool is_little_endian = *((unsigned char*) &x) == 0xFF;
    char *pointer = (*child)->extra;
    
    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);
    memset(new_pointer, 0, to_spec.bytes);
    
    // Copy bytes of value to proper place in new value
    memcpy(is_little_endian ? new_pointer : &new_pointer[to_spec.bytes - from_spec.bytes], pointer, from_spec.bytes);
    
    // Promote the altered child literal value
    (*child)->extra = new_pointer;
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

    unsigned short x = 0xEEFF;
    bool is_little_endian = *((unsigned char*) &x) == 0xFF;
    char *pointer = (*child)->extra;
    char *sign_byte = is_little_endian ? &pointer[from_spec.bytes - 1] : pointer;
    bool sign_bit = *sign_byte & 0x80;
    
    // Turn off sign bit and format to magnitude based unsigned integer
    if(sign_bit) switch(from_spec.bytes){
    case 1:
        *((int8_t*) pointer) *= -1;
        break;
    case 2:
        *((int16_t*) pointer) *= -1;
        break;
    case 4:
        *((int32_t*) pointer) *= -1;
        break;
    case 8:
        *((int64_t*) pointer) *= -1;
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_sext() failed to swap sign bit!\n");
        return FAILURE;
    }

    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);
    memset(new_pointer, 0, to_spec.bytes);
    
    // Copy bytes of value to proper place in new value
    memcpy(is_little_endian ? new_pointer : &new_pointer[to_spec.bytes - from_spec.bytes], pointer, from_spec.bytes);
    
    // Treat as signed integer of same size for platform independent sign swap and representation formatting
    if(sign_bit) switch(to_spec.bytes){
    case 1:
        *((int8_t*) new_pointer) *= -1;
        break;
    case 2:
        *((int16_t*) new_pointer) *= -1;
        break;
    case 4:
        *((int32_t*) new_pointer) *= -1;
        break;
    case 8:
        *((int64_t*) new_pointer) *= -1;
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_sext() failed to swap sign bit!\n");
        return FAILURE;
    }

    // Promote the altered child literal value
    (*child)->extra = new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_fext(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_fext() called when target type is smaller!\n");
        return FAILURE;
    }

    adept_double *new_pointer = ir_pool_alloc(pool, sizeof(adept_double));
    *new_pointer = *((adept_float*) ((*child)->extra));
    
    // Promote the altered child literal value
    (*child)->extra = (void*) new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_ftrunc(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes > from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_ftrunc() called when target type is smaller!\n");
        return FAILURE;
    }

    // Promote the altered child literal value
    *((adept_float*) ((*child)->extra)) = *((adept_double*) ((*child)->extra));
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_fptoui(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_fptoui() called when target type is smaller!\n");
        return FAILURE;
    }

    unsigned short x = 0xEEFF;
    bool is_little_endian = *((unsigned char*) &x) == 0xFF;
    
    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);
    memset(new_pointer, 0, to_spec.bytes);

    uint64_t as_unsigned;
    char *pointer = (char*) &as_unsigned;

    switch(from_spec.bytes){
    case 4:
        as_unsigned = *((adept_float*) ((*child)->extra));
        break;
    case 8:
        as_unsigned = *((adept_double*) ((*child)->extra));
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_fptoui() failed!\n");
        return FAILURE;
    }
    
    // Copy bytes of value to proper place in new value
    memcpy(new_pointer, is_little_endian ? pointer : &pointer[sizeof(uint64_t) - to_spec.bytes], to_spec.bytes);
    
    // Promote the altered child literal value
    (*child)->extra = new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_fptosi(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_fptosi() called when target type is smaller!\n");
        return FAILURE;
    }

    unsigned short x = 0xEEFF;
    bool is_little_endian = *((unsigned char*) &x) == 0xFF;
    
    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);
    memset(new_pointer, 0, to_spec.bytes);

    int64_t as_signed;
    char *pointer = (char*) &as_signed;

    switch(from_spec.bytes){
    case 4:
        as_signed = *((adept_float*) ((*child)->extra));
        break;
    case 8:
        as_signed = *((adept_double*) ((*child)->extra));
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_fptosi() failed!\n");
        return FAILURE;
    }

    bool is_signed = as_signed < 0;
    if(is_signed) as_signed *= -1;
    
    // Copy bytes of value to proper place in new value
    memcpy(new_pointer, is_little_endian ? pointer : &pointer[sizeof(uint64_t) - to_spec.bytes], to_spec.bytes);

    // Treat as signed integer of same size for platform independent sign swap and representation formatting
    if(is_signed) switch(to_spec.bytes){
    case 1:
        *((int8_t*) new_pointer) *= -1;
        break;
    case 2:
        *((int16_t*) new_pointer) *= -1;
        break;
    case 4:
        *((int32_t*) new_pointer) *= -1;
        break;
    case 8:
        *((int64_t*) new_pointer) *= -1;
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_fptosi() failed to swap sign bit!\n");
        return FAILURE;
    }
    
    // Promote the altered child literal value
    (*child)->extra = new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_uitofp(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_uitofp() called when target type is smaller!\n");
        return FAILURE;
    }

    double as_float;
    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);

    switch(from_spec.bytes){
    case 1:
        as_float = *((uint8_t*) ((*child)->extra));
        break;
    case 2:
        as_float = *((uint16_t*) ((*child)->extra));
        break;
    case 4:
        as_float = *((uint32_t*) ((*child)->extra));
        break;
    case 8:
        as_float = *((uint64_t*) ((*child)->extra));
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_uitofp() failed!\n");
        return FAILURE;
    }
    
    // Convert floating point number to requested size
    if(to_spec.bytes == 4){
        *((adept_float*) new_pointer) = as_float;
    } else {
        *((adept_double*) new_pointer) = as_float;
    }

    // Promote the altered child literal value
    (*child)->extra = new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_sitofp(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes < from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_sitofp() called when target type is smaller!\n");
        return FAILURE;
    }
    
    double as_float;
    char *new_pointer = ir_pool_alloc(pool, to_spec.bytes);

    switch(from_spec.bytes){
    case 1:
        as_float = *((int8_t*) ((*child)->extra));
        break;
    case 2:
        as_float = *((int16_t*) ((*child)->extra));
        break;
    case 4:
        as_float = *((int32_t*) ((*child)->extra));
        break;
    case 8:
        as_float = *((int64_t*) ((*child)->extra));
        break;
    default:
        redprintf("INTERNAL ERROR: ir_lower_const_sitofp() failed!\n");
        return FAILURE;
    }
    
    // Convert floating point number to requested size
    if(to_spec.bytes == 4){
        *((adept_float*) new_pointer) = as_float;
    } else {
        *((adept_double*) new_pointer) = as_float;
    }

    // Promote the altered child literal value
    (*child)->extra = new_pointer;
    *inout_value = *child;

    // Change the type of the literal value
    (*inout_value)->type = type;
    return SUCCESS;
}

errorcode_t ir_lower_const_reinterpret(ir_pool_t *pool, ir_value_t **inout_value){
    // NOTE: Assumes that '!VALUE_TYPE_IS_CONSTANT_CAST((*inout_value)->value_type)' is true
    //       In other words, that the value inside the given value is not another constant cast

    ir_type_t *type = (*inout_value)->type;
    ir_value_t **child = (ir_value_t**) &((*inout_value)->extra);

    ir_type_spec_t to_spec, from_spec;
    if(!ir_type_get_spec(type, &to_spec) || !ir_type_get_spec((*child)->type, &from_spec)) return false;

    if(to_spec.bytes != from_spec.bytes){
        redprintf("INTERNAL ERROR: ir_lower_const_reinterpret() called for types of different sizes!\n");
        return FAILURE;
    }

    *inout_value = *child;
    (*inout_value)->type = type;
    return SUCCESS;
}
