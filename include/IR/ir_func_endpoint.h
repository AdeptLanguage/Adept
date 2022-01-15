
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

typedef struct {
    funcid_t ir_func_id;
    funcid_t ast_func_id;
} ir_func_endpoint_t;

typedef listof(ir_func_endpoint_t, endpoints) ir_func_endpoint_list_t;
#define ir_func_endpoint_list_append(LIST, VALUE) list_append((LIST), (VALUE), ir_func_endpoint_t)
#define ir_func_endpoint_list_free(LIST) free((LIST)->endpoints)

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_FUNC_ENDPOINT_H
