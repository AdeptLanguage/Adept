
#include "IR/ir_type_map.h"

bool ir_types_identical(ir_type_map_t *type_map, ir_type_t *a, ir_type_t *b){
    // NOTE: Returns true if the two types are identical
    // NOTE: The two types must match element to element to be considered identical
    // [pointer] [pointer] [u8]    [pointer] [pointer] [s32]  -> false
    // [pointer] [u8]              [pointer] [u8]             -> true

    unsigned int a_kind = ir_type_kind(type_map, a);
    unsigned int b_kind = ir_type_kind(type_map, b);

    if(a_kind != b_kind) return false;

    void *a_extra = ir_type_extra(type_map, a);
    void *b_extra = ir_type_extra(type_map, b);

    switch(a_kind){
    case TYPE_KIND_POINTER:
        return ir_types_identical(type_map, (ir_type_t*) a_extra, (ir_type_t*) b_extra);
    case TYPE_KIND_UNION:
        if(((ir_type_extra_composite_t*) a_extra)->subtypes_length != ((ir_type_extra_composite_t*) b_extra)->subtypes_length) return false;
        for(length_t i = 0; i != ((ir_type_extra_composite_t*) a_extra)->subtypes_length; i++){
            if(!ir_types_identical(type_map, ((ir_type_extra_composite_t*) a_extra)->subtypes[i], ((ir_type_extra_composite_t*) b_extra)->subtypes[i])) return false;
        }
        return true;
    case TYPE_KIND_STRUCTURE:
        if(((ir_type_extra_composite_t*) a_extra)->subtypes_length != ((ir_type_extra_composite_t*) b_extra)->subtypes_length) return false;
        for(length_t i = 0; i != ((ir_type_extra_composite_t*) a_extra)->subtypes_length; i++){
            if(!ir_types_identical(type_map, ((ir_type_extra_composite_t*) a_extra)->subtypes[i], ((ir_type_extra_composite_t*) b_extra)->subtypes[i])) return false;
        }
        return true;
    }

    return true;
}

successful_t ir_type_map_find(ir_pool_t *pool, ir_type_map_t *type_map, char *name, ir_type_t **type_ptr){
    // Does a binary search on the type map to find the requested type by name

    ir_type_index_t index;
    if(!ir_type_map_find_index(type_map, name, &index)) return false;
    
    // Get the underlying type if pool is NULL
    if(DISABLE_LLVM_TYPE_NAMING || pool == NULL){
        *type_ptr = &type_map->mappings[index].type;
        return true;
    }
    
    // Otherwise, return an indexed version
    *type_ptr = (ir_type_t*) ir_pool_alloc(pool, sizeof(ir_type_indexed_t));
    (*type_ptr)->_kind = TYPE_KIND_INDEXED;
    ((ir_type_indexed_t*) *type_ptr)->father = index;
    return true;
}

successful_t ir_type_map_find_index(ir_type_map_t *type_map, char *name, ir_type_index_t *out_index){
    // Does a binary search on the type map to find the requested mapping index by name

    ir_type_mapping_t *mappings = type_map->mappings;
    length_t mappings_length = type_map->mappings_length;
    int first = 0, middle, last = mappings_length - 1, comparison;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(mappings[middle].name, name);

        if(comparison == 0){
            *out_index = middle;
            return true;
        }
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return false;
}

ir_type_t *ir_type_unindex(ir_type_map_t *type_map, ir_type_t *type){
    if(type->_kind != TYPE_KIND_INDEXED) return type;

    ir_type_index_t father = ((ir_type_indexed_t*) type)->father;
    return &type_map->mappings[father].type;
}

unsigned int ir_type_kind(ir_type_map_t *type_map, ir_type_t *type){
    return ir_type_unindex(type_map, type)->_kind;
}

void *ir_type_extra(ir_type_map_t *type_map, ir_type_t *type){
    return ir_type_unindex(type_map, type)->_extra;
}

ir_type_t* ir_type_dereference(ir_type_map_t *type_map, ir_type_t *type){
    if(ir_type_kind(type_map, type) != TYPE_KIND_POINTER) return NULL;
    return (ir_type_t*) ir_type_extra(type_map, type);
}

strong_cstr_t ir_type_str(ir_type_map_t *type_map, ir_type_t *type){
    // NOTE: Returns allocated string of that type
    // NOTE: This function is recusive

    // Unindex the type if it's indexed
    type = ir_type_unindex(type_map, type);

    #define RET_CLONE_STR_MACRO(d, s) { \
        memory = malloc(s); memcpy(memory, d, s); return memory; \
    }

    char *memory;
    char *chained;
    length_t chained_length;

    switch(type->_kind){
    case TYPE_KIND_NONE:
        RET_CLONE_STR_MACRO("__nul_type_kind", 16);
    case TYPE_KIND_POINTER:
        chained = ir_type_str(type_map, (ir_type_t*) type->_extra);
        chained_length = strlen(chained);
        memory = malloc(chained_length + 2);
        memcpy(memory, "*", 1);
        memcpy(&memory[1], chained, chained_length + 1);
        free(chained);
        return memory;
    case TYPE_KIND_S8:      RET_CLONE_STR_MACRO("s8", 3);
    case TYPE_KIND_U8:      RET_CLONE_STR_MACRO("u8", 3);
    case TYPE_KIND_S16:     RET_CLONE_STR_MACRO("s16", 4);
    case TYPE_KIND_U16:     RET_CLONE_STR_MACRO("u16", 4);
    case TYPE_KIND_S32:     RET_CLONE_STR_MACRO("s32", 4);
    case TYPE_KIND_U32:     RET_CLONE_STR_MACRO("u32", 4);
    case TYPE_KIND_S64:     RET_CLONE_STR_MACRO("s64", 4);
    case TYPE_KIND_U64:     RET_CLONE_STR_MACRO("u64", 4);
    case TYPE_KIND_HALF:    RET_CLONE_STR_MACRO("h", 2);
    case TYPE_KIND_FLOAT:   RET_CLONE_STR_MACRO("f", 2);
    case TYPE_KIND_DOUBLE:  RET_CLONE_STR_MACRO("d", 2);
    case TYPE_KIND_BOOLEAN: RET_CLONE_STR_MACRO("bool", 5);
    case TYPE_KIND_VOID:    RET_CLONE_STR_MACRO("void", 5);
    case TYPE_KIND_FUNCPTR: RET_CLONE_STR_MACRO("__funcptr_type_kind", 20);
    case TYPE_KIND_FIXED_ARRAY: {
        ir_type_extra_fixed_array_t *fixed = (ir_type_extra_fixed_array_t*) type->_extra;
        chained = ir_type_str(type_map, fixed->subtype);
        memory = malloc(strlen(chained) + 24);
        sprintf(memory, "[%d] %s", (int) fixed->length, chained);
        free(chained);
        return memory;
    }
    case TYPE_KIND_INDEXED: RET_CLONE_STR_MACRO("__indexed", 10);
    default: RET_CLONE_STR_MACRO("__unk_type_kind", 16);
    }

    #undef RET_CLONE_STR_MACRO
}
