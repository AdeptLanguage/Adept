
#include "IR/ir_pool.h"

#include <stdlib.h>
#include <string.h>

void ir_pool_init(ir_pool_t *pool){
    *pool = (ir_pool_t){
        .fragments = malloc(sizeof(ir_pool_fragment_t) * 4),
        .length = 1,
        .capacity = 4,
    };

    pool->fragments[0] = (ir_pool_fragment_t){
        .memory = malloc(512),
        .used = 0,
        .capacity = 512,
    };
}

void* ir_pool_alloc(ir_pool_t *pool, length_t bytes){
    ir_pool_fragment_t *recent_fragment = &pool->fragments[pool->length - 1];

    // Force alignment of every allocation to have alignment POOL_ALLOCATION_ALIGNMENT
    if(bytes % POOL_ALLOCATION_ALIGNMENT != 0) bytes += POOL_ALLOCATION_ALIGNMENT - bytes % POOL_ALLOCATION_ALIGNMENT;
    
    while(recent_fragment->used + bytes >= recent_fragment->capacity){
        if(pool->length == pool->capacity){
            pool->capacity *= 2;

            ir_pool_fragment_t *new_fragments = malloc(sizeof(ir_pool_fragment_t) * pool->capacity);
            memcpy(new_fragments, pool->fragments, sizeof(ir_pool_fragment_t) * pool->length);
            free(pool->fragments);
            pool->fragments = new_fragments;

            // Update recent_fragment pointer to point into new array
            recent_fragment = &pool->fragments[pool->length - 1];
        }

        ir_pool_fragment_t *next_fragment = &pool->fragments[pool->length++];
        length_t next_fragment_size = recent_fragment->capacity * 2;

        *next_fragment = (ir_pool_fragment_t){
            .memory = malloc(next_fragment_size),
            .used = 0,
            .capacity = next_fragment_size,
        };

        recent_fragment = next_fragment;
    }

    void *memory = (void*) &((char*) recent_fragment->memory)[recent_fragment->used];
    recent_fragment->used += bytes;
    return memory;
}

void ir_pool_free(ir_pool_t *pool){
    for(length_t f = 0; f != pool->length; f++){
        free(pool->fragments[f].memory);
    }
    free(pool->fragments);
}

void ir_pool_snapshot_capture(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    snapshot->used = pool->fragments[pool->length - 1].used;
    snapshot->fragments_length = pool->length;
}

void ir_pool_snapshot_restore(ir_pool_t *pool, ir_pool_snapshot_t *snapshot){
    for(length_t f = pool->length; f != snapshot->fragments_length; f--){
        free(pool->fragments[f - 1].memory);
    }

    pool->length = snapshot->fragments_length;
    pool->fragments[snapshot->fragments_length - 1].used = snapshot->used;
}
