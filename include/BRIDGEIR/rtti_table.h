
#ifndef _ISAAC_RTTI_TABLE_H
#define _ISAAC_RTTI_TABLE_H

#include "AST/ast.h"
#include "BRIDGE/rtti_collector.h"
#include "BRIDGEIR/rtti_table_entry.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"

// ---------------- rtti_table_t ----------------
// A table of finalized runtime type information
typedef struct {
    rtti_table_entry_t *entries;
    length_t length;
} rtti_table_t;

// ---------------- rtti_table_create ----------------
// Creates an RTTI table
rtti_table_t rtti_table_create(rtti_collector_t collector, ast_t *ast);

// ---------------- rtti_table_free ----------------
// Frees an RTTI table
void rtti_table_free(rtti_table_t *rtti_table);

// ---------------- rtti_table_find ----------------
// Finds an entry in an RTTI table
rtti_table_entry_t *rtti_table_find(const rtti_table_t *rtti_table, const char *name);

// ---------------- rtti_table_find_index ----------------
// Finds the index of an entry in an RTTI table
maybe_index_t rtti_table_find_index(const rtti_table_t *rtti_table, const char *name);

#endif // _ISAAC_RTTI_TABLE_H
