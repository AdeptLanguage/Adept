
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast_type.h"
#include "IR/ir_func_endpoint.h"
#include "IRGEN/ir_gen_vtree.h"
#include "UTIL/ground.h"

void vtree_list_free(vtree_list_t *vtree_list){
    for(length_t i = 0; i < vtree_list->length; i++){
        free(vtree_list->vtrees[i]);
    }
    free(vtree_list->vtrees);
}

vtree_t *vtree_list_find_or_insert(vtree_list_t *vtree_list, const ast_type_t *signature){
    for(length_t i = 0; i != vtree_list->length; i++){
        if(ast_types_identical(&vtree_list->vtrees[i]->signature, signature)){
            return vtree_list->vtrees[i];
        }
    }

    vtree_t *new_vtree = malloc(sizeof(vtree_t));
    
    *new_vtree = (vtree_t){
        .signature = ast_type_clone(signature),
        .parent = NULL,
        .virtuals = (ir_func_endpoint_list_t){0},
        .overrides = (ir_func_endpoint_list_t){0},
    };

    vtree_list_append(vtree_list, new_vtree);
    return new_vtree;
}

void vtree_append_virtual(vtree_t *vtree, ir_func_endpoint_t endpoint){
    ir_func_endpoint_list_insert(&vtree->virtuals, endpoint);
}
