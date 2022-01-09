
#include "UTIL/list.h"
#include "UTIL/util.h"
#include "UTIL/ground.h"

typedef listof(void, items) void_list_t;

void *list_append_new_impl(void *raw_list, length_t sizeof_element){
    void_list_t *list = (void_list_t*) raw_list;

    // Make room for new item
    expand((void**) &list->items, sizeof_element, list->length, &list->capacity, 1, 4);

    // Return new item
    return (void*) &(((char*) list->items)[list->length++ * sizeof_element]);
}
