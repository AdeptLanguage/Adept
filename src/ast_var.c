
#include "util.h"
#include "ast_var.h"

void ast_var_list_add_variables(ast_var_list_t *list, const char **names, ast_type_t *type, length_t length){
    coexpand((void**) &list->names, sizeof(const char*), (void**) &list->types,
        sizeof(ast_type_t*), list->length, &list->capacity, length, 4);

    for(length_t i = 0; i != length; i++){
        list->names[list->length] = names[i];
        list->types[list->length++] = type;
    }
}
