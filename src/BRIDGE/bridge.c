
#include <stdbool.h>
#include <stdlib.h>

#include "BRIDGE/bridge.h"
#include "UTIL/ground.h"
#include "UTIL/levenshtein.h"

void bridge_scope_init(bridge_scope_t *out_scope, bridge_scope_t *parent){
    *out_scope = (bridge_scope_t){
        .parent = parent,
        .list = (bridge_var_list_t){0},
        .first_var_id = 0,
        .following_var_id = 0,
        .children = (bridge_scope_ref_list_t){0}
    };
}

void bridge_scope_free(bridge_scope_t *scope){
    for(length_t i = 0; i != scope->children.length; i++){
        bridge_scope_t *child = scope->children.scopes[i];
        bridge_scope_free(child);
        free(child);
    }

    free(scope->list.variables);
    free(scope->children.scopes);

}
bridge_var_t* bridge_scope_find_var(bridge_scope_t *scope, const char *name){
    for(length_t i = 0; i != scope->list.length; i++){
        if(streq(scope->list.variables[i].name, name)){
            return &scope->list.variables[i];
        }
    }

    if(scope->parent){
        return bridge_scope_find_var(scope->parent, name);
    } else {
        return NULL;
    }
}

bridge_var_t* bridge_scope_find_var_by_id(bridge_scope_t *scope, length_t id){
    length_t starting_id = scope->first_var_id;
    length_t ending_id = scope->following_var_id;
    length_t count = scope->list.length;

    if(id >= starting_id && id < ending_id){
        for(length_t i = 0; i != count; i++){
            if(scope->list.variables[i].id == id){
                return &scope->list.variables[i];
            }
        }
    }

    if(id < ending_id){
        for(length_t i = 0; i != scope->children.length; i++){
            bridge_var_t *var = bridge_scope_find_var_by_id(scope->children.scopes[i], id);
            if(var) return var;
        }
    }

    return NULL;
}

bool bridge_scope_var_already_in_list(bridge_scope_t *scope, const char *name){
    for(length_t i = 0; i != scope->list.length; i++){
        if(streq(scope->list.variables[i].name, name)) return true;
    }
    return false;
}

const char* bridge_scope_var_nearest(bridge_scope_t *scope, const char *name){
    char *scope_name = NULL, *parent_name = NULL;
    int scope_distance = -1, parent_distance = -1;

    bridge_var_list_nearest(&scope->list, name, &scope_name, &scope_distance);

    if(scope->parent == NULL){
        return scope_name;
    }

    bridge_var_list_nearest(&scope->parent->list, name, &parent_name, &parent_distance);

    if(scope_distance <= parent_distance){
        return scope_name;
    } else {
        return parent_name;
    }
}

void bridge_var_list_nearest(bridge_var_list_t *list, const char *name, char **out_nearest_name, int *out_distance){
    // Default to no nearest value
    char *found_nearest_name = NULL;

    // Stack array to contain all of the distances
    // NOTE: This may be bad if the length of the variable list is really long
    length_t list_length = list->length;
    int distances[list_length];

    // Calculate distance for every variable name
    for(length_t i = 0; i != list_length; i++){
        distances[i] = levenshtein(name, list->variables[i].name);
    }

    // Minimum number of changes to override NULL
    int minimum = 3;

    // Find the name with the shortest distance
    for(length_t i = 0; i != list_length; i++){
        if(distances[i] < minimum){
            minimum = distances[i];
            found_nearest_name = list->variables[i].name;
        }
    }

    // Output minimum distance if a name close enough was found
    if(found_nearest_name){
        *out_nearest_name = found_nearest_name;
        *out_distance = minimum;
    } else {
        *out_nearest_name = NULL;
        *out_distance = -1;
    }
}
