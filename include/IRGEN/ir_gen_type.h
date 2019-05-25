
#ifndef IR_GEN_TYPE_H
#define IR_GEN_TYPE_H

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
#define CONFORM_MODE_STANDARD      TRAIT_NONE // Basic conforming rules
#define CONFORM_MODE_PRIMITIVES    TRAIT_1    // Allow conforming primitives
#define CONFORM_MODE_INTFLOAT      TRAIT_2    // Allow conforming between integers and floats
#define CONFORM_MODE_POINTERS      TRAIT_3    // Allow conforming any pointer types
#define CONFORM_MODE_INTPTR        TRAIT_4    // Allow conforming between integers and pointers
#define CONFORM_MODE_INTENUM       TRAIT_5    // Allow conforming between integers and enums
#define CONFORM_MODE_FROM_ANY      TRAIT_6    // Allow conforming from an Any
#define CONFORM_MODE_BOOLPTR       TRAIT_7    // Allow conforming from pointer to bool
#define CONFORM_MODE_POINTERPTR    TRAIT_8    // Allow conforming between pointers and non-specific ptr
#define CONFORM_MODE_ALL           TRAIT_ALL  // Allow all conformation methods

#define CONFORM_MODE_CALL_ARGUMENTS CONFORM_MODE_PRIMITIVES | CONFORM_MODE_POINTERPTR
#define CONFORM_MODE_ASSIGNING      CONFORM_MODE_PRIMITIVES | CONFORM_MODE_POINTERPTR | CONFORM_MODE_BOOLPTR
#define CONFORM_MODE_CALCULATION    CONFORM_MODE_PRIMITIVES | CONFORM_MODE_POINTERPTR | CONFORM_MODE_BOOLPTR

// ---------------- ir_gen_type_mappings ----------------
// Generates IR type mappings for all standard and
// user-defined AST types. (Excludes type modifies)
errorcode_t ir_gen_type_mappings(compiler_t *compiler, object_t *object);

// ---------------- ir_gen_resolve_type ----------------
// Resolves an AST type to an IR type
errorcode_t ir_gen_resolve_type(compiler_t *compiler, object_t *object, ast_type_t *unresolved_type, ir_type_t **resolved_type);

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

// ---------------- ir_type_mapping_cmp ----------------
// Compares two 'ir_type_mapping_t' structures.
// Used to sort IR type mappings with qsort()
int ir_type_mapping_cmp(const void *a, const void *b);

// ---------------- ir_primitive_from_ast_type ----------------
// Returns suitable IR primitive value type kind TYPE_KIND_*
// for the given AST type
unsigned int ir_primitive_from_ast_type(const ast_type_t *type);

#endif // IR_GEN_TYPE_H
