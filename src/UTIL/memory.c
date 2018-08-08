
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "UTIL/memory.h"

#ifdef TRACK_MEMORY_USAGE

#undef malloc
#undef free

size_t global_memory_size;
memblock_t *global_memblocks;
size_t global_memblocks_length;
size_t global_memblocks_capacity;
bool global_memblocks_sorted;

void memory_init(){
    #ifndef TRACK_MEMORY_QUIET_UNLESS_LEAK
    printf("[MEMORY TRACKING] ENABLED!\n\n");
    #endif // TRACK_MEMORY_QUIET_UNLESS_LEAK

    global_memory_size = 0;
    global_memblocks = malloc(sizeof(memblock_t) * 256);
    global_memblocks_length = 0;
    global_memblocks_capacity = 256;
    global_memblocks_sorted = false;
}

int memblock_cmp(const void* a, const void *b){
    if(((memblock_t*) a)->pointer < ((memblock_t*) b)->pointer) { return -1; }
    else if(((memblock_t*) a)->pointer > ((memblock_t*) b)->pointer) { return 1; }
    else return 0;
}

void memory_sort(){
    qsort(global_memblocks, global_memblocks_length, sizeof(memblock_t), memblock_cmp);
    global_memblocks_sorted = true;
}


#ifdef TRACK_MEMORY_FILE_AND_LINE
void* memory_alloc(size_t size, const char *file, int line){
#else // TRACK_MEMORY_FILE_AND_LINE
void* memory_alloc(size_t size){
#endif // TRACK_MEMORY_FILE_AND_LINE
    if(global_memblocks_length == global_memblocks_capacity){
        memblock_t *new_memory_blocks = malloc(sizeof(memblock_t) * global_memblocks_capacity * 2);
        memcpy(new_memory_blocks, global_memblocks, sizeof(memblock_t) * global_memblocks_capacity);
        global_memblocks_capacity *= 2;
        free(global_memblocks);
        global_memblocks = new_memory_blocks;
    }

    memblock_t *memblock = &global_memblocks[global_memblocks_length++];
    memblock->pointer = malloc(size);
    memblock->size = size;
    memblock->freed = false;

    #ifdef TRACK_MEMORY_FILE_AND_LINE
    memblock->filename = file;
    memblock->line_number = line;
    #endif // TRACK_MEMORY_FILE_AND_LINE

    global_memory_size += size;
    return memblock->pointer;
}

#ifdef TRACK_MEMORY_FILE_AND_LINE
void memory_free(void* data, const char *file, int line){
#else // TRACK_MEMORY_FILE_AND_LINE
void memory_free(void* data){
#endif // TRACK_MEMORY_FILE_AND_LINE
    if(data == NULL) return;

    #ifdef TRACK_MEMORY_FILE_AND_LINE
    if(global_memblocks_sorted) { memory_free_fast(data, file, line); return; }
    #else // TRACK_MEMORY_FILE_AND_LINE
    if(global_memblocks_sorted) { memory_free_fast(data); return; }
    #endif // TRACK_MEMORY_FILE_AND_LINE

    for(size_t i = global_memblocks_length - 1; i != -1; i--){
        if(global_memblocks[i].pointer == data && !global_memblocks[i].freed){
            global_memory_size -= global_memblocks[i].size;
            global_memblocks[i].freed = true;
            free(data);
            return;
        }
    }

    #ifdef TRACK_MEMORY_FILE_AND_LINE
    printf("INTERNAL ERROR: Tried to free memory that isn't in the global memblocks table! (Address: 0x%p) - %s %d\n", data, file, line);
    #else // TRACK_MEMORY_FILE_AND_LINE
    printf("INTERNAL ERROR: Tried to free memory that isn't in the global memblocks table! (Address: 0x%p)\n", data);
    #endif
    return;
}

#ifdef TRACK_MEMORY_FILE_AND_LINE
void memory_free_fast(void* data, const char *file, int line){
#else // TRACK_MEMORY_FILE_AND_LINE
void memory_free_fast(void* data){
#endif // TRACK_MEMORY_FILE_AND_LINE

    // NOTE: Frees memory faster (but global_memblocks most be sorted first)
    // NOTE: Assumes not NULL
    int first, middle, last;
    first = 0; last = global_memblocks_length - 1;

    while(first <= last){
        middle = (first + last) / 2;

        if(global_memblocks[middle].pointer == data){
            while(middle != 0 && global_memblocks[middle - 1].pointer == data) middle--;
            while(global_memblocks[middle].freed) middle++;

            if(global_memblocks[middle].pointer == data){
                global_memory_size -= global_memblocks[middle].size;
                global_memblocks[middle].freed = true;
                free(data);
                return;
            } else break;
        }
        else if(global_memblocks[middle].pointer > data) last = middle - 1;
        else first = middle + 1;
    }

    #ifdef TRACK_MEMORY_FILE_AND_LINE
    printf("INTERNAL ERROR: FAST FREE: Tried to free memory that isn't in the global memblocks table! (Address: 0x%p) - %s %d\n", data, file, line);
    #else // TRACK_MEMORY_FILE_AND_LINE
    printf("INTERNAL ERROR: FAST FREE: Tried to free memory that isn't in the global memblocks table! (Address: 0x%p)\n", data);
    #endif
    return;
}

void memory_scan(){
    // Scans for unfreed memory blocks
    bool found_unfreed = false;

    #ifndef TRACK_MEMORY_QUIET_UNLESS_LEAK
    printf("\n");
    #endif // TRACK_MEMORY_QUIET_UNLESS_LEAK

    for(size_t i = 0; i != global_memblocks_length; i++){
        if(!global_memblocks[i].freed){
            #ifdef TRACK_MEMORY_QUIET_UNLESS_LEAK
            if(!found_unfreed) printf("\n");
            #endif // TRACK_MEMORY_QUIET_UNLESS_LEAK

            found_unfreed = true;

            #ifdef TRACK_MEMORY_FILE_AND_LINE
            printf("[MEMORY TRACKING] Found unfreed block! (Size: %04lu) (Address: 0x%p) - %s %d\n", (unsigned long) global_memblocks[i].size, global_memblocks[i].pointer, global_memblocks[i].filename, global_memblocks[i].line_number);
            #else // TRACK_MEMORY_FILE_AND_LINE
            printf("[MEMORY TRACKING] Found unfreed block! (Size: %04lu) (Address: 0x%p)\n", (unsigned long) global_memblocks[i].size, global_memblocks[i].pointer);
            #endif // TRACK_MEMORY_FILE_AND_LINE

            free(global_memblocks[i].pointer);
        }
    }

    if(found_unfreed){
        printf("[MEMORY TRACKING] Memory leaks found! (Memory left: %lu)\n", (unsigned long) global_memory_size);
    } else {
        #ifndef TRACK_MEMORY_QUIET_UNLESS_LEAK
        printf("[MEMORY TRACKING] All memory freed! No memory leaks found!\n");
        printf("[MEMORY TRACKING] Total allocations: %lu / %lu\n", (unsigned long) global_memblocks_length, (unsigned long) global_memblocks_capacity);
        #endif // TRACK_MEMORY_QUIET_UNLESS_LEAK
    }
}

#endif // TRACKING_MEMORY_USAGE
