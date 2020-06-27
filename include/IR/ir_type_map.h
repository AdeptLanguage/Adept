
#ifndef IR_TYPE_MAP_H
#define IR_TYPE_MAP_H

#include "IR/ir_type.h"
#include "UTIL/ground.h"

// ---------------- ir_type_mapping_t ----------------
// Mapping for a name to an IR type
typedef struct {
    weak_cstr_t name;
    ir_type_t type;
} ir_type_mapping_t;

// ---------------- ir_type_map_t ----------------
// A list of mappings from names to IR types
typedef struct {
    ir_type_mapping_t *mappings;
    length_t mappings_length;
} ir_type_map_t;

// ---------------- ir_type_map_find ----------------
// Finds a type inside an IR type map by name
// NOTE: 'pool' can be NULL to get the underlying type instead of
//       an indexed version
successful_t ir_type_map_find(ir_pool_t *pool, ir_type_map_t *type_map, char *name, ir_type_t **type_ptr);

// ---------------- ir_type_map_find ----------------
// Finds a type index inside an IR type map by name
successful_t ir_type_map_find_index(ir_type_map_t *type_map, char *name, ir_type_index_t *out_index);

// ---------------- ir_type_unindex_t ----------------
// Returns an IR type so that the first layer is not indexed
// NOTE: This is used when for example we want to do "my_ir_type->kind"
// NOTE: This function assumes that any type indices are correct,
// and therefore will never return NULL
ir_type_t *ir_type_unindex(ir_type_map_t *type_map, ir_type_t *type);

// ---------------- ir_type_kind ----------------
// Returns the correct type->kind regardless of whether the type is indexed
unsigned int ir_type_kind(ir_type_map_t *type_map, ir_type_t *type);

// ---------------- ir_type_extra ----------------
// Returns the correct type->extra regardless of whether the type is indexed
void *ir_type_extra(ir_type_map_t *type_map, ir_type_t *type);

// ---------------- ir_types_identical ----------------
// Returns whether two IR types are identical
bool ir_types_identical(ir_type_map_t *type_map, ir_type_t *a, ir_type_t *b);

// ---------------- ir_type_dereference ----------------
// Gets the type pointed to by a pointer type
ir_type_t* ir_type_dereference(ir_type_map_t *type_map, ir_type_t *type);

// ---------------- ir_type_str ----------------
// Generates a c-string representation from
// an intermediate representation type
strong_cstr_t ir_type_str(ir_type_map_t *type_map, ir_type_t *type);

#endif // IR_TYPE_MAP_H
