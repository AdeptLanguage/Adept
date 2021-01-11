
#ifndef _ISAAC_IR_GEN_RTTI_H
#define _ISAAC_IR_GEN_RTTI_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_gen_rtti.h ==============================
    Module for generating RTTI IR constructs
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IR/ir.h"

typedef struct {
    ir_type_t *any_type_type;
    ir_type_t *any_struct_type_type;
    ir_type_t *any_union_type_type;
    ir_type_t *any_ptr_type_type;
    ir_type_t *any_funcptr_type_type;
    ir_type_t *any_fixed_array_type_type;
    ir_type_t *any_type_ptr_type;
    ir_type_t *ubyte_ptr_type;
    ir_type_t *usize_type;
} ir_rtti_types_t;

errorcode_t ir_gen__types__(compiler_t *compiler, object_t *object, ir_global_t *ir_global);

errorcode_t ir_gen_rtti_fetch_rtti_types(ir_module_t *ir_module, ir_rtti_types_t *out_rtti_types);

errorcode_t ir_gen__types__placeholder(object_t *object, ir_global_t *ir_global);

ir_value_t **ir_gen__types__values(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_RTTI_H
