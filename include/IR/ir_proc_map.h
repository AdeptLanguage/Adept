
#ifndef _ISAAC_IR_PROC_MAP_H
#define _ISAAC_IR_PROC_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_proc_map.h ==============================
    Module for mapping generic keys to procedures
    ---------------------------------------------------------------------------
*/

#include "IR/ir_pool.h"
#include "IR/ir_func_endpoint.h"
#include "UTIL/ground.h"

// ---------------- ir_proc_map_t ----------------
// IR procedure map, used to map a value of a generic 'key'
// type to a list of possible function endpoints
typedef struct {
    void *keys;
    ir_func_endpoint_list_t **endpoint_lists;
    length_t length;
    length_t capacity;

    // Implementation details
    ir_pool_t endpoint_pool;
} ir_proc_map_t;

// ---------------- ir_proc_map_init ----------------
// Initializes a procedure map
void ir_proc_map_init(ir_proc_map_t *map, length_t sizeof_key, length_t estimated_keys);

// ---------------- ir_proc_map_free ----------------
// Frees a procedure map
// Cannot and does not free any memory referenced by contained key values
void ir_proc_map_free(ir_proc_map_t *map);

// ---------------- ir_proc_map_insert ----------------
// Inserts an endpoint into the endpoint list for a given key
// If the given key doesn't already exist in the map, it will be created
void ir_proc_map_insert(ir_proc_map_t *map, const void *key, length_t sizeof_key, ir_func_endpoint_t endpoint, int (*key_compare)(const void*, const void*));

// ---------------- ir_proc_map_find ----------------
// Looks up a key inside of the map and returns a stable pointer
// to its corresponding endpoint list. Returns NULL if the supplied
// key doesn't exist in the map
// NOTE: Guaranteed to return a stable pointer (the pointer will be valid until 'map' is freed)
ir_func_endpoint_list_t *ir_proc_map_find(ir_proc_map_t *map, const void *key, length_t sizeof_key, int (*key_compare)(const void*, const void*));

// ---------------- ir_func_key_t ----------------
typedef struct {
    weak_cstr_t name;
} ir_func_key_t;

// ---------------- ir_method_key_t ----------------
typedef struct {
    // (Methods are grouped ignoring polymorphic parameters)
    weak_cstr_t struct_name;
    weak_cstr_t method_name;
} ir_method_key_t;

// ---------------- compare_ir_func_key ----------------
// Compare function for ir_func_key_t
int compare_ir_func_key(const void*, const void*);

// ---------------- compare_ir_method_key ----------------
// Compare function for ir_method_key_t
int compare_ir_method_key(const void*, const void*);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_PROC_MAP_H
