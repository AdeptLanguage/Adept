
#ifndef IR_BUILDER_H
#define IR_BUILDER_H

/*
    =============================== ir_builder.h ===============================
    Module for building intermediate representation
    ----------------------------------------------------------------------------
*/

#include "IR/ir.h"
#include "UTIL/ground.h"
#include "DRVR/object.h"
#include "DRVR/compiler.h"
#include "BRIDGE/bridge.h"
#include "BRIDGE/funcpair.h"

// ---------------- ir_builder_t ----------------
// Container for storing general information
// about the building state
typedef struct {
    ir_basicblock_t *basicblocks;
    length_t basicblocks_length;
    length_t basicblocks_capacity;
    ir_basicblock_t *current_block;
    length_t current_block_id;
    length_t break_block_id; // 0 == none
    length_t continue_block_id; // 0 == none
    length_t fallthrough_block_id; // 0 == none
    bridge_scope_t *break_continue_scope;
    bridge_scope_t *fallthrough_scope;
    strong_cstr_t *block_stack_labels;
    length_t *block_stack_break_ids;
    length_t *block_stack_continue_ids;
    bridge_scope_t **block_stack_scopes;
    length_t block_stack_length;
    length_t block_stack_capacity;
    ir_pool_t *pool;
    ir_type_map_t *type_map;
    compiler_t *compiler;
    object_t *object;
    length_t ast_func_id;
    length_t ir_func_id;
    bridge_scope_t *scope;
    length_t next_var_id;
    length_t *next_reference_id;
    troolean has_string_struct;
    ir_jobs_t *jobs;
    ast_type_t static_bool;
    ast_elem_base_t static_bool_base;
    ast_elem_t *static_bool_elems;
    ir_type_t *stack_pointer_type;
    type_table_t *type_table;
} ir_builder_t;

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
// Builds an IR value from the result of the previsous instruction
ir_value_t *build_value_from_prev_instruction(ir_builder_t *builder);

// ---------------- build_varptr ----------------
// Builds a varptr instruction
ir_value_t *build_varptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_varptr ----------------
// Builds a globalvarptr instruction
ir_value_t *build_gvarptr(ir_builder_t *builder, ir_type_t *ptr_type, length_t variable_id);

// ---------------- build_load ----------------
// Builds a load instruction
ir_value_t *build_load(ir_builder_t *builder, ir_value_t *value, source_t code_source);

// ---------------- build_store ----------------
// Builds a store instruction
void build_store(ir_builder_t *builder, ir_value_t *value, ir_value_t *destination, source_t code_source);

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

// ---------------- build_static_array ----------------
// Builds a static array
ir_value_t *build_static_array(ir_pool_t *pool, ir_type_t *type, ir_value_t **values, length_t length);

// ---------------- build_anon_global ----------------
// Builds an anonymous global variable
ir_value_t *build_anon_global(ir_module_t *module, ir_type_t *type, bool is_constant);

// ---------------- build_anon_global_initializer ----------------
// Builds an anonymous global variable initializer
void build_anon_global_initializer(ir_module_t *module, ir_value_t *anon_global, ir_value_t *initializer);

// ---------------- ir_builder_funcptr ----------------
// Gets a shared IR function pointer type
ir_type_t *ir_builder_funcptr(ir_builder_t *builder);

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
ir_value_t *build_literal_int(ir_pool_t *pool, long long value);

// ---------------- build_literal_usize ----------------
// Builds a literal usize value
ir_value_t *build_literal_usize(ir_pool_t *pool, length_t value);

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
ir_value_t *build_literal_cstr_of_length_ex(ir_pool_t *pool, ir_type_map_t *type_map, weak_cstr_t value, length_t length);

// ---------------- build_null_pointer ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer(ir_pool_t *pool);

// ---------------- build_null_pointer_of_type ----------------
// Builds a literal null pointer value
ir_value_t *build_null_pointer_of_type(ir_pool_t *pool, ir_type_t *type);

// ---------------- build_bitcast ----------------
// Builds a bitcast instruction
ir_value_t *build_bitcast(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);

// ---------------- build_const_bitcast ----------------
// Builds a const bitcast value
ir_value_t *build_const_bitcast(ir_pool_t *pool, ir_value_t *from, ir_type_t *to);

// ---------------- build_<cast> ----------------
// Builds a specialized value cast
ir_value_t *build_zext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_trunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_fext(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_ftrunc(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_inttoptr(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_ptrtoint(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_fptoui(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_fptosi(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_uitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);
ir_value_t *build_sitofp(ir_builder_t *builder, ir_value_t *from, ir_type_t *to);

// ---------------- build_alloc ----------------
// Allocates space on the stack for a variable of a type
// NOTE: This is only used for dynamic allocations w/ STACK_SAVE and STACK_RESTORE
ir_value_t *build_alloc(ir_builder_t *builder, ir_type_t *type);

// ---------------- build_stack_restore ----------------
// Saves the current position of the stack by returning the stack pointer
ir_value_t *build_stack_save(ir_builder_t *builder);

// ---------------- build_stack_restore ----------------
// Restores the stack to a previous position
void build_stack_restore(ir_builder_t *builder, ir_value_t *stack_pointer);

// ---------------- build_math ----------------
// Builds a basic math instruction
ir_value_t *build_math(ir_builder_t *builder, unsigned int instr_id, ir_value_t *a, ir_value_t *b, ir_type_t *result);

// ---------------- build_bool ----------------
// Builds a literal boolean value
ir_value_t *build_bool(ir_pool_t *pool, bool value);

// ---------------- build_rtti_relocation ----------------
// Adds an RTTI index that requires resolution to the rtti_relocations array
// NOTE: Despite its name, this function does not add any instructions,
//       it simply marks an RTTI index to be filled in later
errorcode_t build_rtti_relocation(ir_builder_t *builder, strong_cstr_t human_notation, unsigned long long *id_ref, source_t source_on_failure);

// ---------------- prepare_for_new_label ----------------
// Ensures there's enough room for another label
void prepare_for_new_label(ir_builder_t *builder);

// ---------------- open_scope ----------------
// Opens a new scope
void open_scope(ir_builder_t *builder);

// ---------------- close_scope ----------------
// Closes the current scope
void close_scope(ir_builder_t *builder);

// ---------------- add_variable ----------------
// Adds a variable to the current bridge_scope_t
void add_variable(ir_builder_t *builder, weak_cstr_t name, ast_type_t *ast_type, ir_type_t *ir_type, trait_t traits);

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
//       Returns ALT_FAILURE if a compile time error occured
errorcode_t handle_single_deference(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value);

errorcode_t handle_children_deference(ir_builder_t *builder);

// ---------------- could_have_deference ----------------
// Returns whether a type could have __defer__ methods that need calling
bool could_have_deference(ast_type_t *ast_type);

// ---------------- handle_pass_management ----------------
// Handles '__pass__' management function calls for passing arguments
// NOTE: 'arg_type_traits' can be NULL
// NOTE: Returns SUCCESS if nothing went wrong
// NOTE: Returns FAILURE if compile time error occured
errorcode_t handle_pass_management(ir_builder_t *builder, ir_value_t **values, ast_type_t *types, trait_t *arg_type_traits, length_t arity);

// ---------------- handle_single_deference ----------------
// Calls __pass__ function for a value and it's children if the function exists
// NOTE: Assumes (ast_type->elements_length == 1)
// NOTE: Returns SUCCESS if value was utilized in passing
//       Returns FAILURE if value was not utilized in passing
//       Returns ALT_FAILURE if a compiler time error occured
errorcode_t handle_single_pass(ir_builder_t *builder, ast_type_t *ast_type, ir_value_t *mutable_value);

errorcode_t handle_children_pass_root(ir_builder_t *builder, bool already_has_return);
errorcode_t handle_children_pass(ir_builder_t *builder);

// ---------------- could_have_pass ----------------
// Returns whether a type could have __pass__ methods that need calling
bool could_have_pass(ast_type_t *ast_type);

// ---------------- handle_assign_management ----------------
// Handles '__assign__' management method calls
// NOTE: 'ast_destination_type' is not a pointer, but value provided is mutable
successful_t handle_assign_management(ir_builder_t *builder, ir_value_t *value, ast_type_t *ast_value_type, ir_value_t *destination,
    ast_type_t *ast_destination_type, bool zero_initialize);

// ---------------- handle_math_management ----------------
// Handles basic math management function calls
ir_value_t *handle_math_management(ir_builder_t *builder, ir_value_t *lhs, ir_value_t *rhs,
    ast_type_t *lhs_type, ast_type_t *rhs_type, ast_type_t *out_type, const char *overload_name);

// ---------------- instantiate_polymorphic_func ----------------
// Instantiates a polymorphic function
errorcode_t instantiate_polymorphic_func(ir_builder_t *builder, ast_func_t *poly_func, ast_type_t *types,
    length_t types_length, ast_type_var_catalog_t *catalog, ir_func_mapping_t *out_mapping);

// ---------------- attempt_autogen___defer__ ----------------
// Attempts to auto-generate __defer__ management method
// NOTE: Does NOT check for existing suitable __defer__ methods
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___defer__(ir_builder_t *builder, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result);

// ---------------- attempt_autogen___pass__ ----------------
// Attempts to auto-generate __pass__ management function
// NOTE: Does NOT check for existing suitable __pass__ functions
// NOTE: Returns FAILURE if couldn't auto generate
errorcode_t attempt_autogen___pass__(ir_builder_t *builder, ast_type_t *arg_types,
        length_t type_list_length, funcpair_t *result);

// ---------------- resolve_type_polymorphics ----------------
// Resolves any polymorphic type variables within an AST type
// NOTE: Will show error messages on failure
// NOTE: in_type == out_type is allowed
// NOTE: out_type is same as in_type if out_type == null
// NOTE: Will also give result to type_table if not NULL and !(compiler->traits & COMPILER_NO_TYPEINFO)
errorcode_t resolve_type_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_type_var_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type);

// ---------------- resolve_expr_polymorphics ----------------
// Resovles any polymorphic type variables within an AST expression
errorcode_t resolve_expr_polymorphics(compiler_t *compiler, type_table_t *type_table, ast_type_var_catalog_t *catalog, ast_expr_t *expr);

#endif // IR_BUILDER_H
