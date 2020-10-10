
#include "UTIL/util.h"
#include "BRIDGE/type_table.h"

void type_table_init(type_table_t *table){
    table->entries = NULL;
    table->length = 0;
    table->capacity = 0;
}

void type_table_free(type_table_t *table){
    if(table == NULL) return;
    for(length_t i = 0; i != table->length; i++){
        free(table->entries[i].name);
        ast_type_free(&table->entries[i].ast_type);
    }
    free(table->entries);
}

bool type_table_add(type_table_t *table, type_table_entry_t entry){
    // DANGEROUS: This function is really quirky, and should avoided when possible
    //            Use 'type_table_give' instead!

    // See definition for details on how to use this function

    int position;

    // Don't add if it already exists
    if(type_table_locate(table, entry.name, &position)) return false;

    // Expand entries list
    if(table->length == table->capacity){
        table->capacity += 64;
        
        type_table_entry_t *new_entries = malloc(sizeof(type_table_entry_t) * table->capacity);
        memcpy(new_entries, table->entries, sizeof(type_table_entry_t) * table->length);
        free(table->entries);
        table->entries = new_entries;
    }

    memmove(&table->entries[position + 1], &table->entries[position], (table->length - position) * sizeof(type_table_entry_t));
    table->length++;

    // Instantiate entry with it's own data
    entry.ast_type = ast_type_clone(&entry.ast_type);
    table->entries[position] = entry;
    return true;
}

void type_table_give(type_table_t *table, ast_type_t *type, maybe_null_strong_cstr_t maybe_alias_name){
    if(ast_type_has_polymorph(type)) return;

    type_table_entry_t weak_ast_type_entry;
    weak_ast_type_entry.name = maybe_alias_name ? maybe_alias_name : ast_type_str(type);
    weak_ast_type_entry.ast_type = *type;
    #ifndef ADEPT_INSIGHT_BUILD
    weak_ast_type_entry.ir_type = NULL;
    #endif
    weak_ast_type_entry.is_alias = (maybe_alias_name != NULL);

    // Don't add if it already exists
    if(!type_table_add(table, weak_ast_type_entry)){
        free(weak_ast_type_entry.name);
        return;
    }

    // HACK: Add extra entry for pointer to given type
    // since we cannot know whether or not '&' is even used on that type
    // (with the current system)
    // Also, this may turn out to be benificial for formulating types that
    // may not have been directly refered to at compile time
    if(strcmp(weak_ast_type_entry.name, "void") != 0){
        ast_type_t with_additional_ptr = ast_type_clone(type);
        ast_type_prepend_ptr(&with_additional_ptr);

        type_table_entry_t strong_ast_type_entry;
        strong_ast_type_entry.name = ast_type_str(&with_additional_ptr);
        strong_ast_type_entry.ast_type = with_additional_ptr;

        #ifndef ADEPT_INSIGHT_BUILD
        strong_ast_type_entry.ir_type = NULL;
        #endif

        strong_ast_type_entry.is_alias = false;

        if(!type_table_add(table, strong_ast_type_entry)){
            ast_type_free(&strong_ast_type_entry.ast_type);
            free(strong_ast_type_entry.name);
            return;
        }

        ast_type_free(&strong_ast_type_entry.ast_type);
    }

    // Mention sub types to the type table
    if(type->elements_length != 0){
        ast_type_t subtype;
        
        switch(type->elements[0]->id){
        case AST_ELEM_POINTER:
            subtype = ast_type_clone(type);
            ast_type_dereference(&subtype);
            type_table_give(table, &subtype, NULL); // TODO: Maybe determine whether or not subtype is alias??
            ast_type_free(&subtype);
            break;
        case AST_ELEM_FIXED_ARRAY:
            subtype = ast_type_clone(type);
            ast_type_unwrap_fixed_array(&subtype);
            type_table_give(table, &subtype, NULL); // TODO: Maybe determine whether or not subtype is alias??
            ast_type_free(&subtype);
            break;
        }
    }
}

void type_table_give_base(type_table_t *table, weak_cstr_t base){
    type_table_entry_t weak_ast_type_entry;
    weak_ast_type_entry.name = strclone(base);

    // Static AST type for base
    ast_elem_base_t static_pointer_element;
    static_pointer_element.id = AST_ELEM_POINTER;
    static_pointer_element.source = NULL_SOURCE;
    ast_elem_base_t static_base_element;
    static_base_element.id = AST_ELEM_BASE;
    static_base_element.base = weak_ast_type_entry.name;
    static_base_element.source = NULL_SOURCE;
    ast_elem_t *static_elements[2];
    static_elements[0] = (ast_elem_t*) &static_pointer_element;
    static_elements[1] = (ast_elem_t*) &static_base_element;
    weak_ast_type_entry.ast_type.elements = &static_elements[1];
    weak_ast_type_entry.ast_type.elements_length = 1;
    weak_ast_type_entry.ast_type.source = NULL_SOURCE;

    #ifndef ADEPT_INSIGHT_BUILD
    weak_ast_type_entry.ir_type = NULL;
    #endif
    weak_ast_type_entry.is_alias = false;

    bool added = type_table_add(table, weak_ast_type_entry);

    // Since we cannot know whether or not '&' is ever used on the type,
    // force another type entry which is a pointer the the type
    // (unless of course the type is void, cause *void is invalid)
    if(strcmp(weak_ast_type_entry.name, "void") != 0){
        // Add *T type table entry
        weak_ast_type_entry.ast_type.elements = &static_elements[0];
        weak_ast_type_entry.ast_type.elements_length = 2;
        weak_ast_type_entry.ast_type.source = NULL_SOURCE;
        weak_ast_type_entry.name = ast_type_str(&weak_ast_type_entry.ast_type);
        
        if(!type_table_add(table, weak_ast_type_entry)){
            free(weak_ast_type_entry.name);
        }
    }
    
    if(!added){
        free(static_base_element.base);
    }
}

bool type_table_locate(type_table_t *table, weak_cstr_t name, int *out_position){
    int first = 0, middle = 0, last = table->length - 1, comparison = 0;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(table->entries[middle].name, name);

        if(comparison == 0){
            *out_position = middle;
            return true;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    *out_position = comparison >= 0 ? middle : middle + 1;
    return false;
}

maybe_index_t type_table_find(type_table_t *table, weak_cstr_t name){
    int position;
    if(type_table_locate(table, name, &position)) return position;
    return -1;
}
