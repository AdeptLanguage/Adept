
#include "util.h"
#include "color.h"
#include "ast_var.h"
#include "levenshtein.h"

void ast_var_scope_init(ast_var_scope_t *out_var_scope, ast_var_scope_t *parent){
    out_var_scope->parent = parent;
    out_var_scope->variables.names = NULL;
    out_var_scope->variables.types = NULL;
    out_var_scope->variables.length = 0;
    out_var_scope->variables.capacity = 0;
    out_var_scope->children = NULL;
    out_var_scope->children_length = 0;
    out_var_scope->children_capacity = 0;
}

void ast_var_scope_free(ast_var_scope_t *scope){
    if(scope->parent != NULL) redprintf("INTERNAL ERROR: TRIED TO FREE A VARIABLE SCOPE THAT HAS A PARENT, ignoring...");

    for(length_t i = 0; i != scope->children_length; i++){
        ast_var_scope_free(scope->children[i]);
        free(scope->children[i]);
    }

    free(scope->variables.names);
    free(scope->variables.types);
    free(scope->children);
}

void ast_var_list_add_variables(ast_var_list_t *list, const char **names, ast_type_t *type, length_t length){
    coexpand((void**) &list->names, sizeof(const char*), (void**) &list->types,
        sizeof(ast_type_t*), list->length, &list->capacity, length, 4);

    for(length_t i = 0; i != length; i++){
        list->names[list->length] = names[i];
        list->types[list->length++] = type;
    }
}

const char* ast_var_list_nearest(ast_var_list_t *var_list, char* name){
    length_t list_length = var_list->length;
    int distances[list_length];

    for(length_t i = 0; i != list_length; i++){
        distances[i] = levenshtein(name, var_list->names[i]);
    }

    const char *nearest = NULL;
    length_t minimum = 3; // Minimum number of changes to override NULL

    for(length_t i = 0; i != list_length; i++){
        if(distances[i] < minimum){
            minimum = distances[i];
            nearest = var_list->names[i];
        }
    }

    return nearest;
}
