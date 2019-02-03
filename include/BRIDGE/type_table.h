
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

// ---------------- type_table_record_t ----------------
// A single record within a bridging type table
typedef struct {
    strong_cstr_t name;
    ast_type_t ast_type;

    #ifndef ADEPT_INSIGHT_BUILD
    ir_type_t *ir_type;
    #endif
    
    bool is_alias;
} type_table_record_t;

// ---------------- type_table_t ----------------
// A bridging type table
// (used to generate runtime type table)
typedef struct {
    type_table_record_t *records;
    length_t length;
    length_t capacity;
    bool reduced;
} type_table_t;

// ---------------- type_table_init ----------------
// Initializes a blank bridging type table
void type_table_init(type_table_t *table);

// ---------------- type_table_free ----------------
// Frees a bridging type table
void type_table_free(type_table_t *table);

// ---------------- type_table_give ----------------
// Mentions a type to a bridging type table
void type_table_give(type_table_t *table, ast_type_t *type, maybe_null_strong_cstr_t maybe_alias_name);

// ---------------- type_table_give_base ----------------
// (same as type_table_give with an ast_type_t containing a single ast_elem_base_t)
void type_table_give_base(type_table_t *table, weak_cstr_t name);

// ---------------- type_table_reduce ----------------
// Reduces a bridging type table by removing duplicate entries
// and sorting the remaining unique records
void type_table_reduce(type_table_t *table);

// ---------------- type_table_find ----------------
// Finds a type_table_record_t within a bridging type table
maybe_index_t type_table_find(type_table_t *table, const char *name);

// ---------------- type_table_records_free ----------------
// Frees a set of bridging type table records
void type_table_records_free(type_table_record_t *records, length_t length);

// ---------------- type_table_record_cmp ----------------
// Comparision function used for sorting type_table_record_t entries
int type_table_record_cmp(const void *a, const void *b);

#ifdef __cplusplus
}
#endif

#endif // TYPE_TABLE_H
