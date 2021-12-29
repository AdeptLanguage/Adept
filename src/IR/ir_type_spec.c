
#include "IR/ir_type_spec.h"

#include <stdbool.h>

successful_t ir_type_get_spec(ir_type_t *type, ir_type_spec_t *out_spec){
    out_spec->bytes = 0;
    out_spec->traits = IR_TYPE_TRAIT_NONE;
    
    switch(type->kind){
    case TYPE_KIND_POINTER:
        out_spec->traits |= IR_TYPE_TRAIT_POINTER;

        // DANGEROUS: Assuming consistently sized pointers
        out_spec->bytes = sizeof(void*);
        return true;
    case TYPE_KIND_S8:
        out_spec->traits |= IR_TYPE_TRAIT_SIGNED;
        out_spec->bytes = 1;
        return true;
    case TYPE_KIND_S16:
        out_spec->traits |= IR_TYPE_TRAIT_SIGNED;
        out_spec->bytes = 2;
        return true;
    case TYPE_KIND_S32:
        out_spec->traits |= IR_TYPE_TRAIT_SIGNED;
        out_spec->bytes = 4;
        return true;
    case TYPE_KIND_S64:
        out_spec->traits |= IR_TYPE_TRAIT_SIGNED;
        out_spec->bytes = 8;
        return true;
    case TYPE_KIND_U8:
        out_spec->bytes = 1;
        return true;
    case TYPE_KIND_U16:
        out_spec->bytes = 2;
        return true;
    case TYPE_KIND_U32:
        out_spec->bytes = 4;
        return true;
    case TYPE_KIND_U64:
        out_spec->bytes = 8;
        return true;
    case TYPE_KIND_FLOAT:
        out_spec->traits |= IR_TYPE_TRAIT_FLOAT;
        out_spec->bytes = 4;
        return true;
    case TYPE_KIND_DOUBLE:
        out_spec->traits |= IR_TYPE_TRAIT_FLOAT;
        out_spec->bytes = 8;
        return true;
    case TYPE_KIND_BOOLEAN:
        out_spec->bytes = 1;
        return true;
    }

    return false;
}
