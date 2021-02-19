
#include "IR/ir_pool.h"

void ir_pool_init(ir_pool_t *pool){
    pool->fragments = malloc(sizeof(ir_pool_fragment_t) * 4);
    pool->fragments_length = 1;
    pool->fragments_capacity = 4;
    pool->fragments[0].memory = malloc(512);
    pool->fragments[0].used = 0;
    pool->fragments[0].capacity = 512;
}

void* ir_pool_alloc(ir_pool_t *pool, length_t bytes){
    // NOTE: Allocates memory in an ir_pool_t
    ir_pool_fragment_t *recent_fragment = &pool->fragments[pool->fragments_length - 1];

    // Force alignment of every allocation to be POOL_ALLOCATION_ALIGNMENT
    if(bytes % POOL_ALLOCATION_ALIGNMENT != 0) bytes += POOL_ALLOCATION_ALIGNMENT - bytes % POOL_ALLOCATION_ALIGNMENT;
    
    while(recent_fragment->used + bytes >= recent_fragment->capacity){
        if(pool->fragments_length == pool->fragments_capacity){
            pool->fragments_capacity *= 2;
            ir_pool_fragment_t *new_fragments = malloc(sizeof(ir_pool_fragment_t) * pool->fragments_capacity);
            memcpy(new_fragments, pool->fragments, sizeof(ir_pool_fragment_t) * pool->fragments_length);
            free(pool->fragments);
            pool->fragments = new_fragments;

            // Update recent_fragment pointer to point into new array
            recent_fragment = &pool->fragments[pool->fragments_length - 1];
        }

        ir_pool_fragment_t *new_fragment = &pool->fragments[pool->fragments_length++];
        new_fragment->capacity = recent_fragment->capacity * 2;
        new_fragment->memory = malloc(new_fragment->capacity);
        new_fragment->used = 0;
        recent_fragment = new_fragment;
    }

    void *memory = (void*) &((char*) recent_fragment->memory)[recent_fragment->used];
    recent_fragment->used += bytes;
    return memory;
}

void ir_pool_free(ir_pool_t *pool){
    for(length_t f = 0; f != pool->fragments_length; f++){
        free(pool->fragments[f].memory);
    }
    free(pool->fragments);
}

void ir_pool_snapshot_capture(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    snapshot->used = pool->fragments[pool->fragments_length - 1].used;
    snapshot->fragments_length = pool->fragments_length;
}

void ir_pool_snapshot_restore(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    for(length_t f = pool->fragments_length; f != snapshot->fragments_length; f--){
        free(pool->fragments[f - 1].memory);
    }

    pool->fragments_length = snapshot->fragments_length;
    pool->fragments[snapshot->fragments_length - 1].used = snapshot->used;
}
