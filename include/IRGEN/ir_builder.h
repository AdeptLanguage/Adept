
#ifndef _ISAAC_IR_BUILDER_H
#define _ISAAC_IR_BUILDER_H

/*
    =============================== ir_builder.h ===============================
    Module for building intermediate representation
    ----------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "IR/ir_func_endpoint.h"
#include "AST/ast_type.h"
#include "AST/ast_poly_catalog.h"
#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"
#include "BRIDGE/bridge.h"
#include "BRIDGEIR/funcpair.h"

// ---------------- ir_builder_t ----------------
// Container for storing general information
// about the building state
typedef struct ir_builder {
    ir_basicblocks_t basicblocks;
    ir_basicblock_t *current_block;
    length_t current_block_id;
    length_t break_block_id; // 0 == none
    length_t continue_block_id; // 0 == none
    length_t fallthrough_block_id; // 0 == none
    bridge_scope_t *break_continue_scope;
    bridge_scope_t *fallthrough_scope;
    weak_cstr_t *block_stack_labels;
    length_t *block_stack_break_ids;
    length_t *block_stack_continue_ids;
    bridge_scope_t **block_stack_scopes;
    length_t block_stack_length;
    length_t block_stack_capacity;
    ir_pool_t *pool;
    ir_type_map_t *type_map;
    compiler_t *compiler;
    object_t *object;
    funcid_t ast_func_id;
    funcid_t ir_func_id;
    bridge_scope_t *scope;
    length_t next_var_id;
    troolean has_string_struct;
    ir_job_list_t *job_list;
    ast_type_t static_bool;
    ast_elem_base_t static_bool_base;
    ast_elem_t *static_bool_elems;
    ir_type_t *stack_pointer_type;
    ir_type_t *s8_type;
    ir_type_t *ptr_type;
    type_table_t *type_table;
    funcid_t noop_defer_function;
    bool has_noop_defer_function;
} ir_builder_t;

// ---------------- ir_builder_init ----------------
// Initializes an IR builder
void ir_builder_init(ir_builder_t *builder, compiler_t *compiler, object_t *object, funcid_t ast_func_id, funcid_t ir_func_id, bool static_builder);

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

// ---------------- build_varptr ----------------
// Builds a varptr instruction to either static or local
// variable based on bridge_var_t
ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, bridge_var_t *var);

// ---------------- build_lvarptr ----------------
// Builds a localvarptr instruction
ir_value_t *build_lvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_varptr ----------------
// Builds a globalvarptr instruction
ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_svarptr ----------------
// Builds a staticvarptr instruction
ir_value_t *build_svarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_malloc ----------------
// Builds a malloc instruction
ir_value_t *build_malloc(ir_builder_t *builder, ir_type_t *type, ir_value_t *amount, bool is_undef, ir_type_t *optional_result_ptr_type);

// ---------------- build_zeroinit ----------------
// Builds a zero initialization instruction
void build_zeroinit(ir_builder_t *builder, ir_value_t *destination);

// ---------------- build_load ----------------
// Builds a load instruction
ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source);

// ---------------- build_store ----------------
// Builds a store instruction
void build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source);

// ---------------- build_call ----------------
// Builds a call instruction
// NOTE: If 'return_result_value' is false, then NULL will be returned (in an effort to avoid unnecessary allocations)
ir_value_t *build_call(ir_builder_t *builder, funcid_t ir_func_id, ir_type_t *result_type, ir_value_t **arguments, length_t arguments_length, bool return_result_value);

// ---------------- build_break ----------------
// Builds a break instruction
void build_break(ir_builder_t *builder, length_t basicblock_id);

// ---------------- build_cond_break ----------------
// Builds a conditional break instruction
void build_cond_break(ir_builder_t *builder, ir_value_t *condition, length_t true_block_id, length_t false_block_id);

// ---------------- build_equals ----------------
// Builds an equals instruction
ir_value_t *build_equals(ir_builder_t *builder, ir_value_t *a, ir_value_t *b);

// ---------------- build_array_access ----------------
// Builds an array access instruction
ir_value_t *build_array_access(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, source_t code_source);
ir_value_t *build_array_access_ex(ir_builder_t *builder, ir_value_t *value, ir_value_t *index, ir_type_t *result_type, source_t code_source);

// ---------------- build_member ----------------
// Builds a member access instruction
ir_value_t *build_member(ir_builder_t *builder, ir_value_t *value, length_t member, ir_type_t *result_type, source_t code_source);

// ---------------- build_return ----------------
// Builds a return instruction
void build_return(ir_builder_t *builder, ir_value_t *value);

// ---------------- build_static_struct ----------------
// Builds a static struct
ir_value_t *build_static_struct(ir_module_t *module, ir_type_t *type, ir_value_t **values, length_t length, bool make_mutable);

// ---------------- build_struct_construction ----------------
// Builds a struct construction
ir_value_t *build_struct_construction(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length);

// ---------------- build_offsetof ----------------
// Builds an 'offsetof' value
ir_value_t *build_offsetof(ir_builder_t *builder, ir_type_t *type, length_t index);
ir_value_t *build_offsetof_ex(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type, length_t index);

// ---------------- build_const_sizeof ----------------
// Builds constant 'sizeof' value
ir_value_t *build_const_sizeof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type);

// ---------------- build_const_alignof ----------------
// Builds constant 'alignof' value
ir_value_t *build_const_alignof(ir_pool_t *pool, ir_type_t *usize_type, ir_type_t *type);

// ---------------- build_const_add ----------------
// Builds constant 'add' value
// NOTE: Does NOT check whether 'a' and 'b' are of the same type
ir_value_t *build_const_add(ir_pool_t *pool, ir_value_t *a, ir_value_t *b);

// ---------------- build_static_array ----------------
// Builds a static array
ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length);

// ---------------- build_anon_global ----------------
// Builds an anonymous global variable
ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant);

// ---------------- build_anon_global_initializer ----------------
// Builds an anonymous global variable initializer
void build_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer);

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

// ---------------- build_literal_int ----------------
// Builds a literal int value
ir_value_t *build_literal_int(ir_pool_t *pool, adept_int value);

// ---------------- build_literal_usize ----------------
// Builds a literal usize value
ir_value_t *build_literal_usize(ir_pool_t *pool, adept_usize value);

// ---------------- build_literal_str ----------------
// Builds a literal string value
// NOTE: If no 'String' type is present, an error will be printed and NULL will be returned
ir_value_t *build_literal_str(ir_builder_t *builder, char *array, length_t length);

// ---------------- build_literal_cstr ----------------
// Builds a literal c-string value
ir_value_t *build_literal_cstr(ir_builder_t *builder, weak_cstr_t value);
ir_value_t *build_literal_cstr_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value);

// ---------------- build_literal_cstr ----------------
// Builds a literal c-string value
ir_value_t *build_literal_cstr_of_length(ir_builder_t *builder, weak_cstr_t value, length_t length);
ir_value_t *build_literal_cstr_of_length_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value, length_t size);

// ---------------- build_null_pointer ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer(ir_pool_t *pool);

// ---------------- build_null_pointer_of_type ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type);

// ---------------- build_cast ----------------
// Casts an IR value to an IR type
// NOTE: const_cast_value_type is a valid VALUE_TYPE_* cast value
// NOTE: nonconst_cast_instr_type is a valid INSTRUCTION_* cast value
ir_value_t *build_cast(ir_builder_t *builder, unsigned int const_cast_value_type, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t *to);

// ---------------- build_const_cast ----------------
// Builds a constant cast value and returns it
// NOTE: const_cast_value_type is a valid VALUE_TYPE_* cast value
ir_value_t *build_const_cast(ir_pool_t *pool, unsigned int const_cast_value_type, ir_value_t *from, ir_type_t *to);

// ---------------- build_nonconst_cast ----------------
// Builds a non-constant cast instruction and returns
// the casted result as an ir_value_t*
// NOTE: nonconst_cast_instr_type is a valid INSTRUCTION_* cast value
ir_value_t *build_nonconst_cast(ir_builder_t *builder, unsigned int nonconst_cast_instr_type, ir_value_t *from, ir_type_t* to);

// ---------------- build_<cast> ----------------
// Builds a specialized value cast
#define build_bitcast(builder, from, to)     build_cast(builder, VALUE_TYPE_CONST_BITCAST, INSTRUCTION_BITCAST, from, to)
#define build_zext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_ZEXT, INSTRUCTION_ZEXT, from, to)
#define build_sext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_SEXT, INSTRUCTION_SEXT, from, to)
#define build_trunc(builder, from, to)       build_cast(builder, VALUE_TYPE_CONST_TRUNC, INSTRUCTION_TRUNC, from, to)
#define build_fext(builder, from, to)        build_cast(builder, VALUE_TYPE_CONST_FEXT, INSTRUCTION_FEXT, from, to)
#define build_ftrunc(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FTRUNC, INSTRUCTION_FTRUNC, from, to)
#define build_inttoptr(builder, from, to)    build_cast(builder, VALUE_TYPE_CONST_INTTOPTR, INSTRUCTION_INTTOPTR, from, to)
#define build_ptrtoint(builder, from, to)    build_cast(builder, VALUE_TYPE_CONST_PTRTOINT, INSTRUCTION_PTRTOINT, from, to)
#define build_fptoui(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FPTOUI, INSTRUCTION_FPTOUI, from, to)
#define build_fptosi(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_FPTOSI, INSTRUCTION_FPTOSI, from, to)
#define build_uitofp(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_UITOFP, INSTRUCTION_UITOFP, from, to)
#define build_sitofp(builder, from, to)      build_cast(builder, VALUE_TYPE_CONST_SITOFP, INSTRUCTION_SITOFP, from, to)
#define build_reinterpret(builder, from, to) build_cast(builder, VALUE_TYPE_CONST_REINTERPRET, INSTRUCTION_REINTERPRET, from, to)

#define build_const_bitcast(pool, from, to)     build_const_cast(pool, VALUE_TYPE_CONST_BITCAST, from, to)
#define build_const_zext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_ZEXT, from, to)
#define build_const_sext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_SEXT, from, to)
#define build_const_trunc(pool, from, to)       build_const_cast(pool, VALUE_TYPE_CONST_TRUNC, from, to)
#define build_const_fext(pool, from, to)        build_const_cast(pool, VALUE_TYPE_CONST_FEXT, from, to)
#define build_const_ftrunc(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FTRUNC, from, to)
#define build_const_inttoptr(pool, from, to)    build_const_cast(pool, VALUE_TYPE_CONST_INTTOPTR, from, to)
#define build_const_ptrtoint(pool, from, to)    build_const_cast(pool, VALUE_TYPE_CONST_PTRTOINT, from, to)
#define build_const_fptoui(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FPTOUI, from, to)
#define build_const_fptosi(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_FPTOSI, from, to)
#define build_const_uitofp(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_UITOFP, from, to)
#define build_const_sitofp(pool, from, to)      build_const_cast(pool, VALUE_TYPE_CONST_SITOFP, from, to)
#define build_const_reinterpret(pool, from, to) build_const_cast(pool, VALUE_TYPE_CONST_REINTERPRET, from, to)

// ---------------- build_alloc ----------------
// Allocates space on the stack for a variable of a type
// NOTE: This is only used for dynamic allocations w/ STACK_SAVE and STACK_RESTORE
ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type);
ir_value_t *build_alloc_array(ir_builder_t *builder, ir_type_t *type, ir_value_t *count);
ir_value_t *build_alloc_aligned(ir_builder_t *builder, ir_type_t *type, unsigned int alignment);

// ---------------- build_stack_restore ----------------
// Saves the current position of the stack by returning the stack pointer
ir_value_t *build_stack_save(ir_builder_t *builder);

// ---------------- build_stack_restore ----------------
// Restores the stack to a previous position
void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer);

// ---------------- build_math ----------------
// Builds a basic math instruction
ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result);

// ---------------- build_phi2 ----------------
// Builds a PHI2 instruction
ir_value_t *build_phi2(ir_builder_t *builder, ir_type_t *result_type, ir_value_t *a, ir_value_t *b, length_t landing_a_block_id, length_t landing_b_block_id);

// ---------------- build_bool ----------------
// Builds a literal boolean value
ir_value_t *build_bool(ir_pool_t *pool, bool value);

// ---------------- build_rtti_relocation ----------------
// Adds an RTTI index that requires resolution to the rtti_relocations array
// NOTE: Despite its name, this function does not add any instructions,
//       it simply marks an RTTI index to be filled in later
errorcode_t build_rtti_relocation(ir_builder_t *builder, strong_cstr_t human_notation, adept_usize *id_ref, source_t source_on_failure);

// ---------------- build_llvm_asm ----------------
// Builds an inline assembly instruction
void build_llvm_asm(ir_builder_t *builder, bool is_intel, weak_cstr_t assembly, weak_cstr_t constraints, ir_value_t **args, length_t arity, bool has_side_effects, bool is_stack_align);

// ---------------- build_deinit_svars ----------------
// Builds an instruction that will handle the deinitialization
// of all static variables
// NOTE: Most of the time, 'build_main_deinitialization'
// should be used instead of this function, since
// it covers all main-related deinitialization routines
void build_deinit_svars(ir_builder_t *builder);

// ---------------- build_unreachable ----------------
// Builds an instruction that indicates an unreachable code path
void build_unreachable(ir_builder_t *builder);

// ---------------- build_main_deinitialization ----------------
// Builds all main-related deinitialization routines
void build_main_deinitialization(ir_builder_t *builder);

// ---------------- prepare_for_new_label ----------------
// Ensures there's enough room for another label
void prepare_for_new_label(ir_builder_t *builder);

// ---------------- open_scope ----------------
// Opens a new scope
void open_scope(ir_builder_t *builder);

// ---------------- close_scope ----------------
// Closes the current scope
void close_scope(ir_builder_t *builder);

// ---------------- push_loop_label ----------------
// Pushes a block label off of the block label stack
void push_loop_label(ir_builder_t *builder, weak_cstr_t label, length_t break_basicblock_id, length_t continue_basicblock_id);

// ---------------- pop_loop_label ----------------
// Pops a block label off of the block label stack
void pop_loop_label(ir_builder_t *builder);

// ---------------- add_variable ----------------
// Adds a variable to the current bridge_scope_t
// Returns a temporary pointer to the constructed variable
bridge_var_t *add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits);

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
errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value);

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
// NOTE: Returns SUCCESS if value was utilized in deference
//       Returns FAILURE if value was not utilized in deference
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
ir_value_t *handle_math_management(ir_builder_t *builder, ir_value_t *lhs, ir_value_t *rhs,
    ast_type_t *lhs_type, ast_type_t *rhs_type, source_t from_source, ast_type_t *out_type, weak_cstr_t overload_name);

// ---------------- handle_access_management ----------------
// Handles '__access__' management function calls for [] operator
// NOTE: Returns SUCCESS if nothing went wrong
// NOTE: Returns FAILURE if compile time error occurred
ir_value_t *handle_access_management(ir_builder_t *builder, ir_value_t *array_mutable_struct_value, ir_value_t *index_value,
    ast_type_t *array_type, ast_type_t *index_type, ast_type_t *out_ptr_to_element_type);

// ---------------- instantiate_poly_func ----------------
// Instantiates a polymorphic function
// NOTE: 'instantiation_source' may be NULL_SOURCE
errorcode_t instantiate_poly_func(compiler_t *compiler, object_t *object, source_t instantiation_source, funcid_t ast_poly_func_id, ast_type_t *types,
        length_t types_list_length, ast_poly_catalog_t *catalog, ir_func_endpoint_t *out_endpoint);

// ---------------- attempt_autogen___defer__ ----------------
// Attempts to auto-generate __defer__ management method
// NOTE: Does NOT check for existing suitable __defer__ methods
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___defer__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_funcpair_t *result);

// ---------------- attempt_autogen___pass__ ----------------
// Attempts to auto-generate __pass__ management function
// NOTE: Does NOT check for existing suitable __pass__ functions
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___pass__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_funcpair_t *result);

// ---------------- attempt_autogen___assign__ ----------------
// Attempts to auto-generate __assign__ management method
// NOTE: Does NOT check for existing suitable __assign__ methods
// NOTE: Returns FAILURE if couldn't auto generate
// NOTE: Returns ALT_FAILURE if something went really wrong
errorcode_t attempt_autogen___assign__(compiler_t *compiler, object_t *object, ast_type_t *arg_types, length_t type_list_length, optional_funcpair_t *result);

// ---------------- resolve_type_polymorphics ----------------
// Resolves any polymorphic type variables within an AST type
// NOTE: Will show error messages on failure
// NOTE: in_type == out_type is allowed
// NOTE: out_type is same as in_type if out_type == null
// NOTE: Will also give result to type_table if not NULL and !(compiler->traits & COMPILER_NO_TYPEINFO)
errorcode_t resolve_type_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type);

// ---------------- resolve_expr_polymorphics ----------------
// Resolves any polymorphic type variables within an AST expression
errorcode_t resolve_expr_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_t *expr);
errorcode_t resolve_exprs_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_t **exprs, length_t count);
errorcode_t resolve_expr_list_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_list_t *exprs);

// ---------------- is_allowed_builtin_auto_conversion ----------------
// Returns whether a builtin auto conversion is allowed
// (For integers / floats)
bool is_allowed_builtin_auto_conversion(compiler_t *compiler, object_t *object, const ast_type_t *a_type, const ast_type_t *b_type);

// ---------------- ir_builder_get_loop_label_info ----------------
// Gets information associated with a loop label
successful_t ir_builder_get_loop_label_info(ir_builder_t *builder, const char *label, bridge_scope_t **out_scope, length_t *out_break_block_id, length_t *out_continue_block_id);

// ---------------- ir_builder_get_noop_defer_func ----------------
// Gets no-op defer function
// Will create if doesn't already exist
errorcode_t ir_builder_get_noop_defer_func(ir_builder_t *builder, source_t source_on_error, funcid_t *out_ir_func_id);

// ---------------- instructions_snapshot_t ----------------
// Snapshot used to easily reset the forward generation of IR instructions
typedef struct {
    length_t current_block_id;
    length_t current_basicblock_instructions_length;
    length_t basicblocks_length;
} instructions_snapshot_t;

void instructions_snapshot_capture(ir_builder_t *builder, instructions_snapshot_t *snapshot);
void instructions_snapshot_restore(ir_builder_t *builder, instructions_snapshot_t *snapshot);

#endif // _ISAAC_IR_BUILDER_H
