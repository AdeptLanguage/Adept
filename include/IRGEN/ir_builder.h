
#ifndef _ISAAC_IR_BUILDER_H
#define _ISAAC_IR_BUILDER_H

/*
    =============================== ir_builder.h ===============================
    Module for building intermediate representation
    ----------------------------------------------------------------------------
*/

#include <stdbool.h>

#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "BRIDGE/bridge.h"
#include "BRIDGE/rtti_collector.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "IR/ir_pool.h"
#include "IR/ir_type.h"
#include "IR/ir_type_map.h"
#include "IR/ir_value.h"
#include "UTIL/datatypes.h"
#include "UTIL/func_pair.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/trait.h"

typedef struct {
    weak_cstr_t label;
    length_t break_id;
    length_t continue_id;
    bridge_scope_t *scope;
} block_t;

typedef listof(block_t, blocks) block_stack_t;
#define block_stack_push(LIST, VALUE) list_append((LIST), (VALUE), block_t)
#define block_stack_pop(LIST) ((LIST)->length--)

// ---------------- ir_builder_t ----------------
// Container for storing general information
// about the building state
typedef struct ir_builder {
    compiler_t *compiler;
    object_t *object;
    ir_basicblocks_t basicblocks;
    ir_basicblock_t *current_block;
    length_t current_block_id;
    length_t break_block_id; // 0 == none
    length_t continue_block_id; // 0 == none
    length_t fallthrough_block_id; // 0 == none
    bridge_scope_t *break_continue_scope;
    bridge_scope_t *fallthrough_scope;
    block_stack_t block_stack;
    ir_pool_t *pool;
    ir_type_map_t *type_map;
    func_id_t ast_func_id;
    func_id_t ir_func_id;
    bridge_scope_t *scope;
    length_t next_var_id;
    troolean has_string_struct;
    ir_job_list_t *job_list;
    ast_type_t static_bool;
    ast_elem_base_t static_bool_base;
    ast_elem_t *static_bool_elems;
    ir_type_t *s8_type;
    ir_type_t *ptr_type;
    func_id_t noop_defer_function;
    bool has_noop_defer_function;
} ir_builder_t;

#include "IR/ir_module.h"

// ---------------- ir_builder_init ----------------
// Initializes an IR builder
void ir_builder_init(ir_builder_t *builder, compiler_t *compiler, object_t *object, func_id_t ast_func_id, func_id_t ir_func_id, bool static_builder);

// ---------------- build_basicblock ----------------
// Builds a new basic block in the current function
// NOTE: All basic block pointers should be recalculated
//       after calling this function
length_t build_basicblock(ir_builder_t *builder);

// ---------------- build_using_basicblock ----------------
// Changes the current basic block that new instructions are added into
void build_using_basicblock(ir_builder_t *builder, length_t basicblock_id);

// ---------------- build_instruction ----------------
// Builds a new undetermined instruction
ir_instr_t *build_instruction(ir_builder_t *builder, length_t size);

// ---------------- build_value_from_prev_instruction ----------------
// Builds an IR value from the result of the previous instruction
ir_value_t *build_value_from_prev_instruction(ir_builder_t *builder);

// ---------------- ir_builder_add_rtti_relocation ----------------
// Adds an RTTI index that requires resolution to the rtti_relocations array
// NOTE: Despite its name, this function does not add any instructions,
//       it simply marks an RTTI index to be filled in later
void ir_builder_add_rtti_relocation(ir_builder_t *builder, strong_cstr_t human_notation, adept_usize *id_ref, source_t source_on_failure);

// ---------------- ir_builder_usize ----------------
// Gets a shared IR usize type
ir_type_t *ir_builder_usize(ir_builder_t *builder);

// ---------------- ir_builder_usize_ptr ----------------
// Gets a shared IR usize pointer type
ir_type_t *ir_builder_usize_ptr(ir_builder_t *builder);

// ---------------- ir_builder_bool ----------------
// Gets a shared IR boolean type
ir_type_t *ir_builder_bool(ir_builder_t *builder);

// ---------------- ir_builder___types__ ----------------
// Gets the global variable index for the runtime type information array '__types__'
maybe_index_t ir_builder___types__(ir_builder_t *builder, source_t source_on_failure);

// ---------------- ir_builder_open_scope ----------------
// Opens a new scope
void ir_builder_open_scope(ir_builder_t *builder);

// ---------------- ir_builder_close_scope ----------------
// Closes the current scope
void ir_builder_close_scope(ir_builder_t *builder);

// ---------------- ir_builder_push_loop_label ----------------
// Pushes a block label off of the block label stack
void ir_builder_push_loop_label(ir_builder_t *builder, weak_cstr_t label, length_t break_basicblock_id, length_t continue_basicblock_id);

// ---------------- ir_builder_pop_loop_label ----------------
// Pops a block label off of the block label stack
void ir_builder_pop_loop_label(ir_builder_t *builder);

// ---------------- ir_builder_add_variable ----------------
// Adds a variable to the current bridge_scope_t
// Returns a temporary pointer to the constructed variable
bridge_var_t *ir_builder_add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits);

// ---------------- handle_deference_for_variables ----------------
// Handles deference for variables in a variable list
// Returns FAILURE on compile time error
errorcode_t handle_deference_for_variables(ir_builder_t *builder, bridge_var_list_t *list);

// ---------------- handle_deference_for_globals ----------------
// Handles deference for global variables
// Returns FAILURE on compile time error
errorcode_t handle_deference_for_globals(ir_builder_t *builder);

// ---------------- handle_single_deference ----------------
// Calls __defer__ method on a value and it's children if the method exists
// NOTE: Assumes (ast_type->elements_length == 1)
// NOTE: Returns SUCCESS if value was utilized in deference
//       Returns FAILURE if value was not utilized in deference
//       Returns ALT_FAILURE if a compile time error occurred
errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value, source_t from_source);

errorcode_t handle_children_deference(ir_builder_t *builder);

// ---------------- could_have_deference ----------------
// Returns whether a type could have __defer__ methods that need calling
bool could_have_deference(ast_type_t *ast_type);

// ---------------- handle_pass_management ----------------
// Handles '__pass__' management function calls for passing arguments
// NOTE: 'arg_type_traits' can be NULL
// NOTE: Returns SUCCESS if nothing went wrong
// NOTE: Returns FAILURE if compile time error occurred
errorcode_t handle_pass_management(ir_builder_t *builder, ir_value_t **values, ast_type_t *types, trait_t *arg_type_traits, length_t arity);

// ---------------- handle_single_pass ----------------
// Calls __pass__ function for a value and it's children if the function exists
// NOTE: Assumes (ast_type->elements_length == 1)
// NOTE: Returns SUCCESS if value was utilized in passing
//       Returns FAILURE if value was not utilized in passing
//       Returns ALT_FAILURE if a compiler time error occurred
errorcode_t handle_single_pass(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value, source_t from_source);

errorcode_t handle_children_pass_root(ir_builder_t *builder, bool already_has_return);
errorcode_t handle_children_pass(ir_builder_t *builder);

// ---------------- could_have_pass ----------------
// Returns whether a type could have __pass__ methods that need calling
bool could_have_pass(ast_type_t *ast_type);

// ---------------- handle_assign_management ----------------
// Handles '__assign__' management method calls
// NOTE: 'ast_destination_type' is not a pointer, but value provided is mutable
// NOTE: Returns SUCCESS if value was utilized in assignment
//       Returns FAILURE if value was not utilized in assignment
//       Returns ALT_FAILURE if a compile time error occurred
errorcode_t handle_assign_management(
    ir_builder_t *builder,
    ir_value_t *value,
    ast_type_t *value_ast_type,
    ir_value_t *destination,
    ast_type_t *destination_ast_type,
    source_t source_on_failure
);

// ---------------- handle_math_management ----------------
// Handles basic math management function calls
// NOTE: 'from_source' is used for error messages, and may be NULL_SOURCE
typedef struct {
    ir_value_t *lhs, *rhs;
    ast_type_t *lhs_type, *rhs_type;
} ir_math_operands_t;

static inline ir_math_operands_t ir_math_operands_flipped(ir_math_operands_t *ops){
    return (ir_math_operands_t){
        .lhs = ops->rhs,
        .lhs_type = ops->rhs_type,
        .rhs = ops->lhs,
        .rhs_type = ops->lhs_type,
    };
}

ir_value_t *handle_math_management(ir_builder_t *builder, ir_math_operands_t *operands, source_t from_source, ast_type_t *out_type, weak_cstr_t overload_name);
ir_value_t *handle_math_management_allow_other_direction(ir_builder_t *builder, ir_math_operands_t *operands, source_t from_source, ast_type_t *out_type, weak_cstr_t overload_name);

// ---------------- handle_access_management ----------------
// Handles '__access__' management function calls for [] operator
// NOTE: Returns SUCCESS if nothing went wrong
// NOTE: Returns FAILURE if compile time error occurred
ir_value_t *handle_access_management(
    ir_builder_t *builder,
    ir_value_t *value,
    ir_value_t *index_value,
    ast_type_t *array_type,
    ast_type_t *index_type,
    ast_type_t *out_ptr_to_element_type,
    source_t source
);

// ---------------- instantiate_poly_func ----------------
// Instantiates a polymorphic function
// NOTE: 'instantiation_source' may be NULL_SOURCE
errorcode_t instantiate_poly_func(compiler_t *compiler, object_t *object, source_t instantiation_source, func_id_t ast_poly_func_id, ast_type_t *types,
        length_t types_list_length, ast_poly_catalog_t *catalog, length_t instantiation_depth, ir_func_endpoint_t *out_endpoint);

// ---------------- instantiate_default_for_virtual_dispatcher ----------------
// Instantiates (if necessary) a concrete version of the default implementation
// of a just-instantiated virtual dispatcher.
errorcode_t instantiate_default_for_virtual_dispatcher(
    compiler_t *compiler,
    object_t *object,
    func_id_t dispatcher_id,
    length_t instantiation_depth,
    source_t instantiation_source,
    ast_poly_catalog_t *catalog,
    func_id_t *out_ast_concrete_virtual_origin
);

// ---------------- attempt_autogen___defer__ ----------------
// Attempts to auto-generate __defer__ management method
// NOTE: Does NOT check for existing suitable __defer__ methods
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___defer__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, length_t instantiation_depth, optional_func_pair_t *result);

// ---------------- attempt_autogen___pass__ ----------------
// Attempts to auto-generate __pass__ management function
// NOTE: Does NOT check for existing suitable __pass__ functions
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___pass__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, length_t instantiation_depth, optional_func_pair_t *result);

// ---------------- attempt_autogen___assign__ ----------------
// Attempts to auto-generate __assign__ management method
// NOTE: Does NOT check for existing suitable __assign__ methods
// NOTE: Returns FAILURE if couldn't auto generate
// NOTE: Returns ALT_FAILURE if something went really wrong
errorcode_t attempt_autogen___assign__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, length_t instantiation_depth, optional_func_pair_t *result);

// ---------------- is_allowed_builtin_auto_conversion ----------------
// Returns whether a builtin auto conversion is allowed
// (For integers / floats)
bool is_allowed_builtin_auto_conversion(compiler_t *compiler, object_t *object, const ast_type_t *a_type, const ast_type_t *b_type);

// ---------------- ir_builder_get_loop_label_info ----------------
// Returns a temporary pointer to the block information associated with a loop label
// Returns NULL if no label with the given name exists
block_t *ir_builder_get_loop_label_info(ir_builder_t *builder, const char *label);

// ---------------- ir_builder_get_noop_defer_func ----------------
// Gets no-op defer function
// Will create if doesn't already exist
errorcode_t ir_builder_get_noop_defer_func(ir_builder_t *builder, source_t source_on_error, func_id_t *out_ir_func_id);

// ---------------- ir_gen_actualize_unknown_enum ----------------
// Converts a value of an unknown enum to a concrete IR value
ir_value_t *ir_gen_actualize_unknown_enum(compiler_t *compiler, object_t *object, weak_cstr_t enum_name, weak_cstr_t kind_name, source_t source, ast_type_t *out_expr_type);

// ---------------- ir_gen_actualize_unknown_plural_enum ----------------
// Converts a value of an unknown plural enum to a concrete IR value for a regular enum
errorcode_t ir_gen_actualize_unknown_plural_enum(ir_builder_t *builder, const char *enum_name, const strong_cstr_list_t *kinds, ir_value_t **ir_value, source_t source);

// ---------------- ir_gen_actualize_unknown_plural_enum_to_anonymous ----------------
// Converts a value of an unknown plural enum to a concrete IR value for an anonymous enum
errorcode_t ir_gen_actualize_unknown_plural_enum_to_anonymous(ir_builder_t *builder, const strong_cstr_list_t *to_kinds, const strong_cstr_list_t *from_kinds, ir_value_t **ir_value, source_t source);

// ---------------- ir_builder_instantiation_depth ----------------
// Gets the current instantiation depth of a builder
length_t ir_builder_instantiation_depth(ir_builder_t *builder);

// ---------------- ir_instrs_snapshot_t ----------------
// Snapshot used to easily reset the forward generation of IR instructions
typedef struct {
    length_t current_block_id;
    length_t current_basicblock_instructions_length;
    length_t basicblocks_length;
    length_t funcs_length;
    length_t job_list_length;
} ir_instrs_snapshot_t;

ir_instrs_snapshot_t ir_instrs_snapshot_capture(ir_builder_t *builder);
void ir_instrs_snapshot_restore(ir_builder_t *builder, ir_instrs_snapshot_t *snapshot);

#endif // _ISAAC_IR_BUILDER_H
