
#include "UTIL/set.h"

void set_init(
    set_t *set,
    length_t starting_capacity,
    set_hash_func_t hash_func,
    set_equals_func_t equals_func,
    set_preinsert_clone_func_t optional_preinsert_clone_func
){
    *set = (set_t){
        .entries = calloc(starting_capacity, sizeof(set_entry_t)),
        .capacity = starting_capacity,
        .count = 0,
        .hash_func = hash_func,
        .equals_func = equals_func,
        .optional_preinsert_clone_func = optional_preinsert_clone_func,
    };
}

void set_free(set_t *set, set_free_item_func_t optional_free_func){
    if(optional_free_func != NULL){
        set_traverse(set, optional_free_func);
    }

    for(length_t i = 0; i != set->capacity; i++){
        set_entry_t *entry = set->entries[i];

        while(entry){
            set_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }
    }
}

bool set_insert(set_t *set, void *item){
    hash_t hash = (*set->hash_func)(item);
    set_entry_t **slot = &set->entries[hash % set->capacity];

    if(*slot == NULL){
        *slot = malloc_init(set_entry_t, {
            .data = set->optional_preinsert_clone_func ? (*set->optional_preinsert_clone_func)(item) : item,
            .next = NULL,
        });
    } else {
        set_entry_t *entry = *slot, *prev;

        while(entry){
            if((*set->equals_func)(entry->data, item)){
                return false;
            }

            prev = entry;
            entry = entry->next;
        }

        prev->next = malloc_init(set_entry_t, {
            .data = set->optional_preinsert_clone_func ? (*set->optional_preinsert_clone_func)(item) : item,
            .next = NULL,
        });
    }

    set->count++;
    return true;
}

void set_traverse(set_t *set, set_traverse_func_t run_func){
    for(length_t i = 0; i != set->capacity; i++){
        set_entry_t *entry = set->entries[i];

        while(entry){
            (*run_func)(entry->data);
            entry = entry->next;
        }
    }
}

void set_collect(set_t *set, set_collect_func_t collect_func, void *user_pointer){
    for(length_t i = 0; i != set->capacity; i++){
        set_entry_t *entry = set->entries[i];

        while(entry){
            (*collect_func)(entry->data, user_pointer);
            entry = entry->next;
        }
    }
}

void set_print_statistics(set_t *set){
    length_t buckets_used = 0;

    for(length_t i = 0; i != set->capacity; i++){
        set_entry_t *entry = set->entries[i];

        if(entry){
            buckets_used++;
        }
    }

    printf("[set statistics : %d items, %d / %d buckets, alpha=%f]\n", (int) set->count, (int) buckets_used, (int) set->capacity, (double) set->count / (double) set->capacity);
}
