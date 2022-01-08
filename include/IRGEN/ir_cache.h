
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
// know that there will be a huge array of them
#define ir_gen_sf_cache_entry_is_occupied(a) ((a)->ast_type.elements_length != 0)

typedef struct ir_gen_sf_cache_entry {
    ast_type_t ast_type;

    troolean has_pass : 2,
             has_defer : 2,
             has_assign : 2;

    // NOTE: Assume function IDs are less than 2^32-1
    unsigned int pass_ast_func_id,   // __pass__
                 pass_ir_func_id,
                 defer_ast_func_id,  // __defer__
                 defer_ir_func_id,
                 assign_ast_func_id, // __assign__
                 assign_ir_func_id;

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

// ---------------- ir_gen_sf_cache_locate_or_insert ----------------
// Locates a cache entry for AST type in special functions cache
// If one doesn't exist yet, one will be created
// Will always return a valid pointer
// NOTE: Does not take any ownership of 'type'
ir_gen_sf_cache_entry_t *ir_gen_sf_cache_locate_or_insert(ir_gen_sf_cache_t *cache, ast_type_t *type);

// ---------------- ir_gen_sf_cache_dump ----------------
// Dumps a visual representation of an special function cache
void ir_gen_sf_cache_dump(FILE *file, ir_gen_sf_cache_t *sf_cache);

#endif // _ISAAC_IR_GEN_CACHE_H
