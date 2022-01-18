
#include "UTIL/string.h"
#include "UTIL/string_list.h"

void cstr_list_free(strong_cstr_list_t *list){
    free_strings(list->items, list->length);
}
