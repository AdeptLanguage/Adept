
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_hash.h"
#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

void ast_layout_init(ast_layout_t *layout, ast_layout_kind_t kind, ast_field_map_t field_map, ast_layout_skeleton_t skeleton, trait_t traits){
    layout->kind = kind;
    layout->field_map = field_map;
    layout->skeleton = skeleton;
    layout->traits = traits;
}

void ast_layout_init_with_struct_fields(ast_layout_t *layout, strong_cstr_t names[], ast_type_t strong_types[], length_t length){
    assert(length < 0xFFFF);

    ast_field_map_t field_map;
    ast_layout_skeleton_t skeleton;

    ast_layout_skeleton_init(&skeleton);

    ast_field_map_init(&field_map);
    for(uint16_t index = 0; index != (uint16_t) length; index++){
        ast_layout_endpoint_t endpoint;
        ast_layout_endpoint_init_with(&endpoint, &index, 1);

        ast_field_map_add(&field_map, names[index], endpoint);
        ast_layout_skeleton_add_type(&skeleton, strong_types[index]);
    }

    ast_layout_init(layout, AST_LAYOUT_STRUCT, field_map, skeleton, TRAIT_NONE);
}

void ast_layout_free(ast_layout_t *layout){
    ast_layout_skeleton_free(&layout->skeleton);
    ast_field_map_free(&layout->field_map);
}

ast_layout_t ast_layout_clone(const ast_layout_t *layout){
    return (ast_layout_t){
        .kind = layout->kind,
        .field_map = ast_field_map_clone(&layout->field_map),
        .skeleton = ast_layout_skeleton_clone(&layout->skeleton),
        .traits = layout->traits,
    };;
}

strong_cstr_t ast_layout_str(ast_layout_t *layout, ast_field_map_t *field_map){
    ast_layout_bone_t as_bone = ast_layout_as_bone(layout);
    ast_layout_endpoint_t endpoint;
    ast_layout_endpoint_init(&endpoint);

    return ast_layout_bone_str(&as_bone, field_map, endpoint);
}

bool ast_layouts_identical(ast_layout_t *layout_a, ast_layout_t *layout_b){
    if(layout_a->kind != layout_b->kind) return false;
    if(layout_a->traits != layout_b->traits) return false;
    if(!ast_field_maps_identical(&layout_a->field_map, &layout_b->field_map)) return false;
    if(!ast_layout_skeletons_identical(&layout_a->skeleton, &layout_b->skeleton)) return false;
    return true;
}

ast_layout_bone_t ast_layout_as_bone(ast_layout_t *layout){
    return (ast_layout_bone_t){
        .kind = (layout->kind == AST_LAYOUT_STRUCT) ? AST_LAYOUT_BONE_KIND_STRUCT : AST_LAYOUT_BONE_KIND_UNION,
        .traits = (layout->traits & AST_LAYOUT_PACKED) ? AST_LAYOUT_BONE_PACKED : TRAIT_NONE,
        .children = layout->skeleton,
    };
}

successful_t ast_layout_get_path(ast_layout_t *layout, ast_layout_endpoint_t endpoint, ast_layout_endpoint_path_t *out_path){
    ast_layout_skeleton_t *skeleton = &layout->skeleton;

    switch(layout->kind){
    case AST_LAYOUT_UNION:
        out_path->waypoints[0].kind = AST_LAYOUT_WAYPOINT_BITCAST;
        break;
    case AST_LAYOUT_STRUCT:
        out_path->waypoints[0].kind = AST_LAYOUT_WAYPOINT_OFFSET;
        out_path->waypoints[0].index = endpoint.indices[0];
        break;
    default:
        die("ast_layout_get_path() - Unrecognized layout type %d\n", (int) layout->kind);
    }

    for(length_t i = 0; i < AST_LAYOUT_MAX_DEPTH && endpoint.indices[i] != AST_LAYOUT_ENDPOINT_END_INDEX; i++){
        length_t bone_index = endpoint.indices[i];

        if(bone_index >= skeleton->bones_length){
            die("ast_layout_skeleton_get_path() - The requested bone is out of bounds\n");
        }

        ast_layout_bone_t *bone = &skeleton->bones[bone_index];

        switch(bone->kind){
        case AST_LAYOUT_BONE_KIND_TYPE:
            // Assumes the end of the endpoint has been reached
            if(i + 1 < AST_LAYOUT_MAX_DEPTH){
                out_path->waypoints[i + 1].kind = AST_LAYOUT_WAYPOINT_END;
            }
            return true;
        case AST_LAYOUT_BONE_KIND_UNION:
            if(i + 1 >= AST_LAYOUT_MAX_DEPTH) break;
            
            skeleton = &bone->children;
            out_path->waypoints[i + 1].kind = AST_LAYOUT_WAYPOINT_BITCAST;
            break;
        case AST_LAYOUT_BONE_KIND_STRUCT:
            if(i + 1 >= AST_LAYOUT_MAX_DEPTH) break;

            skeleton = &bone->children;
            out_path->waypoints[i + 1].kind = AST_LAYOUT_WAYPOINT_OFFSET;
            out_path->waypoints[i + 1].index = endpoint.indices[i + 1]; 
            break;
        }
    }

    die("ast_layout_skeleton_get_path() - Incomplete endpoint\n");
    return false;
}

const char *ast_layout_kind_name(ast_layout_kind_t kind){
    switch(kind){
    case AST_LAYOUT_UNION:  return "union";
    case AST_LAYOUT_STRUCT: return "struct";
    }

    die("ast_layout_kind_name() got unknown layout kind\n");
}

bool ast_layout_is_simple_struct(ast_layout_t *layout){
    return layout->kind == AST_LAYOUT_STRUCT && layout->field_map.is_simple;
}

bool ast_layout_is_simple_union(ast_layout_t *layout){
    ast_field_map_t *field_map = &layout->field_map;

    if(layout->kind != AST_LAYOUT_UNION) return false;

    for(length_t i = 0; i != field_map->arrows_length; i++){
        // The endpoint is assumed to not have a length of zero
        // The endpoint must have a length of one in order for the layout to be simple
        if(field_map->arrows[i].endpoint.indices[1] != AST_LAYOUT_ENDPOINT_END_INDEX) return false;
    }

    return true;
}

hash_t ast_layout_hash(const ast_layout_t *layout){
    hash_t hash;
    hash = hash_data(&layout->kind, sizeof layout->kind);
    hash = hash_combine(hash, ast_field_map_hash(&layout->field_map));
    hash = hash_combine(hash, ast_layout_skeleton_hash(&layout->skeleton));
    hash = hash_combine(hash, hash_data(&layout->traits, sizeof layout->traits));
    return hash;
}

void ast_layout_skeleton_init(ast_layout_skeleton_t *skeleton){
    skeleton->bones = NULL;
    skeleton->bones_length = 0;
    skeleton->bones_capacity = 0;
}

void ast_layout_skeleton_free(ast_layout_skeleton_t *skeleton){
    for(length_t i = 0; i != skeleton->bones_length; i++){
        ast_layout_bone_free(&skeleton->bones[i]);
    }

    free(skeleton->bones);
}

ast_layout_skeleton_t ast_layout_skeleton_clone(const ast_layout_skeleton_t *skeleton){
    ast_layout_skeleton_t clone;
    clone.bones = malloc(sizeof(ast_layout_bone_t) * skeleton->bones_length);
    clone.bones_length = skeleton->bones_length;
    clone.bones_capacity = skeleton->bones_length; // (on purpose)

    for(length_t i = 0; i != skeleton->bones_length; i++){
        clone.bones[i] = ast_layout_bone_clone(&skeleton->bones[i]);
    }

    return clone;
}

bool ast_layout_skeleton_has_polymorph(ast_layout_skeleton_t *skeleton){
    for(length_t i = 0; i != skeleton->bones_length; i++){
        if(ast_layout_bone_has_polymorph(&skeleton->bones[i])) return true;
    }
    return false;
}

strong_cstr_t ast_layout_skeleton_str(ast_layout_skeleton_t *skeleton, ast_field_map_t *field_map, ast_layout_endpoint_t root_endpoint){
    string_builder_t builder;
    string_builder_init(&builder);

    string_builder_append_char(&builder, '(');

    for(length_t i = 0; i != skeleton->bones_length; i++){
        ast_layout_bone_t *bone = &skeleton->bones[i];

        ast_layout_endpoint_t bone_endpoint = root_endpoint;
        ast_layout_endpoint_add_index(&bone_endpoint, i);

        strong_cstr_t bone_str = ast_layout_bone_str(bone, field_map, bone_endpoint);
        string_builder_append(&builder, bone_str);
        free(bone_str);

        if(i + 1 != skeleton->bones_length){
            string_builder_append(&builder, ", ");
        }
    }

    string_builder_append_char(&builder, ')');
    return string_builder_finalize(&builder);
}

bool ast_layout_skeletons_identical(ast_layout_skeleton_t *skeleton_a, ast_layout_skeleton_t *skeleton_b){
    if(skeleton_a->bones_length != skeleton_b->bones_length) return false;

    for(length_t i = 0; i != skeleton_a->bones_length; i++){
        if(!ast_layout_bones_identical(&skeleton_a->bones[i], &skeleton_b->bones[i])) return false;
    }

    return true;
}

void ast_layout_skeleton_print(ast_layout_skeleton_t *skeleton, int indentation){
    for(length_t i = 0; i != skeleton->bones_length; i++){
        ast_layout_bone_print(&skeleton->bones[i], indentation);
    }
}

hash_t ast_layout_skeleton_hash(const ast_layout_skeleton_t *skeleton){
    hash_t hash = 0;

    for(length_t i = 0; i < skeleton->bones_length; i++){
        ast_layout_bone_t *bone = &skeleton->bones[i];
        hash = hash_combine(hash, ast_layout_bone_hash(bone));
    }

    return hash;
}

void ast_layout_bone_print(ast_layout_bone_t *bone, int indentation){
    indent(stdout, indentation);

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE: {
            strong_cstr_t s = ast_type_str(&bone->type);
            printf("%s\n", s);
            free(s);
        }
        return;
    case AST_LAYOUT_BONE_KIND_UNION:
        printf("union:\n");
        break;
    case AST_LAYOUT_BONE_KIND_STRUCT:
        printf("struct:\n");
        break;
    }

    ast_layout_skeleton_print(&bone->children, indentation + 1);
}

void ast_layout_bone_free(ast_layout_bone_t *bone){
    if(bone->kind == AST_LAYOUT_BONE_KIND_TYPE){
        ast_type_free(&bone->type);
    } else {
        ast_layout_skeleton_free(&bone->children);
    }
}

ast_layout_bone_t ast_layout_bone_clone(ast_layout_bone_t *bone){
    ast_layout_bone_t clone;
    clone.kind = bone->kind;
    clone.traits = bone->traits;

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE:
        clone.type = ast_type_clone(&bone->type);
        break;
    case AST_LAYOUT_BONE_KIND_UNION:
    case AST_LAYOUT_BONE_KIND_STRUCT:
        clone.children = ast_layout_skeleton_clone(&bone->children);
        break;
    default:
        die("ast_layout_bone_clone() - Unrecognized bone kind %d\n", (int) bone->kind);
    }

    return clone;
}

bool ast_layout_bone_has_polymorph(ast_layout_bone_t *bone){
    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE:
        return ast_type_has_polymorph(&bone->type);
    case AST_LAYOUT_BONE_KIND_UNION:
    case AST_LAYOUT_BONE_KIND_STRUCT:
        return ast_layout_skeleton_has_polymorph(&bone->children);
    default:
        die("ast_layout_bone_has_polymorph() - Unrecognized bone kind %d\n", (int) bone->kind);
    }

    return false;
}

strong_cstr_t ast_layout_bone_str(ast_layout_bone_t *bone, ast_field_map_t *field_map, ast_layout_endpoint_t endpoint){
    string_builder_t builder;
    string_builder_init(&builder);

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE: {
            maybe_null_weak_cstr_t name = ast_field_map_get_name_of_endpoint(field_map, endpoint);

            if(name){
                string_builder_append(&builder, name);
                string_builder_append_char(&builder, ' ');

                strong_cstr_t type_str = ast_type_str(&bone->type);
                string_builder_append(&builder, type_str);
                free(type_str);
            } else {
                die("ast_layout_bone_str() - Failed to find name for endpoint\n");
            }
        }
        break;
    case AST_LAYOUT_BONE_KIND_UNION: {
            string_builder_append(&builder, "union ");

            strong_cstr_t skeleton_str = ast_layout_skeleton_str(&bone->children, field_map, endpoint);
            string_builder_append(&builder, skeleton_str);
            free(skeleton_str);
        }
        break;
    case AST_LAYOUT_BONE_KIND_STRUCT: {
            string_builder_append(&builder, "struct ");

            strong_cstr_t skeleton_str = ast_layout_skeleton_str(&bone->children, field_map, endpoint);
            string_builder_append(&builder, skeleton_str);
            free(skeleton_str);
        }
        break;
    default:
        die("ast_layout_str() - Unrecognized layout kind %d\n", (int) bone->kind);
    }

    return string_builder_finalize(&builder);
}

hash_t ast_layout_bone_hash(const ast_layout_bone_t *bone){
    hash_t hash = hash_data(&bone->kind, sizeof bone->kind);
    hash = hash_combine(hash, hash_data(&bone->traits, sizeof bone->traits));

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE:
        hash = hash_combine(hash, ast_type_hash(&bone->type));
        break;
    case AST_LAYOUT_BONE_KIND_STRUCT:
    case AST_LAYOUT_BONE_KIND_UNION:
        for(length_t i = 0; i < bone->children.bones_length; i++){
            hash = hash_combine(hash, ast_layout_bone_hash(&bone->children.bones[i]));
        }
        break;
    default:
        die("ast_layout_bone_hash() got unrecognized layout bone kind\n");
    }
    
    return hash;
}

bool ast_layout_bones_identical(ast_layout_bone_t *bone_a, ast_layout_bone_t *bone_b){
    if(bone_a->kind != bone_b->kind) return false;

    switch(bone_a->kind){
    case AST_LAYOUT_BONE_KIND_TYPE:
        return ast_types_identical(&bone_a->type, &bone_b->type);
    case AST_LAYOUT_BONE_KIND_UNION:
    case AST_LAYOUT_BONE_KIND_STRUCT:
        return ast_layout_skeletons_identical(&bone_a->children, &bone_b->children);
    default:
        die("ast_layout_bones_identical() - Unrecognized bone kind %d\n", (int) bone_a->kind);
    }

    return true;
}

void ast_layout_endpoint_init(ast_layout_endpoint_t *endpoint){
    endpoint->indices[0] = AST_LAYOUT_ENDPOINT_END_INDEX;
}

successful_t ast_layout_endpoint_init_with(ast_layout_endpoint_t *endpoint, uint16_t *indices, length_t length){
    if(length > AST_LAYOUT_MAX_DEPTH) return false;

    // Set end index, so we can determine the length of this endpoint's indices
    if(length != AST_LAYOUT_MAX_DEPTH) endpoint->indices[length] = AST_LAYOUT_ENDPOINT_END_INDEX;

    memcpy(endpoint->indices, indices, sizeof(uint16_t) * length);
    return true;
}

successful_t ast_layout_endpoint_add_index(ast_layout_endpoint_t *endpoint, uint16_t index){
    length_t length = 0;

    // Calculate indices length
    while(length < AST_LAYOUT_MAX_DEPTH && endpoint->indices[length] != AST_LAYOUT_ENDPOINT_END_INDEX) length++;

    if(length + 1 > AST_LAYOUT_MAX_DEPTH) return false;

    endpoint->indices[length] = index;

    // Set end index, so we can determine the length of this endpoint's indices
    if(length != AST_LAYOUT_MAX_DEPTH) endpoint->indices[length + 1] = AST_LAYOUT_ENDPOINT_END_INDEX;

    return true;
}

bool ast_layout_endpoint_equals(ast_layout_endpoint_t *a, ast_layout_endpoint_t *b){
    for(length_t i = 0; i != AST_LAYOUT_MAX_DEPTH; i++){
        if(a->indices[i] != b->indices[i]) return false;
        if(a->indices[i] == AST_LAYOUT_ENDPOINT_END_INDEX) break;
    }

    return true;
}

void ast_layout_endpoint_increment(ast_layout_endpoint_t *inout_endpoint){
    length_t length = 0;

    // Compute length
    for(length = 0; length < AST_LAYOUT_MAX_DEPTH && inout_endpoint->indices[length] != AST_LAYOUT_ENDPOINT_END_INDEX; length++);

    // Increment the last index in the endpoint
    if(length != 0){
        inout_endpoint->indices[length - 1]++;
    }
}

void ast_layout_endpoint_print(ast_layout_endpoint_t endpoint){
    for(length_t i = 0; i != AST_LAYOUT_MAX_DEPTH; i++)
        printf("%04X ", (int) endpoint.indices[i]);
    printf("\n");
}

void ast_layout_skeleton_add_type(ast_layout_skeleton_t *skeleton, ast_type_t strong_type){
    expand((void**) &skeleton->bones, sizeof(ast_layout_bone_t), skeleton->bones_length, &skeleton->bones_capacity, 1, 4);    

    ast_layout_bone_t bone;
    bone.kind = AST_LAYOUT_BONE_KIND_TYPE;
    bone.type = strong_type;
    bone.traits = TRAIT_NONE;

    skeleton->bones[skeleton->bones_length++] = bone;
}

ast_layout_skeleton_t *ast_layout_skeleton_add_union(ast_layout_skeleton_t *skeleton, trait_t bone_traits){
    return ast_layout_skeleton_add_child_skeleton(skeleton, AST_LAYOUT_BONE_KIND_UNION, bone_traits);
}

ast_layout_skeleton_t *ast_layout_skeleton_add_struct(ast_layout_skeleton_t *skeleton, trait_t bone_traits){
    return ast_layout_skeleton_add_child_skeleton(skeleton, AST_LAYOUT_BONE_KIND_STRUCT, bone_traits);
}

ast_layout_skeleton_t *ast_layout_skeleton_add_child_skeleton(ast_layout_skeleton_t *skeleton, ast_layout_bone_kind_t bone_kind, trait_t bone_traits){
    expand((void**) &skeleton->bones, sizeof(ast_layout_bone_t), skeleton->bones_length, &skeleton->bones_capacity, 1, 4);

    ast_layout_bone_t *bone = &skeleton->bones[skeleton->bones_length++];
    bone->kind = bone_kind;
    bone->traits = bone_traits;
    ast_layout_skeleton_init(&bone->children);
    
    return &bone->children;
}

ast_type_t *ast_layout_skeleton_get_type(ast_layout_skeleton_t *skeleton, ast_layout_endpoint_t endpoint){
    // Returns NULL on invalid endpoint
    // Otherwise returns weak pointer to AST type
    // The returned pointer is only valid until the layout skeleton is modified

    for(length_t e_idx = 0; e_idx < AST_LAYOUT_MAX_DEPTH && endpoint.indices[e_idx] != AST_LAYOUT_ENDPOINT_END_INDEX; e_idx++){
        uint16_t index = endpoint.indices[e_idx];

        // The current skeleton doesn't have a field at index 'index'
        if(index >= skeleton->bones_length) return NULL;

        ast_layout_bone_t *bone = &skeleton->bones[index];

        if(bone->kind == AST_LAYOUT_BONE_KIND_TYPE){
            return &bone->type;
        } else {
            skeleton = &bone->children;
        }
    }

    return NULL; // Bad endpoint
}

ast_type_t *ast_layout_skeleton_get_type_at_index(ast_layout_skeleton_t *skeleton, length_t index){
    // Assumes the layout uses a simple AST field map

    assert(index < skeleton->bones_length);

    ast_layout_bone_t *bone = &skeleton->bones[index];

    if(bone->kind != AST_LAYOUT_BONE_KIND_TYPE){
        die("ast_layout_skeleton_get_type_at_index() - Bone at index is not a type\n");
    }

    return &bone->type;
}

void ast_field_map_init(ast_field_map_t *field_map){
    field_map->arrows = NULL;
    field_map->arrows_length = 0;
    field_map->arrows_capacity = 0;
    field_map->is_simple = true;
}

void ast_field_map_free(ast_field_map_t *field_map){
    for(length_t i = 0; i != field_map->arrows_length; i++){
        free(field_map->arrows[i].name);
    }
    free(field_map->arrows);
}

ast_field_map_t ast_field_map_clone(const ast_field_map_t *field_map){
    ast_field_map_t clone;
    clone.arrows = malloc(sizeof(ast_field_arrow_t) * field_map->arrows_length);
    clone.arrows_length = field_map->arrows_length;
    clone.arrows_capacity = field_map->arrows_length; // (on purpose)
    clone.is_simple = field_map->is_simple;

    for(length_t i = 0; i != field_map->arrows_length; i++){
        ast_field_arrow_t *clone_arrow = &clone.arrows[i];
        ast_field_arrow_t *original_arrow = &field_map->arrows[i];

        clone_arrow->name = strclone(original_arrow->name);
        clone_arrow->endpoint = original_arrow->endpoint;
    }

    return clone;
}

bool ast_field_maps_identical(ast_field_map_t *field_map_a, ast_field_map_t *field_map_b){
    if(field_map_a->is_simple != field_map_b->is_simple) return false;
    if(field_map_a->arrows_length != field_map_b->arrows_length) return false;

    for(length_t i = 0; i != field_map_a->arrows_length; i++){
        ast_field_arrow_t *arrow_a = &field_map_a->arrows[i];
        ast_field_arrow_t *arrow_b = &field_map_b->arrows[i];

        if(!ast_layout_endpoint_equals(&arrow_a->endpoint, &arrow_b->endpoint)) return false;
        if(!streq(arrow_a->name, arrow_b->name)) return false;
    }

    return true;
}

void ast_field_map_add(ast_field_map_t *field_map, strong_cstr_t name, ast_layout_endpoint_t endpoint){
    expand((void**) &field_map->arrows, sizeof(ast_field_arrow_t), field_map->arrows_length, &field_map->arrows_capacity, 1, 4);

    // Compute whether the new arrow will make this AST field map "not simple"
    // In order to be simple, the layout must have each successive arrow point to
    // the next endpoint that is one level deep.
    // For example, the first endpoint must be [0x0], the second must be [0x1], etc.
    // Otherwise the field map is considered "not simple"

    if(field_map->is_simple){
        if(endpoint.indices[0] != field_map->arrows_length || endpoint.indices[1] != AST_LAYOUT_ENDPOINT_END_INDEX){
            field_map->is_simple = false;
        }
    }

    ast_field_arrow_t *arrow = &field_map->arrows[field_map->arrows_length++];
    arrow->name = name;
    arrow->endpoint = endpoint;
}

successful_t ast_field_map_find(ast_field_map_t *field_map, const char *name, ast_layout_endpoint_t *out_endpoint){
    for(length_t i = 0; i != field_map->arrows_length; i++){
        ast_field_arrow_t *arrow = &field_map->arrows[i];

        if(streq(arrow->name, name)){
            *out_endpoint = arrow->endpoint;
            return true;
        }
    }

    return false;
}

maybe_null_weak_cstr_t ast_field_map_get_name_of_endpoint(ast_field_map_t *field_map, ast_layout_endpoint_t endpoint){
    // Returns NULL if no name exists for an endpoint

    for(length_t i = 0; i != field_map->arrows_length; i++){
        ast_field_arrow_t *arrow = &field_map->arrows[i];
        
        if(ast_layout_endpoint_equals(&arrow->endpoint, &endpoint)){
            return arrow->name;
        }
    }

    return NULL;
}

void ast_field_map_print(ast_field_map_t *field_map, ast_layout_skeleton_t *maybe_skeleton){
    printf("ast_field_map_t with %d arrows:\n", (int) field_map->arrows_length);
    for(length_t i = 0; i != field_map->arrows_length; i++){
        ast_field_arrow_t *arrow = &field_map->arrows[i];
        char *s = NULL;
        
        if(maybe_skeleton){
            ast_type_t *type = ast_layout_skeleton_get_type(maybe_skeleton, arrow->endpoint);
            s = ast_type_str(type);
        } else {
            s = strclone("UnknownType");
        }

        printf("%s %s\n", arrow->name, s);
        free(s);
    }
}

hash_t ast_field_map_hash(const ast_field_map_t *field_map){
    hash_t hash = 0;

    for(length_t i = 0; i < field_map->arrows_length; i++){
        ast_field_arrow_t *arrow = &field_map->arrows[i];

        hash = hash_combine(hash, hash_data(arrow->name, strlen(arrow->name)));
        hash = hash_combine(hash, hash_data(&arrow->endpoint, sizeof arrow->endpoint));
    }

    return hash;
}

length_t ast_simple_field_map_get_count(ast_field_map_t *simple_field_map){
    // NOTE: For the sake of performance, the given field is assume to either
    // be a simple struct or union

    return simple_field_map->arrows_length;
}

weak_cstr_t ast_simple_field_map_get_name_at_index(ast_field_map_t *simple_field_map, length_t index){
    // NOTE: For the sake of performance, the given field is assume to either
    // be a simple struct or union

    return simple_field_map->arrows[index].name;
}
