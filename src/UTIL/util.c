
#include "UTIL/util.h"

void expand(void **inout_memory, length_t unit_size, length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity){
    // Expands an array in memory to be able to fit more units

    if(*inout_capacity == 0 && amount != 0){
        *inout_memory = malloc(unit_size * default_capacity);
        *inout_capacity = default_capacity;
    }

    while(length + amount > *inout_capacity){
        *inout_capacity *= 2;
        
        #ifdef UTIL_USE_REALLOC
        *inout_memory = realloc(*inout_memory, unit_size * *inout_capacity);
        #else
        void *new_memory = malloc(unit_size * *inout_capacity);
        memcpy(new_memory, *inout_memory, unit_size * length);
        free(*inout_memory);
        *inout_memory = new_memory;
        #endif
    }
}

void coexpand(void **inout_memory1, length_t unit_size1, void **inout_memory2, length_t unit_size2, length_t length, length_t *inout_capacity, length_t amount, length_t default_capacity){
    // Expands an array in memory to be able to fit more units

    if(*inout_capacity == 0 && amount != 0){
        *inout_memory1 = malloc(unit_size1 * default_capacity);
        *inout_memory2 = malloc(unit_size2 * default_capacity);
        *inout_capacity = default_capacity;
    }

    while(length + amount > *inout_capacity){
        *inout_capacity *= 2;

        #ifdef UTIL_USE_REALLOC
        *inout_memory1 = realloc(*inout_memory1, unit_size1 * *inout_capacity);
        *inout_memory2 = realloc(*inout_memory2, unit_size2 * *inout_capacity);
        #else
        void *new_memory = malloc(unit_size1 * *inout_capacity);
        memcpy(new_memory, *inout_memory1, unit_size1 * length);
        free(*inout_memory1);
        *inout_memory1 = new_memory;

        new_memory = malloc(unit_size2 * *inout_capacity);
        memcpy(new_memory, *inout_memory2, unit_size2 * length);
        free(*inout_memory2);
        *inout_memory2 = new_memory;
        #endif
    }
}

#ifdef UTIL_USE_REALLOC
void grow_impl(void **inout_memory, length_t unit_size, length_t new_length){
#else
void grow_impl(void **inout_memory, length_t unit_size, length_t old_length, length_t new_length){
#endif
    #ifdef UTIL_USE_REALLOC
    *inout_memory = realloc(*inout_memory, unit_size * new_length);
    #else
    void *memory = malloc(unit_size * new_length);
    memcpy(memory, *inout_memory, unit_size * old_length);
    free(*inout_memory);
    *inout_memory = memory;
    #endif
}

strong_cstr_t strclone(const char *src){
    length_t size = strlen(src) + 1;
    char *clone = malloc(size);
    memcpy(clone, src, size);
    return clone;
}

void freestrs(strong_cstr_t *array, length_t length){
    for(length_t i = 0; i != length; i++) free(array[i]);
    free(array);
}
