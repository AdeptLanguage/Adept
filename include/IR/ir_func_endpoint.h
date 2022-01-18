
#ifndef _ISAAC_IR_FUNC_ENDPOINT_H
#define _ISAAC_IR_FUNC_ENDPOINT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================ ir_func_endpoint.h ============================
    Module for function endpoints
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "UTIL/list.h"

// ---------------- ir_func_endpoint_t ----------------
// Endpoint of a function/method mapping
typedef struct {
    funcid_t ast_func_id;
    funcid_t ir_func_id;
} ir_func_endpoint_t;

// ---------------- ir_func_endpoint_list_t ----------------
// A list of function/method endpoints
typedef listof(ir_func_endpoint_t, endpoints) ir_func_endpoint_list_t;

// ---------------- ir_func_endpoint_list_free ----------------
// Frees a list of function/method endpoints
#define ir_func_endpoint_list_free(LIST) free((LIST)->endpoints)

// ---------------- ir_func_endpoint_list_insert ----------------
// Inserts a function/method endpoint into a sorted list
void ir_func_endpoint_list_insert(ir_func_endpoint_list_t *endpoint_list, ir_func_endpoint_t endpoint);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_FUNC_ENDPOINT_H
