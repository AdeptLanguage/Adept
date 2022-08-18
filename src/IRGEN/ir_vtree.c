
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast_type.h"
#include "IR/ir_func_endpoint.h"
#include "IRGEN/ir_vtree.h"
#include "UTIL/ground.h"

void vtree_list_free(vtree_list_t *vtree_list){
    for(length_t i = 0; i < vtree_list->length; i++){
        vtree_free_fully(vtree_list->vtrees[i]);
    }
    free(vtree_list->vtrees);
}

vtree_t *vtree_list_find_or_append(vtree_list_t *vtree_list, const ast_type_t *signature){
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
        .table = (ir_func_endpoint_list_t){0},
        .children = (vtree_list_t){0},
        .finalized_table = NULL,
    };

    vtree_list_append(vtree_list, new_vtree);
    return new_vtree;
}

vtree_t *vtree_list_find(vtree_list_t *vtree_list, const ast_type_t *signature){
    for(length_t i = 0; i != vtree_list->length; i++){
        if(ast_types_identical(&vtree_list->vtrees[i]->signature, signature)){
            return vtree_list->vtrees[i];
        }
    }
    return NULL;
}

void vtree_free_fully(vtree_t *vtree){
    // Free array of children
    free(vtree->children.vtrees);

    ast_type_free(&vtree->signature);
    ir_func_endpoint_list_free(&vtree->virtuals);
    ir_func_endpoint_list_free(&vtree->table);
    free(vtree);
}

void vtree_append_virtual(vtree_t *vtree, ir_func_endpoint_t endpoint){
    ir_func_endpoint_list_insert(&vtree->virtuals, endpoint);
}

void vtree_print(vtree_t *root, length_t indentation){
    for(length_t i = 0; i != indentation; i++){
        printf(i + 1 == indentation ? " -> " : "    ");
    }

    strong_cstr_t s = ast_type_str(&root->signature);
    printf("%s\n", s);
    free(s);

    for(length_t i = 0; i != root->virtuals.length; i++){
        for(length_t i = 0; i != indentation; i++){
            printf("    ");
        }
        printf("(virtual) %d\n", (int) root->virtuals.endpoints[i].ast_func_id);
    }

    for(length_t i = 0; i != root->table.length; i++){
        for(length_t i = 0; i != indentation; i++){
            printf("    ");
        }
        printf("(override) %d\n", (int) root->table.endpoints[i].ast_func_id);
    }

    for(length_t i = 0; i != root->children.length; i++){
        vtree_print(root->children.vtrees[i], indentation + 1);
    }
}
