
#include "UTIL/search.h"

#include <string.h>

maybe_index_t binary_string_search(char *strings[], length_t string_count, const char *target){
    // Searches for 'target' inside 'strings'
    // If not found returns -1 else returns index inside array

    maybe_index_t first = 0, middle, last = string_count - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(strings[middle], target);

        if(comparison < 0){
            first = middle + 1;
        } else if(comparison > 0){
            last = middle - 1;
        } else {
            return middle;
        }
    }

    return -1;
}

maybe_index_t binary_string_search_const(const char *const strings[], length_t string_count, const char *target){
    // Searches for 'target' inside 'strings'
    // If not found returns -1 else returns index inside array

    maybe_index_t first = 0, middle, last = string_count - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(strings[middle], target);

        if(comparison < 0){
            first = middle + 1;
        } else if(comparison > 0){
            last = middle - 1;
        } else {
            return middle;
        }
    }

    return -1;
}

maybe_index_t binary_int_search(const int ints[], length_t int_count, int target){
    // Searches for 'target' inside 'ints'
    // If not found returns -1 else returns index inside array

    maybe_index_t first, middle, last, comparison;
    first = 0; last = int_count - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = ints[middle] - target;

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}
