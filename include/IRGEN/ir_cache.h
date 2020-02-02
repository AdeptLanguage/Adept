
#ifndef IR_GEN_CACHE_H
#define IR_GEN_CACHE_H

/*
    ============================== ir_gen_find.h ==============================
    Module for locating intermediate representation data structures
    that reside in organized lists
    ---------------------------------------------------------------------------
*/

#include "AST/ast.h"
#include "UTIL/ground.h"

#define IR_GEN_SF_CACHE_SIZE 1024

// ---------------- ir_gen_sf_cache_entry_t ----------------
// Special functions cache entry
typedef struct ir_gen_sf_cache_entry {
    bool occupied;
    ast_type_t ast_type;

    // __pass__
    troolean has_pass_func;
    length_t pass_ir_func_id;
    length_t pass_ast_func_id;

    // __defer__
    troolean has_defer_func;
    length_t defer_ir_func_id;
    length_t defer_ast_func_id;

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

#endif // IR_GEN_CACHE_H
