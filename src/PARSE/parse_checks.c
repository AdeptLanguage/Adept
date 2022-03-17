
#include <stdbool.h>
#include <stdlib.h>

#include "AST/TYPE/ast_type_identical.h"
#include "AST/ast.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "PARSE/parse_checks.h"
#include "PARSE/parse_ctx.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/trait.h"

#define doesnt_use_forbidden_traits(traits, forbidden) (((traits) & (forbidden)) == TRAIT_NONE)
static bool is_valid_method(ast_func_t *func);
static bool is_math_func(const char *func_name);

errorcode_t validate_func_requirements(parse_ctx_t *ctx, ast_func_t *func, source_t source){
    if(func->traits & AST_FUNC_DEFER){
        if(
            func->arity == 1
            && is_valid_method(func)
            && ast_type_is_void(&func->return_type)
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management method __defer__ must be declared as 'func __defer__(this *T) void'");
            return FAILURE;
        }
    }

    if(func->traits & AST_FUNC_PASS){
        if(
            func->arity == 1
            && ast_types_identical(&func->return_type, &func->arg_types[0])
            && func->arg_type_traits[0] == AST_FUNC_ARG_TYPE_TRAIT_POD
            && (
                ast_type_is_base(&func->return_type)
                || ast_type_is_polymorph(&func->return_type)
                || ast_type_is_generic_base(&func->return_type)
                || ast_type_is_fixed_array(&func->return_type)
            )
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management function __pass__ must be declared as 'func __pass__(value POD T) T'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__assign__")){
        if(
            func->arity == 2
            && ast_type_is_void(&func->return_type)
            && is_valid_method(func)
            && ast_type_is_pointer_to(&func->arg_types[0], &func->arg_types[1])
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management method __assign__ must be declared like 'func __assign__(this *T, other T) void'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__access__")){
        if(
            func->arity == 2
            && is_valid_method(func)
            && ast_type_is_pointer(&func->return_type)
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management method __access__ must be declared like '__access__(this *T, index $Key) *$Value'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__array__")){
        if(
            func->arity == 1
            && is_valid_method(func)
            && ast_type_is_pointer(&func->return_type)
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management method __array__ must be declared like '__array__(this *T) *$ArrayElementType'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__length__")){
        if(
            func->arity == 1
            && is_valid_method(func)
            && ast_type_is_base_of(&func->return_type, "usize")
            && doesnt_use_forbidden_traits(func->traits, AST_FUNC_FOREIGN)
        ){
            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Management method __length__ must be declared like '__length__(this *T) usize'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__variadic_array__")){
        // Don't allow multiple User-Defined Variadic Array Types
        if(ctx->ast->common.ast_variadic_array != NULL){
            compiler_panic(ctx->compiler, source, "Special function __variadic_array__ can only be defined once");
            compiler_panic(ctx->compiler, ctx->ast->common.ast_variadic_source, "Previous definition");
            return FAILURE;
        }

        // Must return what the User-Defined Variadic Array Type will be
        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, source, "Special function __variadic_array__ must return a value");
            return FAILURE;
        }

        if(
            func->arity == 4
            && ast_type_is_base_of(&func->arg_types[0], "ptr")
            && ast_type_is_base_of(&func->arg_types[1], "usize")
            && ast_type_is_base_of(&func->arg_types[2], "usize")
            && ast_type_is_base_of(&func->arg_types[3], "ptr")
            && func->arg_type_traits[0] == TRAIT_NONE
            && func->arg_type_traits[1] == TRAIT_NONE
            && func->arg_type_traits[2] == TRAIT_NONE
            && func->arg_type_traits[3] == TRAIT_NONE
        ){
            // Cache User-Defined Variadic Array Type
            if(ctx->ast->common.ast_variadic_array == NULL){
                ctx->ast->common.ast_variadic_array = malloc(sizeof(ast_type_t));
                *ctx->ast->common.ast_variadic_array = ast_type_clone(&func->return_type);
                ctx->ast->common.ast_variadic_source = func->source;
            }

            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Special function __variadic_array__ must be declared like:\n'__variadic_array__(pointer ptr, bytes usize, length usize, maybe_types ptr) ReturnType'");
            return FAILURE;
        }
    }

    if(streq(func->name, "__initializer_list__")){
        // Must return what the User-Defined Variadic InitialierList Type will be
        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, source, "Special function __initializer_list__ must return a value");
            return FAILURE;
        }

        if(
            func->arity == 2
            && ast_type_is_polymorph_ptr(&func->arg_types[0])
            && ast_type_is_base_of(&func->arg_types[1], "usize")
            && func->arg_type_traits[0] == TRAIT_NONE
            && func->arg_type_traits[1] == TRAIT_NONE
        ){
            // Cache User-Defined InitializerList Type
            if(ctx->ast->common.ast_initializer_list == NULL){
                ctx->ast->common.ast_initializer_list = malloc(sizeof(ast_type_t));
                *ctx->ast->common.ast_initializer_list = ast_type_clone(&func->return_type);
                ctx->ast->common.ast_initializer_list_source = func->source;
            }

            return SUCCESS;
        } else {
            compiler_panic(ctx->compiler, source, "Special function __initializer_list__ must be declared like:\n'__initializer_list__(array *$T, length usize) <$T> ReturnType'");
            return FAILURE;
        }
    }
    
    if(is_math_func(func->name)){
        // Must take two arguments
        if(func->arity != 2){
            compiler_panicf(ctx->compiler, source, "Math function %s must take two arguments", func->name);
            return FAILURE;
        }

        // First argument cannot be a pointer
        if(ast_type_is_pointer(&func->arg_types[0])){
            compiler_panicf(ctx->compiler, source, "Math function %s cannot have a pointer as its first argument", func->name);
            return FAILURE;
        }
    }

    return SUCCESS;
}

static bool is_valid_method(ast_func_t *func){
    return func->arity > 0
        && streq(func->arg_names[0], "this")
        && (
               ast_type_is_base_ptr(&func->arg_types[0])
            || ast_type_is_polymorph_ptr(&func->arg_types[0])
            || ast_type_is_generic_base_ptr(&func->arg_types[0])
        )
        && func->arg_type_traits[0] == TRAIT_NONE;
}

static bool is_math_func(const char *func_name){
    static const char *sorted[] = {
        "__add__",
        "__divide__",
        "__equals__",
        "__greater_than__",
        "__greater_than_or_equal__",
        "__less_than__",
        "__less_than_or_equal__",
        "__modulus__",
        "__multiply__",
        "__not_equals__",
        "__subtract__"
    };

    return (binary_string_search(sorted, sizeof sorted / sizeof *sorted, func_name) != -1);
}
