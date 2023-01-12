
#include "AST/ast_type.h"
#include "BRIDGEIR/rtti_table_entry.h"

extern inline void rtti_table_entry_list_append_unchecked(rtti_table_entry_list_t *list, rtti_table_entry_t entry);

static inline int rtti_table_entry_list_cmp(const void *raw_a, const void *raw_b){
    const rtti_table_entry_t *a = (rtti_table_entry_t*) raw_a;
    const rtti_table_entry_t *b = (rtti_table_entry_t*) raw_b;
    return strcmp(a->name, b->name);
}

void rtti_table_entry_free(rtti_table_entry_t *entry){
    free(entry->name);
    ast_type_free(&entry->resolved_ast_type);
}

void rtti_table_entry_list_sort(rtti_table_entry_list_t *entries){
    list_qsort((void*) entries, sizeof(rtti_table_entry_t), &rtti_table_entry_list_cmp);
}
