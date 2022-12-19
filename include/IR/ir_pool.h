
#ifndef _ISAAC_IR_POOL_H
#define _ISAAC_IR_POOL_H

#include "UTIL/ground.h"
#include "UTIL/list.h"

#define POOL_ALLOCATION_ALIGNMENT sizeof(void*)

// ---------------- ir_pool_alloc_init ----------------
// Based on https://tia.mat.br/posts/2015/05/01/initializing_a_heap_allocated_structure_in_c.html
#define ir_pool_alloc_init(POOL, TYPE, ...) (TYPE*) ir_pool_memclone((POOL), (TYPE[]){ __VA_ARGS__ }, sizeof(TYPE))

// ---------------- ir_pool_fragment_t ----------------
// A memory fragment within an 'ir_pool_t'
typedef struct {
    void *memory; // Pointer to memory
    length_t used; // Memory being used (from 0 to n bytes)
    length_t capacity; // Memory block size in bytes
} ir_pool_fragment_t;

// ---------------- ir_pool_t ----------------
// A memory pool containing all allocated
// IR structures for an IR module
typedef struct {
    ir_pool_fragment_t *fragments; // Blocks of memory in which small allocations stored
    length_t length;
    length_t capacity;
} ir_pool_t;

// ---------------- ir_pool_snapshot_t ----------------
// A snapshot in time of the current memory usage of
// an 'ir_pool_t'
// Pool snapshots can be used to revert to a previous
// pool allocation state without much overhead. They
// are the only way to partially deallocate memory from a pool.
typedef struct {
    length_t used; // Bytes being used by current fragment
    length_t fragments_length; // Current fragment count
} ir_pool_snapshot_t;

// ---------------- ir_pool_init ----------------
// Initializes an IR memory pool
void ir_pool_init(ir_pool_t *pool);

// ---------------- ir_pool_alloc ----------------
// Allocates memory in an IR memory pool
void* ir_pool_alloc(ir_pool_t *pool, length_t bytes);

// ---------------- ir_pool_free ----------------
// Frees all memory allocated by an IR memory pool
void ir_pool_free(ir_pool_t *pool);

// ---------------- ir_pool_snapshot_capture ----------------
// Captures a snapshot of the current memory used of an IR pool
ir_pool_snapshot_t ir_pool_snapshot_capture(ir_pool_t *pool);

// ---------------- ir_pool_snapshot_restore ----------------
// Restores an IR pool to a previous memory usage snapshot
void ir_pool_snapshot_restore(ir_pool_t *pool, ir_pool_snapshot_t *snapshot);

// ---------------- ir_pool_memclone ----------------
// Creates a pool-allocated copy of a portion of memory
void *ir_pool_memclone(ir_pool_t *pool, const void *bytes, length_t num_bytes);

#endif // _ISAAC_IR_POOL_H
