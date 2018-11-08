
#include "UTIL/util.h"
#include "BRIDGE/type_table.h"

void type_table_init(type_table_t *table){
    table->records = NULL;
    table->length = 0;
    table->capacity = 0;
    table->reduced = false;
}

void type_table_free(type_table_t *table){
    if(table == NULL) return;
    type_table_records_free(table->records, table->length);
    free(table->records);
}

void type_table_give(type_table_t *table, ast_type_t *type, maybe_null_strong_cstr_t maybe_alias_name){
    expand((void**) &table->records, sizeof(type_table_record_t), table->length, &table->capacity, 1, 16);
    
    strong_cstr_t name = maybe_alias_name ? maybe_alias_name : ast_type_str(type);
    table->records[table->length].name = name;
    table->records[table->length].ast_type = ast_type_clone(type);
    table->records[table->length].ir_type = NULL;
    table->records[table->length++].is_alias = (maybe_alias_name != NULL);

    if(type->elements_length != 0 && type->elements[0]->id == AST_ELEM_POINTER){
        ast_type_t subtype = ast_type_clone(type);
        // Modify ast_type_t to remove a pointer element from the front
        // DANGEROUS: Manually deleting ast_elem_pointer_t
        free(subtype.elements[0]);
        memmove(subtype.elements, &subtype.elements[1], sizeof(ast_elem_t*) * (subtype.elements_length - 1));
        subtype.elements_length--; // Reduce length accordingly

        type_table_give(table, &subtype, NULL); // TODO: Maybe determine whether or not subtype is alias??
        ast_type_free(&subtype);
    }
}

void type_table_give_base(type_table_t *table, weak_cstr_t base){
    expand((void**) &table->records, sizeof(type_table_record_t), table->length, &table->capacity, 1, 16);

    strong_cstr_t name = strclone(base);
    table->records[table->length].name = name;
    ast_type_make_base(&table->records[table->length].ast_type, strclone(base));
    table->records[table->length].ir_type = NULL;
    table->records[table->length++].is_alias = false;
}

void type_table_reduce(type_table_t *table){
    if(table->reduced) return;

    qsort(table->records, table->length, sizeof(type_table_record_t), type_table_record_cmp);

    for(length_t i = 0; i != table->length; i++){
        if(i + 1 != table->length && strcmp(table->records[i].name, table->records[i + 1].name) == 0){
            // Remove this entry, because next entry is identical
            type_table_records_free(&table->records[i], 1);

            memcpy(&table->records[i], &table->records[i + 1], (table->length - i - 1) * sizeof(type_table_record_t));
            table->length--;
            i--;
        }
    }
}

maybe_index_t type_table_find(type_table_t *table, const char *name){
    // TODO: Make this fast

    for(length_t i = 0; i != table->length; i++){
        if(strcmp(table->records[i].name, name) == 0) return i;
    }

    return -1;
}

void type_table_records_free(type_table_record_t *records, length_t length){
    for(length_t i = 0; i != length; i++){
        free(records[i].name);
        ast_type_free(&records[i].ast_type);
    }
}

int type_table_record_cmp(const void *a, const void *b){
    return strcmp(((type_table_record_t*) a)->name, ((type_table_record_t*) b)->name);
} 
