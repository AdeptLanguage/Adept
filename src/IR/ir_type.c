
#include "IR/ir_pool.h"
#include "IR/ir_type.h"

ir_type_t* ir_type_pointer_to(ir_pool_t *pool, ir_type_t *base){
    ir_type_t *ptr_type = ir_pool_alloc(pool, sizeof(ir_type_t));
    ptr_type->_kind = TYPE_KIND_POINTER;
    ptr_type->_extra = base;
    return ptr_type;
}

// (For 64 bit systems)
unsigned int global_type_kind_sizes_64[] = {
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
