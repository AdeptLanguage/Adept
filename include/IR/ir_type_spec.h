
#ifndef _ISAAC_IR_TYPE_TRAITS_H
#define _ISAAC_IR_TYPE_TRAITS_H

/*
    ============================== ir_type_spec.h ==============================
    Module for IR type specification info
    ----------------------------------------------------------------------------
*/

#include "IR/ir_type.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

#define IR_TYPE_TRAIT_NONE      TRAIT_NONE
#define IR_TYPE_TRAIT_POINTER   TRAIT_1
#define IR_TYPE_TRAIT_SIGNED    TRAIT_2
#define IR_TYPE_TRAIT_FLOAT     TRAIT_3
typedef trait_t ir_type_traits_t;

typedef struct {
    length_t bytes;
    ir_type_traits_t traits;
} ir_type_spec_t;

successful_t ir_type_get_spec(ir_type_t *type, ir_type_spec_t *out_spec);

#endif // _ISAAC_IR_TYPE_TRAITS_H
