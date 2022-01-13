
#ifndef _ISAAC_IR_VALUE_STR_H
#define _ISAAC_IR_VALUE_STR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_value_str.h ==============================
    Module for creating human-readable string representations of IR values
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "IR/ir_value.h"

// ---------------- ir_value_str ----------------
// Stringifies an IR value
strong_cstr_t ir_value_str(ir_value_t *value);

// ---------------- ir_values_str ----------------
// Stringifies a list of IR values, separating them with commas
strong_cstr_t ir_values_str(ir_value_t **values, length_t length);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_VALUE_STR_H
