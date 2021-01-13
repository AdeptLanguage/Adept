
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

typedef struct {
    ir_value_t **array_values;            // non-null, weak pointer to RTTI value skeletons which are currently being filled in
    ir_rtti_types_t *rtti_types;          // non-null
    ast_composite_t *core_composite_info; // non-null
    type_table_entry_t *entry;            // non-null
    
    // The following only exist if 'core_composite_info->is_polymorphic' is true
    ast_poly_catalog_t poly_catalog;
    ast_type_t *maybe_weak_generics;
    length_t maybe_weak_generics_length;
} ir_gen_composite_rtti_info_t;

errorcode_t ir_gen__types__(compiler_t *compiler, object_t *object, ir_global_t *ir_global);

errorcode_t ir_gen_rtti_fetch_rtti_types(ir_module_t *ir_module, ir_rtti_types_t *out_rtti_types);

errorcode_t ir_gen__types__placeholder(object_t *object, ir_global_t *ir_global);

ir_value_t **ir_gen__types__values(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types);

errorcode_t ir_gen__types__prepare_each_value(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types, ir_value_t **array_values);

errorcode_t ir_gen__types__composite_entry(compiler_t *compiler, object_t *object, ir_value_t **array_values, length_t array_value_index, ir_rtti_types_t *rtti_types);
errorcode_t ir_gen__types__composite_entry_get_info(compiler_t *compiler, object_t *object, ir_rtti_types_t *rtti_types, type_table_entry_t *entry, ir_value_t **array_values, ir_gen_composite_rtti_info_t *out_info);
void ir_gen__types__composite_entry_free_info(ir_gen_composite_rtti_info_t *info);
ir_value_t *ir_gen__types__composite_entry_members_array(compiler_t *compiler, object_t *object, ir_gen_composite_rtti_info_t *info);
ir_value_t *ir_gen__types__composite_entry_offsets_array(object_t *object, ir_gen_composite_rtti_info_t *info);
ir_value_t *ir_gen__types__composite_entry_member_names_array(ir_module_t *ir_module, ir_gen_composite_rtti_info_t *info);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_GEN_RTTI_H
