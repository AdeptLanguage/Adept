
#ifndef _ISAAC_UTIL_H
#define _ISAAC_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================== util.h ==================================
    Module for IR lowering related utilities
    ----------------------------------------------------------------------------
*/

#include "IR/ir.h"

errorcode_t ir_lower_const_cast(ir_pool_t *pool, ir_value_t **inout_value);
errorcode_t ir_lower_const_bitcast(ir_pool_t *pool, ir_value_t **inout_value);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_UTIL_H
