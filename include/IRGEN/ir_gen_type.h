
#ifndef _ISAAC_IR_GEN_TYPE_H
#define _ISAAC_IR_GEN_TYPE_H

/*
    ============================== ir_gen_type.h ==============================
    Module for generating intermediate-representation types from abstract
    syntax tree types.
    ---------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "AST/ast.h"
#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_builder.h"

// Possible type conforming modes
#define CONFORM_MODE_NOT_APPLICABLE TRAIT_NONE // Basic conforming rules
#define CONFORM_MODE_STANDARD       TRAIT_NONE // Basic conforming rules
#define CONFORM_MODE_PRIMITIVES     TRAIT_1    // Allow conforming primitives
#define CONFORM_MODE_INTFLOAT       TRAIT_2    // Allow conforming between integers and floats
#define CONFORM_MODE_POINTERS       TRAIT_3    // Allow conforming any pointer types
#define CONFORM_MODE_INTPTR         TRAIT_4    // Allow conforming between integers and pointers
#define CONFORM_MODE_INTENUM        TRAIT_5    // Allow conforming between integers and enums
#define CONFORM_MODE_FROM_ANY       TRAIT_6    // Allow conforming from an Any
#define CONFORM_MODE_PTR_TO_BOOL    TRAIT_7    // Allow conforming from pointer to bool
#define CONFORM_MODE_POINTERPTR     TRAIT_8    // Allow conforming between pointers and non-specific ptr
#define CONFORM_MODE_INT_TO_BOOL    TRAIT_9    // Allow conforming from integers to bool
#define CONFORM_MODE_VARIADIC       TRAIT_A    // Allow conforming for variadic function
#define CONFORM_MODE_CLASS_POINTERS TRAIT_B    // Allow conforming class pointers
#define CONFORM_MODE_USER_EXPLICIT  TRAIT_C    // Allow explicit user-defined conversions via __as__
#define CONFORM_MODE_USER_IMPLICIT  TRAIT_D    // Allow implicit user-defined conversions via __as__
#define CONFORM_MODE_ALL            TRAIT_ALL  // Allow all conformation methods

#define CONFORM_MODE_CALL_ARGUMENTS              CONFORM_MODE_STANDARD
#define CONFORM_MODE_CALL_ARGUMENTS_LOOSE_NOUSER CONFORM_MODE_PRIMITIVES | CONFORM_MODE_VARIADIC | CONFORM_MODE_POINTERPTR | CONFORM_MODE_PTR_TO_BOOL | CONFORM_MODE_CLASS_POINTERS
#define CONFORM_MODE_CALL_ARGUMENTS_LOOSE        CONFORM_MODE_CALL_ARGUMENTS_LOOSE_NOUSER | CONFORM_MODE_USER_IMPLICIT
#define CONFORM_MODE_ASSIGNING                   CONFORM_MODE_PRIMITIVES | CONFORM_MODE_POINTERPTR | CONFORM_MODE_PTR_TO_BOOL | CONFORM_MODE_INT_TO_BOOL | CONFORM_MODE_CLASS_POINTERS | CONFORM_MODE_USER_IMPLICIT
#define CONFORM_MODE_CALCULATION                 CONFORM_MODE_PRIMITIVES | CONFORM_MODE_POINTERPTR | CONFORM_MODE_PTR_TO_BOOL | CONFORM_MODE_INT_TO_BOOL

// ---------------- ir_gen_type_mappings ----------------
// Generates IR type mappings for all standard and
// user-defined AST types. (Excludes type modifies)
errorcode_t ir_gen_type_mappings(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_resolve_type ----------------
// Resolves an AST type to an IR type
errorcode_t ir_gen_resolve_type(compiler_t *compiler, object_t *object, const ast_type_t *unresolved_type, ir_type_t **resolved_type);

// ---------------- ast_types_conform ----------------
// Attempts to conform an IR value from an AST type to
// a different AST type. Returns true if successfully
// conformed the value to the new type.
successful_t ast_types_conform(ir_builder_t *builder, ir_value_t **ir_value, ast_type_t *ast_from_type, ast_type_t *ast_to_type, trait_t mode);

// ---------------- ast_types_merge ----------------
// Attempts to find a common AST type for two IR values
// and merge them into a common AST type. Returns true
// if successfully merged them to a common AST type.
successful_t ast_types_merge(ir_builder_t *builder, ir_value_t **ir_value_a, ir_value_t **ir_value_b, ast_type_t *ast_type_a, ast_type_t *ast_type_b);

// ---------------- ast_layout_bone_to_ir_type ----------------
// Converts an ast_layout_bone_t to an ir_type_t
// 'optional_catalog' and 'optional_type_table', may be NULL
// Returns NULL when something goes wrong
ir_type_t *ast_layout_bone_to_ir_type(compiler_t *compiler, object_t *object, ast_layout_bone_t *bone, ast_poly_catalog_t *optional_catalog);

// ---------------- ast_layout_path_get_offset ----------------
// Returns an IR value that represented the offset in bytes
// of an endpoint of an IR type by using an ast_layout_endpoint_path_t
// NOTE: Returns NULL on failure
ir_value_t *ast_layout_path_get_offset(ir_module_t *ir_module, ast_layout_endpoint_t *endpoint, ast_layout_endpoint_path_t *path, ir_type_t *root_ir_type);

// ---------------- ir_type_mapping_cmp ----------------
// Compares two 'ir_type_mapping_t' structures.
// Used to sort IR type mappings with qsort()
int ir_type_mapping_cmp(const void *a, const void *b);

// ---------------- ir_primitive_from_ast_type ----------------
// Returns suitable IR primitive value type kind TYPE_KIND_*
// for the given AST type
unsigned int ir_primitive_from_ast_type(const ast_type_t *type);

#endif // _ISAAC_IR_GEN_TYPE_H
