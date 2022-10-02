
#include "IR/ir_type_map.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void ir_type_map_free(ir_type_map_t *type_map){
    free(type_map->mappings);
}

successful_t ir_type_map_find(ir_type_map_t *type_map, const char *name, ir_type_t **type_ptr){
    // Does a binary search on the type map to find the requested type by name

    ir_type_mapping_t *mappings = type_map->mappings;

    long long first = 0, middle, last = (long long) type_map->length - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            *type_ptr = mappings[middle].type;
            return true;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return false;
}
