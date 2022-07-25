
#ifndef _ISAAC_IR_PROC_QUERY_H
#define _ISAAC_IR_PROC_QUERY_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ============================= ir_proc_query.h =============================
    Module for search queries used to location procedures
    ---------------------------------------------------------------------------
*/

#include <stdbool.h>

#include "AST/ast_type_lean.h"
#include "DRVR/compiler.h"
#include "DRVR/object.h"
#include "IR/ir_value.h"
#include "IRGEN/ir_builder.h"
#include "UTIL/ground.h"
#include "UTIL/trait.h"

struct ir_proc_query;

// ---------------- ir_proc_query_t ----------------
typedef struct ir_proc_query ir_proc_query_t;

void ir_proc_query_init_find_func_regular(
    ir_proc_query_t *out_query,
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t name,
    ast_type_t *arg_types,
    length_t length,
    trait_t traits_mask,
    trait_t traits_match,
    trait_t forbid_traits,
    source_t from_source
);

void ir_proc_query_init_find_method_regular(
    ir_proc_query_t *out_query,
    compiler_t *compiler,
    object_t *object,
    weak_cstr_t struct_name, 
    weak_cstr_t name,
    ast_type_t *arg_types,
    length_t length,
    trait_t forbid_traits,
    source_t from_source
);

void ir_proc_query_init_find_func_conforming(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    trait_t forbid_traits,
    source_t from_source
);

void ir_proc_query_init_find_method_conforming(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t ***inout_arg_values,
    ast_type_t **inout_arg_types,
    length_t *inout_length,
    ast_type_t *optional_gives,
    trait_t forbid_traits,
    source_t from_source
);

void ir_proc_query_init_find_func_conforming_without_defaults(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    bool no_user_casts,
    trait_t forbid_traits,
    source_t from_source
);

void ir_proc_query_init_find_method_conforming_without_defaults(
    ir_proc_query_t *out_query,
    ir_builder_t *builder,
    weak_cstr_t struct_name,
    weak_cstr_t name,
    ir_value_t **arg_values,
    ast_type_t *arg_types,
    length_t length,
    ast_type_t *optional_gives,
    trait_t forbid_traits,
    source_t from_source
);

// Getters for non-trivial physical and virtual values of a procedure search query
compiler_t *ir_proc_query_getter_compiler(ir_proc_query_t *query);
object_t *ir_proc_query_getter_object(ir_proc_query_t *query);
bool ir_proc_query_is_function(ir_proc_query_t *query);
bool ir_proc_query_is_method(ir_proc_query_t *query);
ir_value_t **ir_proc_query_getter_arg_values(ir_proc_query_t *query);
ast_type_t *ir_proc_query_getter_arg_types(ir_proc_query_t *query);
length_t ir_proc_query_getter_length(ir_proc_query_t *query);

// ---------------- Non-user internal types ----------------

typedef struct {
    compiler_t *compiler;
    object_t *object;
} ir_proc_query_rigid_params_t;

typedef struct {
    ir_builder_t *builder;
    bool no_user_casts;
    bool allow_default_values;
} ir_proc_query_conform_params_t;

typedef struct {
    ir_value_t ***inout_arg_values; // (nullable)
    ast_type_t **inout_arg_types;
    length_t *inout_length;
} ir_proc_query_with_defaults_t;

typedef struct {
    ir_value_t **arg_values; // (nullable)
    ast_type_t *arg_types;
    length_t length;
} ir_proc_query_without_defaults_t;

// ---------------- struct ir_proc_query ----------------
// Implementation of a procedure search query
// Never create without using one of the ir_proc_query_init_* functions
struct ir_proc_query {
    bool conform;
    union {
        ir_proc_query_rigid_params_t rigid_params;
        ir_proc_query_conform_params_t conform_params;
    };
    bool allow_default_values;
    union {
        ir_proc_query_with_defaults_t with_defaults;
        ir_proc_query_without_defaults_t without_defaults;
    };
    weak_cstr_t proc_name;
    maybe_null_weak_cstr_t struct_name;
    trait_t traits_mask;
    trait_t traits_match;
    trait_t forbid_traits;
    ast_type_t *optional_gives;
    source_t from_source;
    bool for_dispatcher;
};

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_IR_PROC_QUERY_H
