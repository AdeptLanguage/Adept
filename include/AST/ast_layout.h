
#ifndef _ISAAC_AST_LAYOUT_H
#define _ISAAC_AST_LAYOUT_H

/*
    ============================= ast_layout.h ==============================
    Module for abstract syntax tree field layouts, which includes structs, unions, etc.
    ---------------------------------------------------------------------------
*/

#include <stdbool.h>
#include <stdint.h>

#include "AST/ast_type_lean.h"
#include "UTIL/ground.h"
#include "UTIL/hash.h"
#include "UTIL/trait.h"

// ---------------- AST_LAYOUT_ENDPOINT_END_INDEX ----------------
// Special number used to indicate that no more indices
// exist within an 'ast_layout_endpoint_t'
// If this number isn't found with the 'ast_layout_endpoint_t',
// then the endpoint will have a length of 'AST_LAYOUT_MAX_DEPTH'
#define AST_LAYOUT_ENDPOINT_END_INDEX 0xFFFF

// ---------------- AST_LAYOUT_MAX_FIELDS ----------------
// The maximum number of fields within the root of single skeleton
#define AST_LAYOUT_MAX_FIELDS (AST_LAYOUT_ENDPOINT_END_INDEX - 1)

// ---------------- AST_LAYOUT_MAX_DEPTH ----------------
// The maximum number of nested anonymous composites
#define AST_LAYOUT_MAX_DEPTH 8

// ---------------- ast_layout_endpoint_t ----------------
// Endpoint of an AST field arrow
// Each index is used to reference a member of the previous by index
//
// For example in the type 'struct (is_float bool, union (f float, s *ubyte))':
// - The sequence [0x0, 0xFFFF] would represent the 'is_float' bool
// - The sequence [0x0, 0x0, 0xFFFF] would represent the 'f' float
// - The sequence [0x0, 0x1, 0xFFFF] would represent the 's' *ubyte
typedef struct {
    // Supports up to AST_LAYOUT_MAX_DEPTH layers of indirection
    // The first index that is AST_LAYOUT_ENDPOINT_END_INDEX will denote
    // the number of indicies. If no AST_LAYOUT_ENDPOINT_END_INDEX exists,
    // then the number of indices is AST_LAYOUT_ENDPOINT_MAX_INDICIES.
    uint16_t indices[AST_LAYOUT_MAX_DEPTH];
} ast_layout_endpoint_t;

#define AST_LAYOUT_WAYPOINT_OFFSET  0x1 // (for members inside of structures)
#define AST_LAYOUT_WAYPOINT_BITCAST 0x2 // (for members inside of unions)
#define AST_LAYOUT_WAYPOINT_END     AST_LAYOUT_ENDPOINT_END_INDEX

// ---------------- ast_layout_waypoint_t ----------------
// Specifies how to get from one index in an endpoint to the next
typedef struct {
    uint16_t kind;  // Whether AST_LAYOUT_WAYPOINT_OFFSET or AST_LAYOUT_WAYPOINT_BITCASTs
    length_t index; // Member index (only for AST_LAYOUT_WAYPOINT_OFFSET)
} ast_layout_waypoint_t;

// ---------------- ast_layout_endpoint_path_t ----------------
// Specifies how to arrive at an endpoint
typedef struct {
    // Supports up to AST_LAYOUT_MAX_DEPTH layers of indirection
    // The first index that is AST_LAYOUT_ENDPOINT_END_INDEX will denote
    // the number of waypoints. If no AST_LAYOUT_ENDPOINT_END_INDEX exists,
    // then the number of waypoints is AST_LAYOUT_ENDPOINT_MAX_INDICIES.
    // Each waypoint specifies how to access values with each index in an endpoint.
    ast_layout_waypoint_t waypoints[AST_LAYOUT_MAX_DEPTH];
} ast_layout_endpoint_path_t;

// ---------------- ast_layout_endpoint_init ----------------
// Constructs a blank endpoint
void ast_layout_endpoint_init(ast_layout_endpoint_t *endpoint);

// ---------------- ast_layout_endpoint_init_with ----------------
// Constructs an endpoint from a list of indices
// Will fail if the number of indices specified is greater than AST_LAYOUT_MAX_DEPTH
successful_t ast_layout_endpoint_init_with(ast_layout_endpoint_t *endpoint, uint16_t *indices, length_t length);

// ---------------- ast_layout_endpoint_add_index ----------------
// Appends an index to an endpoint
// Will fail if the endpoint already has AST_LAYOUT_MAX_DEPTH indicies
successful_t ast_layout_endpoint_add_index(ast_layout_endpoint_t *endpoint, uint16_t index);

// ---------------- ast_layout_endpoint_equals ----------------
// Returns whether two endpoints are functionally equivalent
bool ast_layout_endpoint_equals(ast_layout_endpoint_t *a, ast_layout_endpoint_t *b);

// ---------------- ast_layout_endpoint_increment ----------------
// Increments the last index of an endpoint
void ast_layout_endpoint_increment(ast_layout_endpoint_t *inout_endpoint);

// ---------------- ast_layout_endpoint_increment ----------------
// Prints an 'ast_layout_endpoint_t' to stdout for debugging
void ast_layout_endpoint_print(ast_layout_endpoint_t endpoint);

// ---------------- ast_field_arrow_t ----------------
// A single arrow from a field name to a location
typedef struct {
    strong_cstr_t name;             // Name of field
    ast_layout_endpoint_t endpoint; // Where the name maps to
} ast_field_arrow_t;

// ---------------- ast_field_map_t ----------------
// A collection of 'ast_field_arrow_t's that represent
// what names map to what locations.
typedef struct {
    // Mappings that convert field names to locations
    ast_field_arrow_t *arrows;
    length_t arrows_length;
    length_t arrows_capacity;

    // Whether this field map doesn't contain any overlapping fields
    bool is_simple;
} ast_field_map_t;

// ---------------- ast_field_map_init ----------------
// Constructs an empty AST field map
void ast_field_map_init(ast_field_map_t *field_map);

// ---------------- ast_layout_skeleton_t ----------------
// Represents a composite type layout, without any names attached
// For example, the type: 'struct (bool, union (float, *ubyte))'
// could be represented with this type
typedef struct ast_layout_skeleton {
    struct ast_layout_bone *bones;
    length_t bones_length;
    length_t bones_capacity;
} ast_layout_skeleton_t;

// ---------------- ast_layout_bone_kind_t ----------------
// Represents the kind of a particular 'ast_layout_bone_t'
typedef enum {
    AST_LAYOUT_BONE_KIND_TYPE,
    AST_LAYOUT_BONE_KIND_STRUCT,
    AST_LAYOUT_BONE_KIND_UNION
} ast_layout_bone_kind_t;

// ---------------- ast_layout_bone_t ----------------
// Represents one of the following:
//  - An AST Type
//  - An Anonymous Union
//  - An Anonymous Struct
typedef struct ast_layout_bone {
    ast_layout_bone_kind_t kind;
    trait_t traits;
    union {
        ast_type_t type;
        ast_layout_skeleton_t children;
    };
} ast_layout_bone_t;

#define AST_LAYOUT_BONE_PACKED TRAIT_1


// ---------------- ast_layout_kind_t ----------------
// Kind of root of composite layout
typedef enum {AST_LAYOUT_STRUCT, AST_LAYOUT_UNION} ast_layout_kind_t;

// ---------------- ast_layout_t ----------------
// Contains attached names to a layout skeleton
typedef struct ast_layout {
    ast_layout_kind_t kind;
    ast_field_map_t field_map;
    ast_layout_skeleton_t skeleton;
    trait_t traits;
} ast_layout_t;

#define AST_LAYOUT_PACKED TRAIT_1

// ---------------- ast_layout_init ----------------
// Constructs an 'ast_layout_t'
void ast_layout_init(ast_layout_t *layout, ast_layout_kind_t kind, ast_field_map_t field_map, ast_layout_skeleton_t skeleton, trait_t traits);

// ---------------- ast_layout_init_with_struct_fields ----------------
// Creates an AST layout that is a struct layout with the given field names and types.
// The primitives arrays supplied are weak, but the values within the arrays will have
// ownership taken.
void ast_layout_init_with_struct_fields(ast_layout_t *layout, strong_cstr_t names[], ast_type_t strong_types[], length_t length);

// ---------------- ast_layout_free ----------------
// Frees an 'ast_layout_t'
void ast_layout_free(ast_layout_t *layout);

// ---------------- ast_layout_clone ----------------
// Clones an 'ast_layout_t'
ast_layout_t ast_layout_clone(const ast_layout_t *layout);

// ---------------- ast_layout_str ----------------
// Converts an 'ast_layout_t' to a string
strong_cstr_t ast_layout_str(ast_layout_t *layout, ast_field_map_t *field_map);

// ---------------- ast_layouts_identical ----------------
// Returns whether two layouts are equivalent
bool ast_layouts_identical(ast_layout_t *layout_a, ast_layout_t *layout_b);

// ---------------- ast_layout_as_bone ----------------
// Returns an 'ast_layout_bone_t' which is equivalent to an 'ast_layout_t'
// (Some information is lost)
ast_layout_bone_t ast_layout_as_bone(ast_layout_t *layout);

// ---------------- ast_layout_get_path ----------------
// Gets the path to an endpoint for a given layout skeleton
successful_t ast_layout_get_path(ast_layout_t *layout, ast_layout_endpoint_t endpoint, ast_layout_endpoint_path_t *out_path);

// ---------------- ast_layout_get_path ----------------
// Returns the name of a layout kind
const char *ast_layout_kind_name(ast_layout_kind_t kind);

// ---------------- ast_layout_is_simple_struct ----------------
// Returns whether a layout is that of a simple struct type
bool ast_layout_is_simple_struct(ast_layout_t *layout);

// ---------------- ast_layout_is_simple_union ----------------
// Returns whether a layout is that of a simple union type
bool ast_layout_is_simple_union(ast_layout_t *layout);

// ---------------- ast_layout_hash ----------------
// Hashes for an AST layout
hash_t ast_layout_hash(const ast_layout_t *layout);

// ---------------- ast_layout_skeleton_init ----------------
// Constructs an empty 'ast_layout_skeleton_t'
void ast_layout_skeleton_init(ast_layout_skeleton_t *skeleton);

// ---------------- ast_layout_skeleton_free ----------------
// Frees an 'ast_layout_skeleton_t'
void ast_layout_skeleton_free(ast_layout_skeleton_t *skeleton);

// ---------------- ast_layout_skeleton_clone ----------------
// Clones an 'ast_layout_skeleton_t'
ast_layout_skeleton_t ast_layout_skeleton_clone(const ast_layout_skeleton_t *skeleton);

// ---------------- ast_layout_skeleton_str ----------------
// Converts an 'ast_layout_skeleton_t' to a string
strong_cstr_t ast_layout_skeleton_str(ast_layout_skeleton_t *skeleton, ast_field_map_t *field_map, ast_layout_endpoint_t root_endpoint);

// ---------------- ast_layout_skeletons_identical ----------------
// Returns whether two 'ast_layout_skeleton_t' values are identical
bool ast_layout_skeletons_identical(ast_layout_skeleton_t *skeleton_a, ast_layout_skeleton_t *skeleton_b);

// ---------------- ast_layout_skeleton_print ----------------
// Prints an 'ast_layout_skeleton_t' to stdout for debugging
// One 'indentation' = Four Spaces
void ast_layout_skeleton_print(ast_layout_skeleton_t *skeleton, int indentation);

// ---------------- ast_layout_skeleton_hash ----------------
// Hashes an AST layout skeleton
hash_t ast_layout_skeleton_hash(const ast_layout_skeleton_t *skeleton);

// ---------------- ast_layout_skeleton_has_polymorph ----------------
// Returns whether an 'ast_layout_skeleton_t' contains a polymorph
bool ast_layout_skeleton_has_polymorph(ast_layout_skeleton_t *skeleton);

// ---------------- ast_layout_bone_free ----------------
// Frees an 'ast_layout_bone_t'
void ast_layout_bone_free(ast_layout_bone_t *bone);

// ---------------- ast_layout_bone_clone ----------------
// Clones an 'ast_layout_bone_t'
ast_layout_bone_t ast_layout_bone_clone(ast_layout_bone_t *bone);

// ---------------- ast_layout_bone_has_polymorph ----------------
// Returns whether an 'ast_layout_bone_t' contains a polymorph
bool ast_layout_bone_has_polymorph(ast_layout_bone_t *bone);

// ---------------- ast_layout_bone_str ----------------
// Converts an 'ast_layout_bone_t' to a string
// Returns NULL on internal error
strong_cstr_t ast_layout_bone_str(ast_layout_bone_t *bone, ast_field_map_t *field_map, ast_layout_endpoint_t endpoint);

// ---------------- ast_layout_bone_hash ----------------
// Hashes an AST layout bone
hash_t ast_layout_bone_hash(const ast_layout_bone_t *bone);

// ---------------- ast_layout_bones_identical ----------------
// Returns whether two 'ast_layout_bone_t' values are identical
bool ast_layout_bones_identical(ast_layout_bone_t *bone_a, ast_layout_bone_t *bone_b);

// ---------------- ast_layout_bone_print ----------------
// Prints an 'ast_layout_bone_print' to stdout for debugging
// One 'indentation' = Four Spaces
void ast_layout_bone_print(ast_layout_bone_t *bone, int indentation);

// ---------------- ast_field_map_init ----------------
// Constructs an empty 'ast_field_map_t'
void ast_field_map_init(ast_field_map_t *field_map);

// ---------------- ast_field_map_free ----------------
// Frees an 'ast_field_map_t'
void ast_field_map_free(ast_field_map_t *field_map);

// ---------------- ast_field_map_clone ----------------
// Clones an 'ast_field_map_t'
ast_field_map_t ast_field_map_clone(const ast_field_map_t *field_map);

// ---------------- ast_field_maps_identical ----------------
// Returns whether two AST field maps are identical
bool ast_field_maps_identical(ast_field_map_t *field_map_a, ast_field_map_t *field_map_b);

// ---------------- ast_field_map_add ----------------
// Adds an arrow from individual components to an 'ast_field_map_t'
void ast_field_map_add(ast_field_map_t *field_map, strong_cstr_t name, ast_layout_endpoint_t endpoint);

// ---------------- ast_field_map_find ----------------
// Finds the endpoint which a name points to within
// an 'ast_field_map_t'
successful_t ast_field_map_find(ast_field_map_t *field_map, const char *name, ast_layout_endpoint_t *out_endpoint);

// ---------------- ast_field_map_get_name_of_endpoint ----------------
// Finds the first name which points to an endpoint within
// an 'ast_field_map_t'
// Returns NULL if none was found
maybe_null_weak_cstr_t ast_field_map_get_name_of_endpoint(ast_field_map_t *field_map, ast_layout_endpoint_t endpoint);

// ---------------- ast_field_map_get_name_of_endpoint ----------------
// Prints an 'ast_field_map_t' to stdout for debugging
// 'maybe_skeleton' is optional, but can be supplied in order to include field types
void ast_field_map_print(ast_field_map_t *field_map, ast_layout_skeleton_t *maybe_skeleton);

// ---------------- ast_field_map_hash ----------------
// Hashes an AST field map
hash_t ast_field_map_hash(const ast_field_map_t *field_map);

// ---------------- ast_simple_field_map_get_count ----------------
// Gets the number of arrows in an 'ast_field_map_t'
// This number is only significant if the 'ast_field_map_t' is
// either for a simple struct or a simple union.
// This function assumes that one of those is the case.
length_t ast_simple_field_map_get_count(ast_field_map_t *simple_field_map);

// ---------------- ast_simple_field_map_get_name_at_index ----------------
// Gets the name of an arrow in an 'ast_field_map_t' by index.
// This is only significant if the 'ast_field_map_t' is
// either for a simple struct or a simple union.
// This function assumes that one of those is the case.
weak_cstr_t ast_simple_field_map_get_name_at_index(ast_field_map_t *simple_field_map, length_t index);

// ---------------- ast_layout_skeleton_add_type ----------------
// Takes ownership of and adds an AST type to a layout skeleton
void ast_layout_skeleton_add_type(ast_layout_skeleton_t *skeleton, ast_type_t strong_type);

// ---------------- ast_layout_skeleton_add_struct ----------------
// Returns a weak pointer to newly added child struct
// The returned pointer is ONLY valid until the next bone is added to the skeleton
ast_layout_skeleton_t *ast_layout_skeleton_add_struct(ast_layout_skeleton_t *skeleton, trait_t bone_traits);

// ---------------- ast_layout_skeleton_add_union ----------------
// Returns a weak pointer to newly added child union
// The returned pointer is ONLY valid until the next bone is added to the skeleton
ast_layout_skeleton_t *ast_layout_skeleton_add_union(ast_layout_skeleton_t *skeleton, trait_t bone_traits);

// ---------------- ast_layout_skeleton_add_child_skeleton ----------------
// Returns a weak pointer to newly added child skeleton
// The returned pointer is ONLY valid until the next bone is added to the skeleton
ast_layout_skeleton_t *ast_layout_skeleton_add_child_skeleton(ast_layout_skeleton_t *skeleton, ast_layout_bone_kind_t bone_kind, trait_t bone_traits);

// ---------------- ast_layout_skeleton_get_type ----------------
// Returns the AST type of the value at an endpoint
ast_type_t *ast_layout_skeleton_get_type(ast_layout_skeleton_t *skeleton, ast_layout_endpoint_t endpoint);

// ---------------- ast_layout_skeleton_get_type_at_index ----------------
// Returns the AST type of the value at the endpoint [0xN] where N is 'index'
// The bone at 'index' must be an AST type
// Returns NULL when used incorrectly
ast_type_t *ast_layout_skeleton_get_type_at_index(ast_layout_skeleton_t *skeleton, length_t index);

#endif // _ISAAC_AST_LAYOUT_H
