
#include "UTIL/list.h"

#include <stdlib.h>

#include "UTIL/ground.h"
#include "UTIL/util.h"

typedef listof(void, items) void_list_t;

void *list_append_new_impl(void *list_struct, length_t sizeof_element){
    void_list_t *list = (void_list_t*) list_struct;

    // Make room for new item
    expand((void**) &list->items, sizeof_element, list->length, &list->capacity, 1, 4);

    // Return new item
    return (void*) &(((char*) list->items)[list->length++ * sizeof_element]);
}

void list_qsort(void *list_struct, length_t sizeof_element, int (*cmp)(const void*, const void*)){
    void_list_t *list = (void_list_t*) list_struct;

    qsort(list->items, list->length, sizeof_element, cmp);
}
