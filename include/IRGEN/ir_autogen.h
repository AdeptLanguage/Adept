
#ifndef _ISAAC_IR_AUTOGEN_H
#define _ISAAC_IR_AUTOGEN_H

/*
    =============================== ir_autogen.h ===============================
    Helper module for automatically generating management procedures
    ----------------------------------------------------------------------------
*/

#include "AST/ast_type.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/ground.h"

enum ir_autogen_action_kind {
    IR_AUTOGEN_ACTION_DEFER,
    IR_AUTOGEN_ACTION_PASS
};

errorcode_t ir_autogen_for_children_of_struct_like(
    ir_builder_t *builder,
    enum ir_autogen_action_kind action_kind,
    ast_elem_t *struct_like_elem,
    ast_type_t *param_ast_type
);

errorcode_t ir_autogen_for_child_of_struct(
    ir_builder_t *builder,
    enum ir_autogen_action_kind action_kind,
    ast_type_t *ast_field_type,
    length_t field_i,
    ast_type_t *param_ast_type,
    source_t composite_source,
    source_t field_source
);

#endif // _ISAAC_IR_AUTOGEN_H
