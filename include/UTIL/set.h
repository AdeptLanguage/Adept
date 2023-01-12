
#ifndef _ISAAC_SET_H
#define _ISAAC_SET_H

#include "UTIL/ground.h"
#include "UTIL/hash.h"

// ---------------- set_hash_func_t ----------------
// A function that a set collection uses to hash its elements
typedef hash_t (*set_hash_func_t)(const void *);

// ---------------- set_equals_func_t ----------------
// A function that a set collection uses to determine if two elements are equal
typedef bool (*set_equals_func_t)(const void *, const void *);

// ---------------- set_preinsert_clone_func_t ----------------
// A function that a set collection uses to clone an element before inserting it
typedef void *(*set_preinsert_clone_func_t)(const void *);

// ---------------- set_free_item_func_t ----------------
// A function that a set collection uses to free an element
typedef void (*set_free_item_func_t)(void *item);

// ---------------- set_traverse_func_t ----------------
// A function that can be run when traversing a set collection
typedef void (*set_traverse_func_t)(void *item);

// ---------------- set_collect_func_t ----------------
// A function that can be run when collecting over a set collection
typedef void (*set_collect_func_t)(void *item, void *user_pointer);

// ---------------- set_entry_t ----------------
// An entry in a set collection
typedef struct set_entry {
    void *data;
    struct set_entry *next;
} set_entry_t;

// ---------------- set_t ----------------
// A set collection
typedef struct {
    set_entry_t **entries;
    length_t capacity;
    length_t count;

    set_hash_func_t hash_func;
    set_equals_func_t equals_func;
    set_preinsert_clone_func_t optional_preinsert_clone_func;
} set_t;

// ---------------- set_init ----------------
// Initializes a set collection
void set_init(
    set_t *set,
    length_t starting_capacity,
    set_hash_func_t hash_func,
    set_equals_func_t equals_func,
    set_preinsert_clone_func_t optional_preinsert_clone_func
);

// ---------------- set_free ----------------
// Frees a set collection
// `optional_free_func` may be non-NULL to indicate how to clean up inside of individual items
void set_free(set_t *set, set_free_item_func_t optional_free_func);

// ---------------- set_insert ----------------
// Inserts an item into a set
// The pointer `item` will be taken as-is unless `set->optional_preinsert_clone_func` is used to change this behavior
bool set_insert(set_t *set, void *item);

// ---------------- set_traverse ----------------
// Traverses a set without any outside influence
void set_traverse(set_t *set, set_traverse_func_t run_func);

// ---------------- set_collect ----------------
// Traverses a set with access to an outside influence `user_pointer`
void set_collect(set_t *set, set_collect_func_t collect_func, void *user_pointer);

// ---------------- set_print_statistics ----------------
// Prints various statistics about a set collection
void set_print_statistics(set_t *set);

#endif // _ISAAC_SET_H
