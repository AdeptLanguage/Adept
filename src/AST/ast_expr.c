
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_expr.h"
#include "AST/ast_named_expression.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

bool expr_is_mutable(ast_expr_t *expr){
    switch(expr->id){
    case EXPR_VARIABLE:
    case EXPR_MEMBER:
    case EXPR_DEREFERENCE:
    case EXPR_ARRAY_ACCESS:
        return true;
    case EXPR_TERNARY:
        return expr_is_mutable(((ast_expr_ternary_t*) expr)->if_true) && expr_is_mutable(((ast_expr_ternary_t*) expr)->if_false);
    case EXPR_POSTINCREMENT: case EXPR_POSTDECREMENT:
        return expr_is_mutable(((ast_expr_unary_t*) expr)->value);
    case EXPR_PHANTOM:
        return ((ast_expr_phantom_t*) expr)->is_mutable;
    default:
        return false;
    }
}

void ast_expr_free_fully(ast_expr_t *expr){
    ast_expr_free(expr);
    free(expr);
}

void ast_exprs_free(ast_expr_t **exprs, length_t length){
    for(length_t i = 0; i != length; i++){
        ast_expr_free(exprs[i]);
    }
}

void ast_exprs_free_fully(ast_expr_t **exprs, length_t length){
    for(length_t i = 0; i != length; i++){
        ast_expr_free_fully(exprs[i]);
    }
    free(exprs);
}

ast_expr_t *ast_expr_clone_if_not_null(ast_expr_t *expr){
    return expr ? ast_expr_clone(expr) : NULL;
}

extern inline ast_expr_t **ast_exprs_clone(ast_expr_t **exprs, length_t arity);

ast_expr_t *ast_expr_clone(ast_expr_t* expr){
    switch(expr->id){
    case EXPR_NULL:
    case EXPR_BREAK:
    case EXPR_CONTINUE:
    case EXPR_FALLTHROUGH:
        return memclone(expr, sizeof(ast_expr_t));
    case EXPR_BYTE:
        return memclone(expr, sizeof(ast_expr_byte_t));
    case EXPR_UBYTE:
        return memclone(expr, sizeof(ast_expr_ubyte_t));
    case EXPR_SHORT:
        return memclone(expr, sizeof(ast_expr_short_t));
    case EXPR_USHORT:
        return memclone(expr, sizeof(ast_expr_ushort_t));
    case EXPR_INT:
        return memclone(expr, sizeof(ast_expr_int_t));
    case EXPR_UINT:
        return memclone(expr, sizeof(ast_expr_uint_t));
    case EXPR_LONG:
        return memclone(expr, sizeof(ast_expr_long_t));
    case EXPR_ULONG:
        return memclone(expr, sizeof(ast_expr_ulong_t));
    case EXPR_USIZE:
        return memclone(expr, sizeof(ast_expr_usize_t));
    case EXPR_FLOAT:
        return memclone(expr, sizeof(ast_expr_float_t));
    case EXPR_DOUBLE:
        return memclone(expr, sizeof(ast_expr_double_t));
    case EXPR_BOOLEAN:
        return memclone(expr, sizeof(ast_expr_boolean_t));
    case EXPR_GENERIC_INT:
        return memclone(expr, sizeof(ast_expr_generic_int_t));
    case EXPR_GENERIC_FLOAT:
        return memclone(expr, sizeof(ast_expr_generic_float_t));
    case EXPR_CSTR:
        return memclone(expr, sizeof(ast_expr_cstr_t));
    case EXPR_STR:
        return memclone(expr, sizeof(ast_expr_str_t));
    case EXPR_ADD:
    case EXPR_SUBTRACT:
    case EXPR_MULTIPLY:
    case EXPR_DIVIDE:
    case EXPR_MODULUS:
    case EXPR_EQUALS:
    case EXPR_NOTEQUALS:
    case EXPR_GREATER:
    case EXPR_LESSER:
    case EXPR_GREATEREQ:
    case EXPR_LESSEREQ:
    case EXPR_AND:
    case EXPR_OR:
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
    case EXPR_BIT_LGC_LSHIFT:
    case EXPR_BIT_LGC_RSHIFT: {
            ast_expr_math_t *original = (ast_expr_math_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_math_t, {
                .id = original->id,
                .source = original->source,
                .a = ast_expr_clone(original->a),
                .b = ast_expr_clone(original->b),
            });
        }
    case EXPR_CALL: {
            ast_expr_call_t *original = (ast_expr_call_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_call_t, {
                .id = original->id,
                .source = original->source,
                .name = strclone(original->name),
                .args = ast_exprs_clone(original->args, original->arity),
                .arity = original->arity,
                .is_tentative = original->is_tentative,
                .only_implicit = original->only_implicit,
                .no_user_casts = original->no_user_casts,
                .gives = AST_TYPE_IS_NONE(original->gives) ? AST_TYPE_NONE : ast_type_clone(&original->gives),
            });
        }
    case EXPR_SUPER: {
            ast_expr_super_t *original = (ast_expr_super_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_super_t, {
                .id = original->id,
                .source = original->source,
                .args = ast_exprs_clone(original->args, original->arity),
                .arity = original->arity,
                .is_tentative = original->is_tentative,
            });
        }
    case EXPR_VARIABLE: {
            ast_expr_variable_t *original = (ast_expr_variable_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_variable_t, {
                .id = original->id,
                .source = original->source,
                .name = original->name,
            });
        }
    case EXPR_MEMBER: {
            ast_expr_member_t *original = (ast_expr_member_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_member_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone(original->value),
                .member = strclone(original->member),
            });
        }
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NOT:
    case EXPR_NEGATE:
    case EXPR_DELETE:
    case EXPR_PREINCREMENT:
    case EXPR_PREDECREMENT:
    case EXPR_POSTINCREMENT:
    case EXPR_POSTDECREMENT:
    case EXPR_TOGGLE:
    case EXPR_VA_START:
    case EXPR_VA_END: {
            ast_expr_unary_t *original = (ast_expr_unary_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_unary_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone(original->value),
            });
        }
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *original = (ast_expr_func_addr_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_func_addr_t, {
                .id = original->id,
                .source = original->source,
                .name = original->name,
                .traits = original->traits,
                .match_args_length = original->match_args_length,
                .tentative = original->tentative,
                .has_match_args = original->has_match_args,
                .match_args = ast_types_clone(original->match_args, original->match_args_length),
            });
        }
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS: {
            ast_expr_array_access_t *original = (ast_expr_array_access_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_array_access_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone(original->value),
                .index = ast_expr_clone(original->index),
            });
        }
    case EXPR_CAST: {
            ast_expr_cast_t *original = (ast_expr_cast_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_cast_t, {
                .id = original->id,
                .source = original->source,
                .from = ast_expr_clone(original->from),
                .to = ast_type_clone(&original->to),
            });
        }
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_TYPEINFO:
    case EXPR_TYPENAMEOF: {
            ast_expr_unary_type_t *original = (ast_expr_unary_type_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_unary_type_t, {
                .id = original->id,
                .source = original->source,
                .type = ast_type_clone(&original->type),
            });
        }
    case EXPR_SIZEOF_VALUE: {
            ast_expr_sizeof_value_t *original = (ast_expr_sizeof_value_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_sizeof_value_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone(original->value),
            });
        }
    case EXPR_PHANTOM: {
            ast_expr_phantom_t *original = (ast_expr_phantom_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_phantom_t, {
                .id = original->id,
                .source = original->source,
                .type = ast_type_clone(&original->type),
                .ir_value = original->ir_value,
            });
        }
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *original = (ast_expr_call_method_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_call_method_t, {
                .id = original->id,
                .source = original->source,
                .name = strclone(original->name),
                .args = ast_exprs_clone(original->args, original->arity),
                .arity = original->arity,
                .value = ast_expr_clone(original->value),
                .is_tentative = original->is_tentative,
                .allow_drop = original->allow_drop,
                .gives = AST_TYPE_IS_NONE(original->gives) ? AST_TYPE_NONE : ast_type_clone(&original->gives),
            });
        }
    case EXPR_NEW: {
            ast_expr_new_t *original = (ast_expr_new_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_new_t, {
                .id = original->id,
                .source = original->source,
                .type = ast_type_clone(&original->type),
                .amount = ast_expr_clone_if_not_null(original->amount),
                .is_undef = original->is_undef,
                .inputs = optional_ast_expr_list_clone(&original->inputs),
            });
        }
    case EXPR_NEW_CSTRING:
        return memclone(expr, sizeof(ast_expr_new_cstring_t));
    case EXPR_STATIC_ARRAY:
    case EXPR_STATIC_STRUCT: {
            ast_expr_static_data_t *original = (ast_expr_static_data_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_static_data_t, {
                .id = original->id,
                .source = original->source,
                .type = ast_type_clone(&original->type),
                .values = ast_exprs_clone(original->values, original->length),
                .length = original->length,
            });
        }
        break;
    case EXPR_ENUM_VALUE:
        return memclone(expr, sizeof(ast_expr_enum_value_t));
    case EXPR_GENERIC_ENUM_VALUE:
        return memclone(expr, sizeof(ast_expr_generic_enum_value_t));
    case EXPR_TERNARY: {
            ast_expr_ternary_t *original = (ast_expr_ternary_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_ternary_t, {
                .id = original->id,
                .source = original->source,
                .condition = ast_expr_clone(original->condition),
                .if_true = ast_expr_clone(original->if_true),
                .if_false = ast_expr_clone(original->if_false),
            });
        }
    case EXPR_VA_ARG: {
            ast_expr_va_arg_t *original = (ast_expr_va_arg_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_va_arg_t, {
                .id = original->id,
                .source = original->source,
                .va_list = ast_expr_clone(original->va_list),
                .arg_type = ast_type_clone(&original->arg_type),
            });
        }
    case EXPR_INITLIST: {
            ast_expr_initlist_t *original = (ast_expr_initlist_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_initlist_t, {
                .id = original->id,
                .source = original->source,
                .elements = ast_exprs_clone(original->elements, original->length),
                .length = original->length,
            });
        }
    case EXPR_POLYCOUNT: {
            ast_expr_polycount_t *original = (ast_expr_polycount_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_polycount_t, {
                .id = original->id,
                .source = original->source,
                .name = strclone(original->name),
            });
        }
    case EXPR_LLVM_ASM: {
            ast_expr_llvm_asm_t *original = (ast_expr_llvm_asm_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_llvm_asm_t, {
                .id = original->id,
                .source = original->source,
                .assembly = strclone(original->assembly),
                .constraints = original->constraints,
                .args = ast_exprs_clone(original->args, original->arity),
                .arity = original->arity,
                .has_side_effects = original->has_side_effects,
                .is_stack_align = original->is_stack_align,
                .is_intel = original->is_intel,
            });
        }
    case EXPR_EMBED: {
            ast_expr_embed_t *original = (ast_expr_embed_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_embed_t, {
                .id = original->id,
                .source = original->source,
                .filename = strclone(original->filename),
            });
        }
    case EXPR_DECLARE:
    case EXPR_DECLAREUNDEF:
    case EXPR_ILDECLARE:
    case EXPR_ILDECLAREUNDEF: {
            ast_expr_declare_t *original = (ast_expr_declare_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_declare_t, {
                .id = original->id,
                .source = original->source,
                .name = original->name,
                .type = ast_type_clone(&original->type),
                .value = ast_expr_clone_if_not_null(original->value),
                .traits = original->traits,
                .inputs = optional_ast_expr_list_clone(&original->inputs),
            });
        }
    case EXPR_ASSIGN:
    case EXPR_ADD_ASSIGN:
    case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN:
    case EXPR_DIVIDE_ASSIGN:
    case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN:
    case EXPR_OR_ASSIGN:
    case EXPR_XOR_ASSIGN:
    case EXPR_LSHIFT_ASSIGN:
    case EXPR_RSHIFT_ASSIGN:
    case EXPR_LGC_LSHIFT_ASSIGN:
    case EXPR_LGC_RSHIFT_ASSIGN: {
            ast_expr_assign_t *original = (ast_expr_assign_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_assign_t, {
                .id = original->id,
                .source = original->source,
                .destination = ast_expr_clone_if_not_null(original->destination),
                .value = ast_expr_clone_if_not_null(original->value),
                .is_pod = original->is_pod,
            });
        }
    case EXPR_RETURN: {
            ast_expr_return_t *original = (ast_expr_return_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_return_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone_if_not_null(original->value),
                .last_minute = ast_expr_list_clone(&original->last_minute),
            });
        }
    case EXPR_IF:
    case EXPR_UNLESS:
    case EXPR_WHILE:
    case EXPR_UNTIL:
    case EXPR_WHILECONTINUE:
    case EXPR_UNTILBREAK: {
            ast_expr_if_t *original = (ast_expr_if_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_if_t, {
                .id = original->id,
                .source = original->source,
                .label = original->label,
                .value = ast_expr_clone_if_not_null(original->value),
                .statements = ast_expr_list_clone(&original->statements),
            });
        }
    case EXPR_IFELSE:
    case EXPR_UNLESSELSE: {
            ast_expr_ifelse_t *original = (ast_expr_ifelse_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_ifelse_t, {
                .id = original->id,
                .source = original->source,
                .label = original->label,
                .value = ast_expr_clone(original->value),
                .statements = ast_expr_list_clone(&original->statements),
                .else_statements = ast_expr_list_clone(&original->else_statements),
            });
        }
    case EXPR_EACH_IN: {
            ast_expr_each_in_t *original = (ast_expr_each_in_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_each_in_t, {
                .id = original->id,
                .source = original->source,
                .label = original->label,
                .it_name = original->it_name ? strclone(original->it_name) : NULL,
                .it_type = original->it_type ? malloc_init(ast_type_t, ast_type_clone(original->it_type)) : NULL,
                .low_array = ast_expr_clone_if_not_null(original->low_array),
                .length = ast_expr_clone_if_not_null(original->length),
                .list = ast_expr_clone_if_not_null(original->list),
                .statements = ast_expr_list_clone(&original->statements),
                .is_static = original->is_static,
            });
        }
    case EXPR_REPEAT: {
            ast_expr_repeat_t *original = (ast_expr_repeat_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_repeat_t, {
                .id = original->id,
                .source = original->source,
                .label = original->label,
                .limit = ast_expr_clone_if_not_null(original->limit),
                .statements = ast_expr_list_clone(&original->statements),
                .is_static = original->is_static,
                .idx_name = original->idx_name,
            });
        }
    case EXPR_BREAK_TO:
    case EXPR_CONTINUE_TO:
        return memclone(expr, sizeof(ast_expr_break_to_t));
    case EXPR_SWITCH: {
            ast_expr_switch_t *original = (ast_expr_switch_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_switch_t, {
                .id = original->id,
                .source = original->source,
                .value = ast_expr_clone(original->value),
                .cases = ast_case_list_clone(&original->cases),
                .or_default = ast_expr_list_clone(&original->or_default),
                .is_exhaustive = original->is_exhaustive,
            });
        }
    case EXPR_VA_COPY: {
            ast_expr_va_copy_t *original = (ast_expr_va_copy_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_va_copy_t, {
                .id = original->id,
                .source = original->source,
                .src_value = ast_expr_clone(original->src_value),
                .dest_value = ast_expr_clone(original->dest_value),
            });
        }
    case EXPR_FOR: {
            ast_expr_for_t *original = (ast_expr_for_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_for_t, {
                .id = original->id,
                .source = original->source,
                .label = original->label,
                .condition = ast_expr_clone_if_not_null(original->condition),
                .before = ast_expr_list_clone(&original->before),
                .after = ast_expr_list_clone(&original->after),
                .statements = ast_expr_list_clone(&original->statements),
            });
        }
    case EXPR_DECLARE_NAMED_EXPRESSION: {
            ast_expr_declare_named_expression_t *original = (ast_expr_declare_named_expression_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_declare_named_expression_t, {
                .id = original->id,
                .source = original->source,
                .named_expression = ast_named_expression_clone(&original->named_expression),
            });
        }
    case EXPR_CONDITIONLESS_BLOCK: {
            ast_expr_conditionless_block_t *original = (ast_expr_conditionless_block_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_conditionless_block_t, {
                .id = original->id,
                .source = original->source,
                .statements = ast_expr_list_clone(&original->statements),
            });
        }
    case EXPR_ASSERT: {
            ast_expr_assert_t *original = (ast_expr_assert_t*) expr;

            return (ast_expr_t*) malloc_init(ast_expr_assert_t, {
                .id = original->id,
                .source = original->source,
                .assertion = ast_expr_clone(original->assertion),
                .message = ast_expr_clone_if_not_null(original->message),
            });
        }
    default:
        die("ast_expr_clone() - Got unrecognized expression ID 0x%08X\n", expr->id);
        return NULL;
    }
}

ast_expr_t *ast_expr_create_bool(adept_bool value, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_boolean_t, {
        .id = EXPR_BOOLEAN,
        .source = source,
        .value = value,
    });
}

ast_expr_t *ast_expr_create_long(adept_long value, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_long_t, {
        .id = EXPR_LONG,
        .source = source,
        .value = value,
    });
}

ast_expr_t *ast_expr_create_double(adept_double value, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_double_t, {
        .id = EXPR_DOUBLE,
        .source = source,
        .value = value,
    });
}

ast_expr_t *ast_expr_create_string(char *array, length_t length, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_str_t, {
        .id = EXPR_STR,
        .source = source,
        .array = array,
        .length = length,
    });
}

ast_expr_t *ast_expr_create_cstring(char *array, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_cstr_t, {
        .id = EXPR_CSTR,
        .source = source,
        .array = array,
        .length = strlen(array),
    });
}

ast_expr_t *ast_expr_create_cstring_of_length(char *array, length_t length, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_cstr_t, {
        .id = EXPR_CSTR,
        .source = source,
        .array = array,
        .length = length,
    });
}

ast_expr_t *ast_expr_create_null(source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_null_t, {
        .id = EXPR_NULL,
        .source = source,
    });
}

ast_expr_t *ast_expr_create_variable(weak_cstr_t name, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_variable_t, {
        .id = EXPR_VARIABLE,
        .source = source,
        .name = name,
    });
}

ast_expr_t *ast_expr_create_call(strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source){
    ast_expr_call_t *expr = malloc(sizeof(ast_expr_call_t));
    ast_expr_create_call_in_place(expr, name, arity, args, is_tentative, gives, source);
    return (ast_expr_t*) expr;
}

void ast_expr_create_call_in_place(ast_expr_call_t *out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source){
    *out_expr = (ast_expr_call_t){
        .id = EXPR_CALL,
        .source = source,
        .name = name,
        .args = args,
        .arity = arity,
        .is_tentative = is_tentative,
        .gives = (gives && gives->elements_length) ? *gives : AST_TYPE_NONE,
        .only_implicit = false,
        .no_user_casts = false,
        .no_discard = false,
    };
}

ast_expr_t *ast_expr_create_super(ast_expr_t **args, length_t arity, bool is_tentative, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_super_t, {
        .id = EXPR_SUPER,
        .source = source,
        .args = args,
        .arity = arity,
        .is_tentative = is_tentative,
    });
}

ast_expr_t *ast_expr_create_call_method(strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source){
    ast_expr_call_method_t *expr = malloc(sizeof(ast_expr_call_method_t));
    ast_expr_create_call_method_in_place(expr, name, value, arity, args, is_tentative, allow_drop, gives, source);
    return (ast_expr_t*) expr;
}

void ast_expr_create_call_method_in_place(ast_expr_call_method_t *out_expr, strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source){
    *out_expr = (ast_expr_call_method_t){
        .id = EXPR_CALL_METHOD,
        .source = source,
        .name = name,
        .arity = arity,
        .args = args,
        .value = value,
        .is_tentative = is_tentative,
        .allow_drop = allow_drop,
        .gives = (gives && gives->elements_length) ? *gives : AST_TYPE_NONE,
    };
}

ast_expr_t *ast_expr_create_enum_value(weak_cstr_t name, weak_cstr_t kind, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_enum_value_t, {
        .id = EXPR_ENUM_VALUE,
        .source = source,
        .enum_name = name,
        .kind_name = kind,
    });
}

ast_expr_t *ast_expr_create_generic_enum_value(weak_cstr_t kind, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_generic_enum_value_t, {
        .id = EXPR_GENERIC_ENUM_VALUE,
        .source = source,
        .kind_name = kind,
    });
}

ast_expr_t *ast_expr_create_ternary(ast_expr_t *condition, ast_expr_t *if_true, ast_expr_t *if_false, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_ternary_t, {
        .id = EXPR_TERNARY,
        .source = source,
        .condition = condition,
        .if_true = if_true,
        .if_false = if_false,
    });
}

ast_expr_t *ast_expr_create_cast(ast_type_t to, ast_expr_t *from, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_cast_t, {
        .id = EXPR_CAST,
        .to = to,
        .from = from,
        .source = source,
    });
}

ast_expr_t *ast_expr_create_phantom(ast_type_t ast_type, void *ir_value, source_t source, bool is_mutable){
    return (ast_expr_t*) malloc_init(ast_expr_phantom_t, {
        .id = EXPR_PHANTOM,
        .source = source,
        .type = ast_type,
        .ir_value = ir_value,
        .is_mutable = is_mutable,
    });
}

ast_expr_t *ast_expr_create_typenameof(ast_type_t strong_type, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_typenameof_t, {
        .id = EXPR_TYPENAMEOF,
        .source = source,
        .type = strong_type,
    });
}

ast_expr_t *ast_expr_create_embed(strong_cstr_t filename, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_embed_t, {
        .id = EXPR_EMBED,
        .source = source,
        .filename = filename,
    });
}

ast_expr_t *ast_expr_create_declaration(
    unsigned int expr_id,
    source_t source,
    weak_cstr_t name,
    ast_type_t type,
    trait_t traits,
    ast_expr_t *value,
    optional_ast_expr_list_t inputs
){
    return (ast_expr_t*) malloc_init(ast_expr_declare_t, {
        .id = expr_id,
        .source = source,
        .name = name,
        .traits = traits,
        .type = type,
        .value = value,
        .inputs = inputs,
    });
}

ast_expr_t *ast_expr_create_assignment(unsigned int stmt_id, source_t source, ast_expr_t *mutable_expression, ast_expr_t *value, bool is_pod){
    return (ast_expr_t*) malloc_init(ast_expr_assign_t, {
        .id = stmt_id,
        .source = source,
        .destination = mutable_expression,
        .value = value,
        .is_pod = is_pod,
    });
}

ast_expr_t *ast_expr_create_return(source_t source, ast_expr_t *value, ast_expr_list_t last_minute){
    return (ast_expr_t*) malloc_init(ast_expr_return_t, {
        .id = EXPR_RETURN,
        .source = source,
        .value = value,
        .last_minute = last_minute,
    });
}

ast_expr_t *ast_expr_create_member(ast_expr_t *value, strong_cstr_t member_name, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_member_t, {
        .id = EXPR_MEMBER,
        .value = value,
        .member = member_name,
        .source = source,
    });
}
                
ast_expr_t *ast_expr_create_access(ast_expr_t *value, ast_expr_t *index, source_t source){
    return (ast_expr_t*) malloc_init(ast_expr_array_access_t, {
        .id = EXPR_ARRAY_ACCESS,
        .source = source,
        .value = value,
        .index = index,
    });
}

ast_expr_t *ast_expr_create_va_arg(source_t source, ast_expr_t *va_list_value, ast_type_t arg_type){
    return (ast_expr_t*) malloc_init(ast_expr_va_arg_t, {
        .id = EXPR_VA_ARG,
        .source = source,
        .va_list = va_list_value,
        .arg_type = arg_type,
    });
}

ast_expr_t *ast_expr_create_polycount(source_t source, strong_cstr_t name){
    return (ast_expr_t*) malloc_init(ast_expr_polycount_t, {
        .id = EXPR_POLYCOUNT,
        .source = source,
        .name = name,
    });
}

ast_expr_t *ast_expr_create_va_copy(source_t source, ast_expr_t *dest_value, ast_expr_t *src_value){
    return (ast_expr_t*) malloc_init(ast_expr_va_copy_t, {
        .id = EXPR_VA_COPY,
        .source = source,
        .dest_value = dest_value,
        .src_value = src_value,
    });
}

ast_expr_t *ast_expr_create_simple_conditional(source_t source, unsigned int conditional_type, maybe_null_weak_cstr_t label, ast_expr_t *condition, ast_expr_list_t statements){
    return (ast_expr_t*) malloc_init(ast_expr_conditional_t, {
        .id = conditional_type,
        .source = source,
        .label = label,
        .value = condition,
        .statements = statements,
    });
}

ast_expr_t *ast_expr_create_for(source_t source, weak_cstr_t label, ast_expr_list_t before, ast_expr_list_t after, ast_expr_t *condition, ast_expr_list_t statements){
    return (ast_expr_t*) malloc_init(ast_expr_for_t, {
        .id = EXPR_FOR,
        .source = source,
        .label = label,
        .before = before,
        .after = after,
        .condition = condition,
        .statements = statements,
    });
}

ast_expr_t *ast_expr_create_unary(unsigned int expr_id, source_t source, ast_expr_t *value){
    return (ast_expr_t*) malloc_init(ast_expr_unary_t, {
        .id = expr_id,
        .source = source,
        .value = value,
    });
}

ast_expr_t *ast_expr_create_initlist(source_t source, ast_expr_t **values, length_t length){
    return (ast_expr_t*) malloc_init(ast_expr_initlist_t, {
        .id = EXPR_INITLIST,
        .source = source,
        .elements = values,
        .length = length,
    });
}

ast_expr_t *ast_expr_create_math(source_t source, unsigned int expr_id, ast_expr_t *left, ast_expr_t *right){
    return (ast_expr_t*) malloc_init(ast_expr_math_t, {
        .id = expr_id,
        .source = source,
        .a = left,
        .b = right,
    });
}

ast_expr_t *ast_expr_create_switch(source_t source, ast_expr_t *value, ast_case_list_t cases, ast_expr_list_t or_default, bool is_exhaustive){
    return (ast_expr_t*) malloc_init(ast_expr_switch_t, {
        .id = EXPR_SWITCH,
        .source = source,
        .value = value,
        .cases = cases,
        .or_default = or_default,
        .is_exhaustive = is_exhaustive,
    });
}

ast_expr_t *ast_expr_create_declare_named_expression(source_t source, ast_named_expression_t named_expression){
    return (ast_expr_t*) malloc_init(ast_expr_declare_named_expression_t, {
        .id = EXPR_DECLARE_NAMED_EXPRESSION,
        .source = source,
        .named_expression = named_expression,
    });
}

ast_expr_t *ast_expr_create_assert(source_t source, ast_expr_t *assertion){
    return (ast_expr_t*) malloc_init(ast_expr_assert_t, {
        .id = EXPR_ASSERT,
        .source = source,
        .assertion = assertion,
    });
}


ast_expr_list_t ast_expr_list_create(length_t initial_capacity){
    return (ast_expr_list_t){
        .statements = initial_capacity ? malloc(sizeof(ast_expr_t*) * initial_capacity) : NULL,
        .length = 0,
        .capacity = initial_capacity,
    };
}

void ast_expr_list_free(ast_expr_list_t *list){
    ast_exprs_free_fully(list->statements, list->length);
}

void ast_expr_list_append_unchecked(ast_expr_list_t *list, ast_expr_t *expr){
    list->statements[list->length++] = expr;
}

ast_expr_list_t ast_expr_list_clone(ast_expr_list_t *list){
    ast_expr_list_t result = (ast_expr_list_t){
        .statements = malloc(sizeof(ast_expr_t*) * list->capacity),
        .length = list->length,
        .capacity = list->capacity,
    };

    for(length_t i = 0; i != result.length; i++){
        result.statements[i] = ast_expr_clone(list->statements[i]);
    }

    return result;
}

optional_ast_expr_list_t optional_ast_expr_list_clone(optional_ast_expr_list_t *list){
    if(list->has){
        return (optional_ast_expr_list_t){
            .has = true,
            .value = ast_expr_list_clone(&list->value),
        };
    } else {
        return (optional_ast_expr_list_t){
            .has = false,
        };
    }
}

errorcode_t ast_expr_deduce_to_size(ast_expr_t *expr, length_t *out_value){
    switch(expr->id){
    case EXPR_GENERIC_INT: {
            adept_generic_int value = ((ast_expr_generic_int_t*) expr)->value;
            *out_value = value < 0 ? 0 : value;
            return SUCCESS;
        }
    case EXPR_BYTE: {
            adept_byte value = ((ast_expr_byte_t*) expr)->value;
            *out_value = value < 0 ? 0 : value;
            return SUCCESS;
        }
    case EXPR_SHORT: {
            adept_short value = ((ast_expr_short_t*) expr)->value;
            *out_value = value < 0 ? 0 : value;
            return SUCCESS;
        }
    case EXPR_INT: {
            adept_int value = ((ast_expr_int_t*) expr)->value;
            *out_value = value < 0 ? 0 : value;
            return SUCCESS;
        }
    case EXPR_LONG: {
            adept_long value = ((ast_expr_long_t*) expr)->value;
            *out_value = value < 0 ? 0 : value;
            return SUCCESS;
        }
    case EXPR_UBYTE:
        *out_value = ((ast_expr_ubyte_t*) expr)->value;
        return SUCCESS;
    case EXPR_USHORT:
        *out_value = ((ast_expr_ushort_t*) expr)->value;
        return SUCCESS;
    case EXPR_UINT:
        *out_value = ((ast_expr_uint_t*) expr)->value;
        return SUCCESS;
    case EXPR_ULONG:
        *out_value = ((ast_expr_ulong_t*) expr)->value;
        return SUCCESS;
    case EXPR_USIZE:
        *out_value = ((ast_expr_usize_t*) expr)->value;
        return SUCCESS;
    case EXPR_ADD: {
            length_t a, b;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->a, &a)) return FAILURE;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->b, &b)) return FAILURE;
            *out_value = a + b;
            return SUCCESS;
        }
    case EXPR_SUBTRACT: {
            length_t a, b;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->a, &a)) return FAILURE;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->b, &b)) return FAILURE;
            *out_value = a - b;
            return SUCCESS;
        }
    case EXPR_MULTIPLY: {
            length_t a, b;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->a, &a)) return FAILURE;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->b, &b)) return FAILURE;
            *out_value = a * b;
            return SUCCESS;
        }
    case EXPR_DIVIDE: {
            length_t a, b;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->a, &a)) return FAILURE;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->b, &b)) return FAILURE;
            *out_value = a / b;
            return SUCCESS;
        }
    case EXPR_MODULUS: {
            length_t a, b;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->a, &a)) return FAILURE;
            if(ast_expr_deduce_to_size(((ast_expr_math_t*) expr)->b, &b)) return FAILURE;
            *out_value = a % b;
            return SUCCESS;
        }
    }
    return FAILURE;
}

ast_case_t ast_case_clone(ast_case_t *original){
    return (ast_case_t){
        .condition = ast_expr_clone(original->condition),
        .statements = ast_expr_list_clone(&original->statements),
        .source = original->source,
    };
}

ast_case_list_t ast_case_list_clone(ast_case_list_t *original){
    ast_case_list_t result = (ast_case_list_t){
        .cases = malloc(sizeof(ast_case_t) * original->capacity),
        .length = original->length,
        .capacity = original->capacity,
    };

    for(length_t i = 0; i != result.length; i++){
        result.cases[i] = ast_case_clone(&original->cases[i]);
    }

    return result;
}

void ast_case_list_free(ast_case_list_t *list){
    for(length_t i = 0; i != list->length; i++){
        ast_case_t *single_case = &list->cases[i];
        ast_expr_free_fully(single_case->condition);
        ast_expr_list_free(&single_case->statements);
    }

    free(list->cases);
}

ast_case_t *ast_case_list_append(ast_case_list_t *list, ast_case_t ast_case){
    expand((void**) &list->cases, sizeof(ast_case_t), list->length, &list->capacity, 1, 8);

    ast_case_t *new_case = &list->cases[list->length++];
    *new_case = ast_case;
    return new_case;
}

unsigned short from_assign[EXPR_TOTAL] = {
    [EXPR_ADD_ASSIGN] = EXPR_ADD,
    [EXPR_SUBTRACT_ASSIGN] = EXPR_ADD,
    [EXPR_MULTIPLY_ASSIGN] = EXPR_MULTIPLY,
    [EXPR_DIVIDE_ASSIGN] = EXPR_DIVIDE,
    [EXPR_MODULUS_ASSIGN] = EXPR_MODULUS,
    [EXPR_AND_ASSIGN] = EXPR_BIT_AND,
    [EXPR_OR_ASSIGN] = EXPR_BIT_OR,
    [EXPR_XOR_ASSIGN] = EXPR_BIT_XOR,
    [EXPR_LSHIFT_ASSIGN] = EXPR_BIT_LSHIFT,
    [EXPR_RSHIFT_ASSIGN] = EXPR_BIT_RSHIFT,
    [EXPR_LGC_LSHIFT_ASSIGN] = EXPR_BIT_LGC_LSHIFT,
    [EXPR_LGC_RSHIFT_ASSIGN] = EXPR_BIT_LGC_RSHIFT,
};
