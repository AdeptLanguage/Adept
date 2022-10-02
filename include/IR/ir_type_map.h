
#ifndef _ISAAC_IR_TYPE_MAP_H
#define _ISAAC_IR_TYPE_MAP_H

/*
    =============================== ir_type_map.h ===============================
    Module for intermediate representation type maps
    -----------------------------------------------------------------------------
*/

#include "IR/ir_type.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"

// ---------------- ir_type_mapping_t ----------------
// Mapping for a name to an IR type
typedef struct {
    weak_cstr_t name;
    ir_type_t *type;
} ir_type_mapping_t;

#define ir_type_mapping_create(NAME, TYPE) ((ir_type_mapping_t){.name = (NAME), .type = (TYPE)})

// ---------------- ir_type_map_t ----------------
// A list of mappings from names to IR types
typedef listof(ir_type_mapping_t, mappings) ir_type_map_t;

// ---------------- ir_type_map_append ----------------
// Appends a mapping to a type map
#define ir_type_map_append(LIST, VALUE) list_append(LIST, VALUE, ir_type_mapping_t)

// ---------------- ir_type_map_free ----------------
// Frees the data of a type map
void ir_type_map_free(ir_type_map_t *type_map);

// ---------------- ir_type_map_find ----------------
// Finds a type inside an IR type map by name
successful_t ir_type_map_find(ir_type_map_t *type_map, const char *name, ir_type_t **type_ptr);

#endif // _ISAAC_IR_TYPE_MAP_H
