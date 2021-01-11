
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "AST/ast_type.h"
#include "AST/ast_layout.h"

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

ast_layout_bone_t ast_layout_as_bone(ast_layout_t *layout){
    ast_layout_bone_t bone;
    bone.kind = layout->kind == AST_LAYOUT_STRUCT ? AST_LAYOUT_BONE_KIND_STRUCT : AST_LAYOUT_BONE_KIND_UNION;
    bone.traits = (layout->traits & AST_LAYOUT_PACKED) ? AST_LAYOUT_BONE_PACKED : TRAIT_NONE;
    bone.children = layout->skeleton;
    return bone;
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
        internalerrorprintf("ast_layout_get_path() got unknown layout type\n");
        return false;
    }

    for(length_t i = 0; i < AST_LAYOUT_MAX_DEPTH && endpoint.indices[i] != AST_LAYOUT_WAYPOINT_END; i++){
        length_t bone_index = endpoint.indices[i];

        if(bone_index >= skeleton->bones_length){
            internalerrorprintf("ast_layout_skeleton_get_path() got bad endpoint - bone out of bounds\n");
            return false;
        }

        ast_layout_bone_t *bone = &skeleton->bones[bone_index];

        switch(bone->kind){
        case AST_LAYOUT_BONE_KIND_TYPE:
            // Assumes the end of the endpoint has been reached
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

    // Bad endpoint
    internalerrorprintf("ast_layout_skeleton_get_path() got bad endpoint - endpoint is incomplete\n");
    return false;
}

const char *ast_layout_kind_name(ast_layout_kind_t kind){
    switch(kind){
    case AST_LAYOUT_UNION: return "union";
    case AST_LAYOUT_STRUCT: return "struct";
    default: internalerrorprintf("ast_layout_kind_name() got unknown layout kind\n");
    }
    return "unkcomposite";
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

void ast_layout_skeleton_print(ast_layout_skeleton_t *skeleton, int indentation){
    for(length_t i = 0; i != skeleton->bones_length; i++){
        ast_layout_bone_print(&skeleton->bones[i], indentation);
    }
}

void ast_layout_bone_print(ast_layout_bone_t *bone, int indentation){
    indent(stdout, indentation);

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE: {
            char *s = ast_type_str(&bone->type);
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
        internalerrorprintf("ast_layout_skeleton_get_type_at_index() found non-AST-type at given index\n");
        return NULL;
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

        if(strcmp(arrow->name, name) == 0){
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
