
#ifndef TYPE_TABLE_H
#define TYPE_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "UTIL/ground.h"
#include "AST/ast_type.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "IR/ir_type.h"
#endif

// ---------------- type_table_entry_t ----------------
// A single entry within a bridging type table
typedef struct {
    strong_cstr_t name;
    ast_type_t ast_type;

    #ifndef ADEPT_INSIGHT_BUILD
    ir_type_t *ir_type;
    #endif
    
    bool is_alias;
} type_table_entry_t;

// ---------------- type_table_t ----------------
// A bridging type table
typedef struct {
    type_table_entry_t *entries;
    length_t length;
    length_t capacity;
} type_table_t;

// ---------------- type_table_init ----------------
// Initializes a blank bridging type table
void type_table_init(type_table_t *table);

// ---------------- type_table_free ----------------
// Frees a bridging type table
void type_table_free(type_table_t *table);

// ---------------- type_table_find ----------------
// Finds an entry in a bridging type table
maybe_index_t type_table_find(type_table_t *table, weak_cstr_t name);

// ---------------- type_table_locate ----------------
// Locates an entry in a bridging type table
// Returns whether found matching entry
// NOTE: out_position must not be NULL
bool type_table_locate(type_table_t *table, weak_cstr_t name, int *out_position);

// ---------------- type_table_give ----------------
// Mentions a type to a bridging type table
void type_table_give(type_table_t *table, ast_type_t *type, maybe_null_strong_cstr_t maybe_alias_name);

// ---------------- type_table_give_base ----------------
// Mentions a base type to a bridging type table
void type_table_give_base(type_table_t *table, weak_cstr_t base);

// ---------------- type_table_add ----------------
// Adds entry if it has a unique name
// NOTE: Returns whether the entry was added
// NOTE: If call fails, 'entry.name' will most likely need to be freed
// DANGEROUS: This function is really quirky, and should avoided when possible
//            Use 'type_table_give' instead!
/*
    -----------------------------------------------------------------
                        Ownership transfer details
    -----------------------------------------------------------------
    typedef struct {
        strong_cstr_t name;                Ownership taken if added
        ast_type_t ast_type;               Ownership never taken (cloned instead)

        #ifndef ADEPT_INSIGHT_BUILD
        ir_type_t *ir_type;                Ownershipless
        #endif
        
        bool is_alias;                     Ownershipless
    } type_table_entry_t;
    -----------------------------------------------------------------
*/
bool type_table_add(type_table_t *table, type_table_entry_t entry);

#ifdef __cplusplus
}
#endif

#endif // TYPE_TABLE_H
