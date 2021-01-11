
#ifndef _ISAAC_IR_LOWERING_H
#define _ISAAC_IR_LOWERING_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_lowering.h ==============================
    Module for IR lowering related utilities
    ---------------------------------------------------------------------------
*/

#include "IR/ir_pool.h"
#include "IR/ir_value.h"

errorcode_t ir_lower_const_cast(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_bitcast(ir_value_t **inout_value);
errorcode_t ir_lower_const_zext(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_sext(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_fext(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_trunc(ir_value_t **inout_value);
errorcode_t ir_lower_const_ftrunc(ir_value_t **inout_value);
errorcode_t ir_lower_const_fptoui(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_fptosi(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_uitofp(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_sitofp(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_reinterpret(ir_value_t **inout_value);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_LOWERING_H
