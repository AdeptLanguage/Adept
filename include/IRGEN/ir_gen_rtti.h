
#ifndef _ISAAC_IR_GEN_RTTI_H
#define _ISAAC_IR_GEN_RTTI_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================== ir_gen_rtti.h ==============================
    Module for generating RTTI IR constructs
    ----------------------------------------------------------------------------
*/

#include "UTIL/ground.h"
#include "DRVR/compiler.h"
#include "IR/ir.h"

// ---------------- ir_rtti_types_t ----------------
// Collection of IR types necessary for generating RTTI
typedef struct {
    ir_type_t *any_type_type;
    ir_type_t *any_composite_type_type;
    ir_type_t *any_ptr_type_type;
    ir_type_t *any_funcptr_type_type;
    ir_type_t *any_fixed_array_type_type;
    ir_type_t *any_type_ptr_type;
    ir_type_t *ubyte_ptr_type;
    ir_type_t *usize_type;
} ir_rtti_types_t;

// ---------------- ir_gen_composite_rtti_info_t ----------------
// Common information used to generate RTTI info for composite types
typedef struct {
    ir_value_t **array_values;            // non-null, weak pointer to RTTI value skeletons which are currently being filled in
    ir_rtti_types_t *rtti_types;          // non-null
    ast_composite_t *core_composite_info; // non-null
    type_table_entry_t *entry;            // non-null
    
    // The following only exist if 'core_composite_info->is_polymorphic' is true
    ast_poly_catalog_t poly_catalog;
    ast_type_t *maybe_weak_generics;
    length_t maybe_weak_generics_length;

    // The following only exist as buffers and should not be accessed directly
    // (After the creation of the ir_gen_composite_rtti_info_t)
    ast_composite_t anonymous_composite_info_collection;
} ir_gen_composite_rtti_info_t;

// ---------------- ir_gen__types__ ----------------
// Initializes the '__types__' global variable
errorcode_t ir_gen__types__(compiler_t *compiler, object_t *object, ir_global_t *ir_global);

// ---------------- ir_gen_rtti_fetch_rtti_types ----------------
// Collects IR types necessary for generating RTTI
errorcode_t ir_gen_rtti_fetch_rtti_types(ir_module_t *ir_module, ir_rtti_types_t *out_rtti_types);

// ---------------- ir_gen__types__placeholder ----------------
// Initializes the '__types__' global variable as a null pointer
errorcode_t ir_gen__types__placeholder(object_t *object, ir_global_t *ir_global);

// ---------------- ir_gen__types__values ----------------
// Generates the RTTI values for the global '__types__' array
ir_value_t **ir_gen__types__values(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__prepare_each_value ----------------
// Prepares each of the RTTI values for the global '__types__' array
// Called from 'ir_gen__types__values'
errorcode_t ir_gen__types__prepare_each_value(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types, ir_value_t **array_values);

// ---------------- ir_gen__types__pointer_entry ----------------
// Fills in the RTTI for a pointer type inside the prepared global values for '__types__'
errorcode_t ir_gen__types__pointer_entry(object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__fixed_array_entry ----------------
// Fills in the RTTI for a fixed array type inside the prepared global values for '__types__'
errorcode_t ir_gen__types__fixed_array_entry(object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__func_ptr_entry ----------------
// Fills in the RTTI for a function pointer type inside the prepared global values for '__types__'
errorcode_t ir_gen__types__func_ptr_entry(object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__primitive_entry ----------------
// Fills in the RTTI for a primitive type inside the prepared global values for '__types__'
errorcode_t ir_gen__types__primitive_entry(object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__composite_entry ----------------
// Fills in the RTTI for a composite type inside the prepared global values for '__types__'
errorcode_t ir_gen__types__composite_entry(compiler_t *compiler, object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);

// ---------------- ir_gen__types__composite_entry_get_info ----------------
// Collects common information used to generate RTTI info for a composite type
errorcode_t ir_gen__types__composite_entry_get_info(object_t *object, ir_rtti_types_t *rtti_types, type_table_entry_t *entry, ir_value_t **array_values, ir_gen_composite_rtti_info_t *out_info);

// ---------------- ir_gen__types__composite_entry_free_info ----------------
// Frees common information used to generate RTTI info for a composite type
void ir_gen__types__composite_entry_free_info(ir_gen_composite_rtti_info_t *info);

// ---------------- ir_gen__types__composite_entry_members_array ----------------
// Generates the anonymous global array of member types for the RTTI of a composite type
ir_value_t *ir_gen__types__composite_entry_members_array(compiler_t *compiler, object_t *object, ir_gen_composite_rtti_info_t *info);

// ---------------- ir_gen__types__composite_entry_offsets_array ----------------
// Generates the anonymous global array of member offsets for the RTTI of a composite type
ir_value_t *ir_gen__types__composite_entry_offsets_array(object_t *object, ir_gen_composite_rtti_info_t *info);

// ---------------- ir_gen__types__composite_entry_member_names_array ----------------
// Generates the anonymous global array of member names for the RTTI of a composite type
ir_value_t *ir_gen__types__composite_entry_member_names_array(ir_module_t *ir_module, ir_gen_composite_rtti_info_t *info);

// ---------------- ir_gen__types__get_rtti_pointer_for ----------------
// Gets the RTTI type information pointer for an AST type
// Returns an ir_value_t that's a null pointer if the type couldn't be found
ir_value_t *ir_gen__types__get_rtti_pointer_for(object_t *object, ast_type_t *ast_type, ir_value_t **array_values, ir_rtti_types_t *rtti_types);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_RTTI_H
