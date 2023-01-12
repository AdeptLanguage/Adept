
#ifndef _ISAAC_RTTI_TABLE_ENTRY_H
#define _ISAAC_RTTI_TABLE_ENTRY_H

#include "AST/ast_type_lean.h"
#include "IR/ir_type.h"
#include "UTIL/trait.h"
#include "UTIL/list.h"
#include "UTIL/ground.h"

#define RTTI_TABLE_ENTRY_IS_ALIAS TRAIT_1
#define RTTI_TABLE_ENTRY_IS_ENUM TRAIT_2

// ---------------- rtti_table_entry_t ----------------
// A single entry in an RTTI table
typedef struct {
    strong_cstr_t name;
    ast_type_t resolved_ast_type;
    trait_t traits;
    ir_type_t *ir_type;
} rtti_table_entry_t;

void rtti_table_entry_free(rtti_table_entry_t *entry);

// ---------------- rtti_table_entry_list_t ----------------
// A list of RTTI table entries
typedef listof(rtti_table_entry_t, entries) rtti_table_entry_list_t;

// ---------------- rtti_table_entry_list_append ----------------
// Appends an entry to a list of RTTI table entries
#define rtti_table_entry_list_append(LIST, VALUE) list_append((LIST), (VALUE), rtti_table_entry_t);

// ---------------- rtti_table_entry_list_append_unchecked ----------------
// Appends an entry to a list of RTTI table entries without checking that enough space exists
inline void rtti_table_entry_list_append_unchecked(rtti_table_entry_list_t *list, rtti_table_entry_t entry){
    list->entries[list->length++] = entry;
}

// ---------------- rtti_table_entry_list_sort ----------------
// Sorts a list of RTTI table entries
void rtti_table_entry_list_sort(rtti_table_entry_list_t *entries);

#endif // _ISAAC_RTTI_TABLE_ENTRY_H
