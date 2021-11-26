
#ifndef _ISAAC_IR_GEN_CACHE_H
#define _ISAAC_IR_GEN_CACHE_H

/*
    =============================== ir_cache.h ===============================
    Module for efficiently looking up information about AST types
    during IR generation

    This module primarily deals with the Special Function cache, which is
    used to quickly lookup commonly accessed special purpose functions such as:
    - __pass__
    - __defer__
    - __assign__
    --------------------------------------------------------------------------
*/

#include "AST/ast.h"
#include "UTIL/ground.h"

#define IR_GEN_SF_CACHE_SIZE 1024

// ---------------- ir_gen_sf_cache_entry_t ----------------
// Special Functions cache entry.
// The structural layout is squished together tightly, since we
// know that there will be a large array of them
#define ir_gen_sf_cache_entry_is_occupied(a) ((a)->ast_type.elements_length != 0)

typedef struct ir_gen_sf_cache_entry {
    ast_type_t ast_type;

    troolean has_pass : 2,
             has_defer : 2,
             has_assign : 2;

    length_t pass_ir_func_id;   // __pass__
    length_t defer_ir_func_id;  // __defer__
    length_t assign_ir_func_id; // __assign__

    struct ir_gen_sf_cache_entry *next;
} ir_gen_sf_cache_entry_t;

// ---------------- ir_gen_sf_cache_t ----------------
// Special functions cache
typedef struct {
    ir_gen_sf_cache_entry_t *storage;
    length_t capacity;
} ir_gen_sf_cache_t;

// ---------------- ir_gen_sf_cache_init ----------------
// Initializes special functions cache
void ir_gen_sf_cache_init(ir_gen_sf_cache_t *cache);

// ---------------- ir_gen_sf_cache_free ----------------
// Frees special functions cache
void ir_gen_sf_cache_free(ir_gen_sf_cache_t *cache);

// ---------------- ir_gen_sf_cache_locate ----------------
// Locates cache entry for AST type in special functions cache
ir_gen_sf_cache_entry_t *ir_gen_sf_cache_locate(ir_gen_sf_cache_t *cache, ast_type_t type);

// ---------------- ir_gen_sf_cache_dump ----------------
// Dumps a visual representation of an special function cache
void ir_gen_sf_cache_dump(FILE *file, ir_gen_sf_cache_t *sf_cache);

#endif // _ISAAC_IR_GEN_CACHE_H
