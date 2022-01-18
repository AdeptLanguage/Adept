
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

strong_cstr_t ir_type_str(ir_type_t *type){
    // NOTE: Returns allocated string that represents an IR type

    switch(type->kind){
    case TYPE_KIND_NONE:
        return strclone("none_t");
    case TYPE_KIND_POINTER: {
            strong_cstr_t inside = ir_type_str((ir_type_t*) type->extra);
            strong_cstr_t result = mallocandsprintf("*%s", inside);
            free(inside);
            return result;
        }
    case TYPE_KIND_S8:
        return strclone("s8");
    case TYPE_KIND_U8:
        return strclone("u8");
    case TYPE_KIND_S16:
        return strclone("s16");
    case TYPE_KIND_U16:
        return strclone("u16");
    case TYPE_KIND_S32:
        return strclone("s32");
    case TYPE_KIND_U32:
        return strclone("u32");
    case TYPE_KIND_S64:
        return strclone("s64");
    case TYPE_KIND_U64:
        return strclone("u64");
    case TYPE_KIND_HALF:
        return strclone("h");
    case TYPE_KIND_FLOAT:
        return strclone("f");
    case TYPE_KIND_DOUBLE:
        return strclone("d");
    case TYPE_KIND_BOOLEAN:
        return strclone("bool");
    case TYPE_KIND_VOID:
        return strclone("void");
    case TYPE_KIND_FUNCPTR:
        return strclone("funcptr_t");
    case TYPE_KIND_FIXED_ARRAY: {
            ir_type_extra_fixed_array_t *fixed = (ir_type_extra_fixed_array_t*) type->extra;
            strong_cstr_t inside = ir_type_str(fixed->subtype);
            strong_cstr_t result = mallocandsprintf("[%d] %s", (int) fixed->length, inside);
            free(inside);
            return result;
        }
    case TYPE_KIND_STRUCTURE:
        return strclone("struct_t");
    case TYPE_KIND_UNION:
        return strclone("union_t");
    default:
        return strclone("unknown_t");
    }
}

bool ir_types_identical(ir_type_t *a, ir_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must match element to element to be considered identical
    // [pointer] [pointer] [u8]    [pointer] [pointer] [s32]  -> false
    // [pointer] [u8]              [pointer] [u8]             -> true

    if(a->kind != b->kind) return false;

    switch(a->kind){
    case TYPE_KIND_POINTER:
        return ir_types_identical((ir_type_t*) a->extra, (ir_type_t*) b->extra);
    case TYPE_KIND_STRUCTURE: case TYPE_KIND_UNION:
        if(((ir_type_extra_composite_t*) a->extra)->subtypes_length != ((ir_type_extra_composite_t*) b->extra)->subtypes_length) return false;
        for(length_t i = 0; i != ((ir_type_extra_composite_t*) a->extra)->subtypes_length; i++){
            if(!ir_types_identical(((ir_type_extra_composite_t*) a->extra)->subtypes[i], ((ir_type_extra_composite_t*) b->extra)->subtypes[i])) return false;
        }
        return true;
    }

    return true;
}

ir_type_t* ir_type_pointer_to(ir_pool_t *pool, ir_type_t *base){
    ir_type_t *ptr_type = ir_pool_alloc(pool, sizeof(ir_type_t));
    ptr_type->kind = TYPE_KIND_POINTER;
    ptr_type->extra = base;
    return ptr_type;
}

ir_type_t* ir_type_fixed_array_of(ir_pool_t *pool, length_t length, ir_type_t *base){
    ir_type_extra_fixed_array_t *extra = ir_pool_alloc(pool, sizeof(ir_type_extra_fixed_array_t));
    extra->length = length;
    extra->subtype = base;

    ir_type_t *fixed_array_type = ir_pool_alloc(pool, sizeof(ir_type_t));
    fixed_array_type->kind = TYPE_KIND_FIXED_ARRAY;
    fixed_array_type->extra = extra;
    return fixed_array_type;
}

ir_type_t* ir_type_dereference(ir_type_t *type){
    if(type->kind != TYPE_KIND_POINTER) return NULL;
    return (ir_type_t*) type->extra;
}

bool ir_type_is_pointer_to(ir_type_t *type, unsigned int child_type_kind){
    if(type->kind != TYPE_KIND_POINTER) return false;
    if(((ir_type_t*) type->extra)->kind != child_type_kind) return false;
    return true;
}

// (For 64 bit systems)
unsigned int global_type_kind_sizes_in_bits_64[] = {
     0, // TYPE_KIND_NONE
    64, // TYPE_KIND_POINTER
     8, // TYPE_KIND_S8
    16, // TYPE_KIND_S16
    32, // TYPE_KIND_S32
    64, // TYPE_KIND_S64
     8, // TYPE_KIND_U8
    16, // TYPE_KIND_U16
    32, // TYPE_KIND_U32
    64, // TYPE_KIND_U64
    16, // TYPE_KIND_HALF
    32, // TYPE_KIND_FLOAT
    64, // TYPE_KIND_DOUBLE
     1, // TYPE_KIND_BOOLEAN
     0, // TYPE_KIND_UNION
     0, // TYPE_KIND_STRUCTURE
     0, // TYPE_KIND_VOID
    64, // TYPE_KIND_FUNCPTR
     0, // TYPE_KIND_FIXED_ARRAY
};

bool global_type_kind_signs[] = { // (0 == unsigned, 1 == signed)
    0, // TYPE_KIND_NONE
    0, // TYPE_KIND_POINTER
    1, // TYPE_KIND_S8
    1, // TYPE_KIND_S16
    1, // TYPE_KIND_S32
    1, // TYPE_KIND_S64
    0, // TYPE_KIND_U8
    0, // TYPE_KIND_U16
    0, // TYPE_KIND_U32
    0, // TYPE_KIND_U64
    1, // TYPE_KIND_HALF
    1, // TYPE_KIND_FLOAT
    1, // TYPE_KIND_DOUBLE
    0, // TYPE_KIND_BOOLEAN
    0, // TYPE_KIND_UNION
    0, // TYPE_KIND_STRUCTURE
    0, // TYPE_KIND_VOID
    0, // TYPE_KIND_FUNCPTR
    0, // TYPE_KIND_FIXED_ARRAY
};

bool global_type_kind_is_integer[] = { // (0 == non-integer, 1 == integer)
    0, // TYPE_KIND_NONE
    0, // TYPE_KIND_POINTER
    1, // TYPE_KIND_S8
    1, // TYPE_KIND_S16
    1, // TYPE_KIND_S32
    1, // TYPE_KIND_S64
    1, // TYPE_KIND_U8
    1, // TYPE_KIND_U16
    1, // TYPE_KIND_U32
    1, // TYPE_KIND_U64
    0, // TYPE_KIND_HALF
    0, // TYPE_KIND_FLOAT
    0, // TYPE_KIND_DOUBLE
    0, // TYPE_KIND_BOOLEAN
    0, // TYPE_KIND_UNION
    0, // TYPE_KIND_STRUCTURE
    0, // TYPE_KIND_VOID
    0, // TYPE_KIND_FUNCPTR
    0, // TYPE_KIND_FIXED_ARRAY
};

bool global_type_kind_is_float[] = { // (0 == non-float, 1 == float)
    0, // TYPE_KIND_NONE
    0, // TYPE_KIND_POINTER
    0, // TYPE_KIND_S8
    0, // TYPE_KIND_S16
    0, // TYPE_KIND_S32
    0, // TYPE_KIND_S64
    0, // TYPE_KIND_U8
    0, // TYPE_KIND_U16
    0, // TYPE_KIND_U32
    0, // TYPE_KIND_U64
    1, // TYPE_KIND_HALF
    1, // TYPE_KIND_FLOAT
    1, // TYPE_KIND_DOUBLE
    0, // TYPE_KIND_BOOLEAN
    0, // TYPE_KIND_UNION
    0, // TYPE_KIND_STRUCTURE
    0, // TYPE_KIND_VOID
    0, // TYPE_KIND_FUNCPTR
    0, // TYPE_KIND_FIXED_ARRAY
};
