
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
    func_id_t ast_func_id;
    func_id_t ir_func_id;
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

// ---------------- ir_func_endpoint_list_insert_at ----------------
// Inserts a function/method endpoint at a position without regard to sorting
void ir_func_endpoint_list_insert_at(ir_func_endpoint_list_t *endpoint_list, ir_func_endpoint_t endpoint, length_t index);

// ---------------- ir_func_endpoint_list_append ----------------
// Appends a function/method endpoint without regard to sorting
#define ir_func_endpoint_list_append(LIST, VALUE) list_append((LIST), (VALUE), ir_func_endpoint_t)

// ---------------- ir_func_endpoint_list_append_list ----------------
// Appends a function/method endpoint list without regard to sorting
void ir_func_endpoint_list_append_list(ir_func_endpoint_list_t *endpoint_t, ir_func_endpoint_list_t *other);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_FUNC_ENDPOINT_H
