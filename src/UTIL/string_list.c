
#include "UTIL/search.h"
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

maybe_index_t strong_cstr_list_bsearch(const strong_cstr_list_t *list, const char *item){
    return binary_string_search(list->items, list->length, item);
}

void strong_cstr_list_sort(strong_cstr_list_t *list){
    qsort(list->items, list->length, sizeof(strong_cstr_t), (int(*)(const void *, const void *)) &string_compare_for_qsort);
}

void strong_cstr_list_presorted_remove_duplicates(strong_cstr_list_t *list){
    for(length_t i = 1; i < list->length; i++){
        char *prev = list->items[i - 1];
        char *current = list->items[i];

        if(streq(prev, current)){
            free(current);
            memmove(&list->items[i], &list->items[i + 1], (list->length - i - 1) * sizeof(strong_cstr_t));
            list->length--;
        }
    }
}

const char *strong_cstr_list_presorted_has_duplicates(strong_cstr_list_t *list){
    for(length_t i = 1; i < list->length; i++){
        const char *prev = list->items[i - 1];
        const char *current = list->items[i];

        if(streq(prev, current)){
            return current;
        }
    }

    return NULL;
}

bool strong_cstr_list_equals(strong_cstr_list_t *a, strong_cstr_list_t *b){
    if(a->length != b->length) return false;

    for(length_t i = 0; i != a->length; i++){
        if(!streq(a->items[i], b->items[i])) return false;
    }

    return true;
}
