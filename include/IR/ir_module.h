
#ifndef _ISAAC_IR_MODULE_H
#define _ISAAC_IR_MODULE_H

/*
    =============================== ir_module.h ===============================
    Module for intermediate representation modules
    -----------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "IR/ir_proc_map.h"
#include "IR/ir_type_map.h"
#include "UTIL/ground.h"

// ---------------- ir_module_t ----------------
// An intermediate representation module
typedef struct ir_module {
    ir_shared_common_t common;
    ir_pool_t pool;
    ir_type_map_t type_map;
    ir_funcs_t funcs;
    ir_proc_map_t func_map;
    ir_proc_map_t method_map;
    ir_global_t *globals;
    length_t globals_length;
    ir_anon_globals_t anon_globals;
    ir_gen_sf_cache_t sf_cache;
    rtti_relocations_t rtti_relocations;
    struct ir_builder *init_builder;
    struct ir_builder *deinit_builder;
    ir_static_variables_t static_variables;
    ir_job_list_t job_list;
    free_list_t defer_free;
    ir_vtable_init_list_t vtable_init_list;
    ir_vtable_dispatch_list_t vtable_dispatch_list;
} ir_module_t;

// ---------------- ir_module_free ----------------
// Initializes an IR module for use
void ir_module_init(ir_module_t *ir_module, length_t funcs_length, length_t globals_length, length_t number_of_function_names_guess);

// ---------------- ir_module_free ----------------
// Frees data within an IR module
void ir_module_free(ir_module_t *ir_module);

// ---------------- ir_module_create_func_mapping ----------------
// Creates a new function mapping
void ir_module_create_func_mapping(ir_module_t *module, weak_cstr_t function_name, ir_func_endpoint_t endpoint, bool add_to_job_list);

// ---------------- ir_module_create_method_mapping ----------------
// Create a new method mapping
void ir_module_create_method_mapping(ir_module_t *module, weak_cstr_t struct_name, weak_cstr_t method_name, ir_func_endpoint_t endpoint);

// ---------------- ir_module_defer_free ----------------
// Schedules a heap allocation to be deallocated
// when an IR module is destroyed.
// Can be used to preserve data that an IR module weakly references
void ir_module_defer_free(ir_module_t *module, void *pointer);

#endif // _ISAAC_IR_MODULE_H
