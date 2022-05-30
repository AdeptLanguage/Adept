
#include "IR/ir_proc_map.h"

#include <stdlib.h>
#include <string.h>

#include "UTIL/ground.h"
#include "UTIL/util.h"

static void ir_proc_map_expand(ir_proc_map_t *map, length_t sizeof_key){
    coexpand(
        (void**) &map->keys, sizeof_key,
        (void**) &map->endpoint_lists, sizeof *map->endpoint_lists,
        map->length, &map->capacity,
        1, 4
    );
}

void ir_proc_map_init(ir_proc_map_t *map, length_t sizeof_key, length_t estimated_keys){
    *map = (ir_proc_map_t){
        .keys = malloc(sizeof_key * estimated_keys),
        .endpoint_lists = malloc(sizeof(ir_func_endpoint_t) * estimated_keys),
        .length = 0,
        .capacity = estimated_keys,
        .endpoint_pool = {0},
    };

    ir_pool_init(&map->endpoint_pool);
}

void ir_proc_map_free(ir_proc_map_t *map){
    free(map->keys);

    for(length_t i = 0; i < map->length; i++){
        ir_func_endpoint_list_free(map->endpoint_lists[i]);
    }

    free(map->endpoint_lists);

    ir_pool_free(&map->endpoint_pool);
}

void ir_proc_map_insert(ir_proc_map_t *map, const void *key, length_t sizeof_key, ir_func_endpoint_t endpoint, int (*key_compare)(const void*, const void*)){
    length_t position = find_insert_position(map->keys, map->length, key_compare, key, sizeof_key);

    if(position < map->length && (*key_compare)(key, (char*) map->keys + sizeof_key * (position)) == 0){
        // Key already exists in map
    } else {
        // Key doesn't already exist in map

        ir_proc_map_expand(map, sizeof_key);

        memmove((char*) map->keys + sizeof_key * (position + 1), (char*) map->keys + sizeof_key * (position), sizeof_key * (map->length - position));
        memmove(&map->endpoint_lists[position + 1], &map->endpoint_lists[position], sizeof(*map->endpoint_lists) * (map->length - position));

        memcpy((char*) map->keys + sizeof_key * position, key, sizeof_key);
        map->endpoint_lists[position] = (ir_func_endpoint_list_t*) ir_pool_alloc(&map->endpoint_pool, sizeof(ir_func_endpoint_list_t));
        map->length += 1;

        memset(map->endpoint_lists[position], 0, sizeof(ir_func_endpoint_list_t));
    }

    ir_func_endpoint_list_insert(map->endpoint_lists[position], endpoint);
}

ir_func_endpoint_list_t *ir_proc_map_find(ir_proc_map_t *map, const void *key, length_t sizeof_key, int (*key_compare)(const void*, const void*)){
    void **found_key = bsearch(key, map->keys, map->length, sizeof_key, key_compare);

    if(found_key != NULL){
        // Use index of found key to access corresponding endpoint list
        length_t co_index = (length_t)((char*) found_key - (char*) map->keys) / sizeof_key;
        return map->endpoint_lists[co_index];
    } else {
        return NULL;
    }
}

int compare_ir_func_key(const void *raw_a, const void *raw_b){
    const ir_func_key_t *a = raw_a;
    const ir_func_key_t *b = raw_b;
    return strcmp(a->name, b->name);
}

int compare_ir_method_key(const void *raw_a, const void *raw_b){
    const ir_method_key_t *a = raw_a;
    const ir_method_key_t *b = raw_b;

    int compare = strcmp(a->struct_name, b->struct_name);
    if(compare != 0) return compare;

    return strcmp(a->method_name, b->method_name);
}
