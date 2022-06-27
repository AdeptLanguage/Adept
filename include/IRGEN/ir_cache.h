
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
#include "UTIL/func_pair.h"
#include "UTIL/ground.h"

#define IR_GEN_SF_CACHE_SUGGESTED_NUM_BUCKETS 1024

// ---------------- ir_gen_sf_cache_entry_t ----------------
// Special Functions cache entry.
// The structural layout is squished together tightly, since we
// know that there will be a huge array of them
#define ir_gen_sf_cache_entry_is_occupied(a) ((a)->ast_type.elements_length != 0)

typedef struct ir_gen_sf_cache_entry {
    ast_type_t ast_type;

    troolean has_pass : 2,
             has_defer : 2,
             has_assign : 2;
    
    func_pair_t pass;   // __pass__
    func_pair_t defer;  // __defer__
    func_pair_t assign; // __assign__
    
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
void ir_gen_sf_cache_init(ir_gen_sf_cache_t *cache, length_t num_buckets);

// ---------------- ir_gen_sf_cache_free ----------------
// Frees special functions cache
void ir_gen_sf_cache_free(ir_gen_sf_cache_t *cache);

// ---------------- ir_gen_sf_cache_read ----------------
// Reads the values (has_* and *) of inside an ir_gen_sf_cache_t to populate an optional_func_pair_t.
// Usage: `if(ir_gen_sf_cache_read(cache->has_pass, cache->pass, result) == SUCCESS) return SUCCESS;`
errorcode_t ir_gen_sf_cache_read(troolean has, func_pair_t maybe_pair, optional_func_pair_t *result);

// ---------------- ir_gen_sf_cache_locate_or_insert ----------------
// Locates a cache entry for AST type in special functions cache
// If one doesn't exist yet, one will be created
// Will never return NULL
// NOTE: Does not take any ownership of 'type'
ir_gen_sf_cache_entry_t *ir_gen_sf_cache_locate_or_insert(ir_gen_sf_cache_t *cache, ast_type_t *type);

// ---------------- ir_gen_sf_cache_dump ----------------
// Dumps a visual representation of an special function cache
void ir_gen_sf_cache_dump(FILE *file, ir_gen_sf_cache_t *sf_cache);

#endif // _ISAAC_IR_GEN_CACHE_H
