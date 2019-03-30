
#include <string.h>
#include "UTIL/improved_qsort.h"

void improved_qsort(void *array, size_t length, size_t unit_size, int (*cmp_func)(void *user, void *a, void *b), void *user_data){
    if(length < 2) return;

    size_t partition_index = improved_qsort_partition(array, 0, length - 1, unit_size, cmp_func, user_data);
    improved_qsort(array, partition_index, unit_size, cmp_func, user_data);
    improved_qsort((char*) array + (partition_index + 1) * unit_size, length - partition_index - 1, unit_size, cmp_func, user_data);
}

size_t improved_qsort_partition(void *array, size_t beginning, size_t end, size_t unit_size, int (*cmp_func)(void *user, void *a, void *b), void *user_data){
    void *pivot = (char*) array + (end * unit_size);
    int i = beginning - 1;
    char tmp[unit_size];

    for(int j = beginning; j < end; j++){
        if(cmp_func(user_data, (char*) array + j * unit_size, pivot) <= 0){
            i++;

            memcpy(tmp, (char*) array + i * unit_size, unit_size);
            memcpy((char*) array + i * unit_size, (char*) array + j * unit_size, unit_size);
            memcpy((char*) array + j * unit_size, tmp, unit_size);
        }
    }

    memcpy(tmp, (char*) array + (i + 1) * unit_size, unit_size);
    memcpy((char*) array + (i + 1) * unit_size, (char*) array + end * unit_size, unit_size);
    memcpy((char*) array + end * unit_size, tmp, unit_size);
    return i + 1;
}
