
#include "BRIDGEIR/rtti_table.h"
#include "IRGEN/ir_gen_rtti.h"
#include "UTIL/builtin_type.h"

rtti_table_t rtti_table_create(rtti_collector_t rtti_collector, ast_t *ast){
    rtti_table_entry_list_t list = rtti_collector_collect(&rtti_collector);

    // Sort entries
    rtti_table_entry_list_sort(&list);

    // Annotate entries
    for(length_t i = 0; i < list.length; i++){
        rtti_table_entry_t *entry = &list.entries[i];

        if(ast_type_is_base(&entry->resolved_ast_type)){
            const char *name = ast_type_struct_name(&entry->resolved_ast_type);

            // Mark enums
            if(ast_type_is_anonymous_enum(&entry->resolved_ast_type) || (ast_composite_find_exact(ast, name) == NULL && !typename_is_extended_builtin_type(name) && ast_find_enum(ast->enums, ast->enums_length, name) >= 0)){
                entry->traits |= RTTI_TABLE_ENTRY_IS_ENUM;
            }
        } else if(ast_type_is_anonymous_enum(&entry->resolved_ast_type)){
            entry->traits |= RTTI_TABLE_ENTRY_IS_ENUM;
        }
    }

    return (rtti_table_t){
        .entries = list.entries,
        .length = list.length,
    };
}

rtti_table_entry_t *rtti_table_find(const rtti_table_t *rtti_table, const char *name){
    maybe_index_t index = rtti_table_find_index(rtti_table, name);

    if(index >= 0){
        return &rtti_table->entries[index];
    } else {
        return NULL;
    }
}

maybe_index_t rtti_table_find_index(const rtti_table_t *rtti_table, const char *name){
    rtti_table_entry_t *entries = rtti_table->entries;
    long long first = 0, middle = 0, last = (long long) rtti_table->length - 1, comparison = 0;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(entries[middle].name, name);

        if(comparison == 0){
            return middle;
        } else if(comparison > 0){
            last = middle - 1;
        } else {
            first = middle + 1;
        }
    }

    return comparison >= 0 ? middle : middle + 1;
}

void rtti_table_free(rtti_table_t *rtti_table){
    for(length_t i = 0; i < rtti_table->length; i++){
        rtti_table_entry_free(&rtti_table->entries[i]);
    }

    free(rtti_table->entries);
}
