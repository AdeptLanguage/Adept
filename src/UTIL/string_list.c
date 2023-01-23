
#include "UTIL/string.h"
#include "UTIL/string_list.h"

void strong_cstr_list_free(strong_cstr_list_t *list){
    free_strings(list->items, list->length);
}

strong_cstr_list_t strong_cstr_list_clone(const strong_cstr_list_t *list){
    const length_t count = list->length;
    strong_cstr_t *items = malloc(sizeof(strong_cstr_t) * count);

    for(length_t i = 0; i != count; i++){
        items[i] = strclone(list->items[i]);
    }

    return (strong_cstr_list_t){
        .items = items,
        .length = count,
        .capacity = count,
    };
}
