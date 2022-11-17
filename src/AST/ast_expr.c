
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

ast_expr_t *ast_expr_clone(ast_expr_t* expr){
    ast_expr_t *clone = NULL;

    #define MACRO_VALUE_CLONE(expr_type) { \
        clone = malloc(sizeof(expr_type)); \
        *((expr_type*) clone) = *((expr_type*) expr); \
    }

    switch(expr->id){
    case EXPR_BYTE:          MACRO_VALUE_CLONE(ast_expr_byte_t); break;
    case EXPR_UBYTE:         MACRO_VALUE_CLONE(ast_expr_ubyte_t); break;
    case EXPR_SHORT:         MACRO_VALUE_CLONE(ast_expr_short_t); break;
    case EXPR_USHORT:        MACRO_VALUE_CLONE(ast_expr_ushort_t); break;
    case EXPR_INT:           MACRO_VALUE_CLONE(ast_expr_int_t); break;
    case EXPR_UINT:          MACRO_VALUE_CLONE(ast_expr_uint_t); break;
    case EXPR_LONG:          MACRO_VALUE_CLONE(ast_expr_long_t); break;
    case EXPR_ULONG:         MACRO_VALUE_CLONE(ast_expr_ulong_t); break;
    case EXPR_USIZE:         MACRO_VALUE_CLONE(ast_expr_usize_t); break;
    case EXPR_FLOAT:         MACRO_VALUE_CLONE(ast_expr_float_t); break;
    case EXPR_DOUBLE:        MACRO_VALUE_CLONE(ast_expr_double_t); break;
    case EXPR_BOOLEAN:       MACRO_VALUE_CLONE(ast_expr_boolean_t); break;
    case EXPR_GENERIC_INT:   MACRO_VALUE_CLONE(ast_expr_generic_int_t); break;
    case EXPR_GENERIC_FLOAT: MACRO_VALUE_CLONE(ast_expr_generic_float_t); break;
    case EXPR_CSTR:          MACRO_VALUE_CLONE(ast_expr_cstr_t); break;
    case EXPR_NULL:
    case EXPR_BREAK:
    case EXPR_CONTINUE:
    case EXPR_FALLTHROUGH:
        clone = malloc(sizeof(ast_expr_t));
        break;
    case EXPR_STR:
        #define expr_as_str ((ast_expr_str_t*) expr)
        #define clone_as_str ((ast_expr_str_t*) clone)

        clone = malloc(sizeof(ast_expr_str_t));
        clone_as_str->array = expr_as_str->array;
        clone_as_str->length = expr_as_str->length;
        break;

        #undef expr_as_str
        #undef clone_as_str
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
    case EXPR_BIT_LGC_RSHIFT:
        #define expr_as_math ((ast_expr_math_t*) expr)
        #define clone_as_math ((ast_expr_math_t*) clone)

        clone = malloc(sizeof(ast_expr_math_t));
        clone_as_math->a = ast_expr_clone(expr_as_math->a);
        clone_as_math->b = ast_expr_clone(expr_as_math->b);
        break;

        #undef expr_as_math
        #undef clone_as_math
    case EXPR_CALL:
        #define expr_as_call ((ast_expr_call_t*) expr)
        #define clone_as_call ((ast_expr_call_t*) clone)

        clone = malloc(sizeof(ast_expr_call_t));
        clone_as_call->name = strclone(expr_as_call->name);
        clone_as_call->args = malloc(sizeof(ast_expr_t*) * expr_as_call->arity);
        clone_as_call->arity = expr_as_call->arity;
        clone_as_call->is_tentative = expr_as_call->is_tentative;
        clone_as_call->only_implicit = expr_as_call->only_implicit;
        clone_as_call->no_user_casts = expr_as_call->no_user_casts;

        for(length_t i = 0; i != expr_as_call->arity; i++){
            clone_as_call->args[i] = ast_expr_clone(expr_as_call->args[i]);
        }

        if(expr_as_call->gives.elements_length != 0){
            clone_as_call->gives = ast_type_clone(&expr_as_call->gives);
        } else {
            memset(&clone_as_call->gives, 0, sizeof(ast_type_t));
        }
        break;

        #undef expr_as_call
        #undef clone_as_call
    case EXPR_VARIABLE:
        #define expr_as_variable ((ast_expr_variable_t*) expr)
        #define clone_as_variable ((ast_expr_variable_t*) clone)

        clone = malloc(sizeof(ast_expr_variable_t));
        clone_as_variable->name = expr_as_variable->name;
        break;

        #undef expr_as_variable
        #undef clone_as_variable
    case EXPR_MEMBER:
        #define expr_as_member ((ast_expr_member_t*) expr)
        #define clone_as_member ((ast_expr_member_t*) clone)
        
        clone = malloc(sizeof(ast_expr_member_t));
        clone_as_member->value = ast_expr_clone(expr_as_member->value);
        clone_as_member->member = malloc(strlen(expr_as_member->member) + 1);
        strcpy(clone_as_member->member, expr_as_member->member);
        break;

        #undef expr_as_member    
        #undef clone_as_member
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
    case EXPR_VA_END:
        #define expr_as_unary ((ast_expr_unary_t*) expr)
        #define clone_as_unary ((ast_expr_unary_t*) clone)

        clone = malloc(sizeof(ast_expr_unary_t));
        clone_as_unary->value = ast_expr_clone(expr_as_unary->value);
        break;

        #undef expr_as_unary
        #undef clone_as_unary
    case EXPR_FUNC_ADDR:
        #define expr_as_func_addr ((ast_expr_func_addr_t*) expr)
        #define clone_as_func_addr ((ast_expr_func_addr_t*) clone)

        clone = malloc(sizeof(ast_expr_func_addr_t));
        clone_as_func_addr->name = expr_as_func_addr->name;
        clone_as_func_addr->traits = expr_as_func_addr->traits;
        clone_as_func_addr->match_args_length = expr_as_func_addr->match_args_length;
        clone_as_func_addr->tentative = expr_as_func_addr->tentative;
        clone_as_func_addr->has_match_args = expr_as_func_addr->has_match_args;
        
        if(expr_as_func_addr->match_args == NULL){
            clone_as_func_addr->match_args = NULL;
        } else {
            clone_as_func_addr->match_args = malloc(sizeof(ast_unnamed_arg_t) * expr_as_func_addr->match_args_length);

            for(length_t a = 0; a != clone_as_func_addr->match_args_length; a++){
                clone_as_func_addr->match_args[a] = ast_type_clone(&expr_as_func_addr->match_args[a]);
            }
        }
        break;

        #undef expr_as_func_addr
        #undef clone_as_func_addr
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        #define expr_as_access ((ast_expr_array_access_t*) expr)
        #define clone_as_access ((ast_expr_array_access_t*) clone)

        clone = malloc(sizeof(ast_expr_array_access_t));
        clone_as_access->value = ast_expr_clone(expr_as_access->value);
        clone_as_access->index = ast_expr_clone(expr_as_access->index);
        break;

        #undef expr_as_access
        #undef clone_as_access
    case EXPR_CAST:
        #define expr_as_cast ((ast_expr_cast_t*) expr)
        #define clone_as_cast ((ast_expr_cast_t*) clone)

        clone = malloc(sizeof(ast_expr_cast_t));
        clone_as_cast->from = ast_expr_clone(expr_as_cast->from);
        clone_as_cast->to = ast_type_clone(&expr_as_cast->to);
        break;

        #undef expr_as_cast
        #undef clone_as_cast
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_TYPEINFO:
    case EXPR_TYPENAMEOF:
        #define expr_as_unary_type_expr ((ast_expr_unary_type_t*) expr)
        #define clone_as_unary_type_expr ((ast_expr_unary_type_t*) clone)

        clone = malloc(sizeof(ast_expr_unary_type_t));
        clone_as_unary_type_expr->type = ast_type_clone(&expr_as_unary_type_expr->type);
        break;

        #undef expr_as_unary_type_expr
        #undef clone_as_unary_type_expr
    case EXPR_SIZEOF_VALUE:
        #define expr_as_sizeof_value ((ast_expr_sizeof_value_t*) expr)
        #define clone_as_sizeof_value ((ast_expr_sizeof_value_t*) clone)

        clone = malloc(sizeof(ast_expr_sizeof_value_t));
        clone_as_sizeof_value->value = ast_expr_clone(expr_as_sizeof_value->value);
        break;

        #undef expr_as_sizeof_value
        #undef clone_as_sizeof_value
    case EXPR_PHANTOM:
        #define expr_as_phantom ((ast_expr_phantom_t*) expr)
        #define clone_as_phantom ((ast_expr_phantom_t*) clone)

        clone = malloc(sizeof(ast_expr_phantom_t));
        clone_as_phantom->type = ast_type_clone(&expr_as_phantom->type);
        clone_as_phantom->ir_value = expr_as_phantom->ir_value;
        break;

        #undef expr_as_phantom
        #undef clone_as_phantom
    case EXPR_CALL_METHOD:
        #define expr_as_call_method ((ast_expr_call_method_t*) expr)
        #define clone_as_call_method ((ast_expr_call_method_t*) clone)

        clone = malloc(sizeof(ast_expr_call_method_t));
        clone_as_call_method->name = strclone(expr_as_call_method->name);
        clone_as_call_method->args = malloc(sizeof(ast_expr_t*) * expr_as_call_method->arity);
        clone_as_call_method->arity = expr_as_call_method->arity;
        clone_as_call_method->value = ast_expr_clone(expr_as_call_method->value);
        clone_as_call_method->is_tentative = expr_as_call_method->is_tentative;
        clone_as_call_method->allow_drop = expr_as_call_method->allow_drop;
        for(length_t i = 0; i != expr_as_call_method->arity; i++){
            clone_as_call_method->args[i] = ast_expr_clone(expr_as_call_method->args[i]);
        }

        if(expr_as_call_method->gives.elements_length != 0){
            clone_as_call_method->gives = ast_type_clone(&expr_as_call_method->gives);
        } else {
            memset(&clone_as_call_method->gives, 0, sizeof(ast_type_t));
        }

        break;
        
        #undef expr_as_call_method
        #undef clone_as_call_method
    case EXPR_NEW:
        #define expr_as_new ((ast_expr_new_t*) expr)
        #define clone_as_new ((ast_expr_new_t*) clone)

        clone = malloc(sizeof(ast_expr_new_t));
        clone_as_new->type = ast_type_clone(&expr_as_new->type);
        clone_as_new->amount = expr_as_new->amount ? ast_expr_clone(expr_as_new->amount) : NULL;
        clone_as_new->is_undef = expr_as_new->is_undef;
        clone_as_new->inputs = optional_ast_expr_list_clone(&expr_as_new->inputs);

        break;

        #undef expr_as_new
        #undef clone_as_new
    case EXPR_NEW_CSTRING:
        #define expr_as_new_cstring ((ast_expr_new_cstring_t*) expr)
        #define clone_as_new_cstring ((ast_expr_new_cstring_t*) clone)

        clone = malloc(sizeof(ast_expr_new_cstring_t));
        clone_as_new_cstring->value = expr_as_new_cstring->value;
        break;

        #undef expr_as_new_cstring
        #undef clone_as_new_cstring
    case EXPR_STATIC_ARRAY: case EXPR_STATIC_STRUCT:
        #define expr_as_static_data ((ast_expr_static_data_t*) expr)
        #define clone_as_static_data ((ast_expr_static_data_t*) clone)

        clone = malloc(sizeof(ast_expr_static_data_t));
        clone_as_static_data->type = ast_type_clone(&expr_as_static_data->type);
        clone_as_static_data->values = malloc(sizeof(ast_expr_t*) * expr_as_static_data->length);
        clone_as_static_data->length = expr_as_static_data->length;

        for(length_t i = 0; i != expr_as_static_data->length; i++){
            clone_as_static_data->values[i] = ast_expr_clone(expr_as_static_data->values[i]);
        }
        break;

        #undef expr_as_static_data
        #undef clone_as_static_data
    case EXPR_ENUM_VALUE:
        #define expr_as_enum_value ((ast_expr_enum_value_t*) expr)
        #define clone_as_enum_value ((ast_expr_enum_value_t*) clone)

        clone = malloc(sizeof(ast_expr_enum_value_t));
        clone_as_enum_value->enum_name = expr_as_enum_value->enum_name;
        clone_as_enum_value->kind_name = expr_as_enum_value->kind_name;
        break;

        #undef expr_as_enum_value
        #undef clone_as_enum_value
    case EXPR_TERNARY:
        #define expr_as_ternary ((ast_expr_ternary_t*) expr)
        #define clone_as_ternary ((ast_expr_ternary_t*) clone)

        clone = malloc(sizeof(ast_expr_ternary_t));
        clone_as_ternary->condition = ast_expr_clone(expr_as_ternary->condition);
        clone_as_ternary->if_true = ast_expr_clone(expr_as_ternary->if_true);
        clone_as_ternary->if_false = ast_expr_clone(expr_as_ternary->if_false);
        break;

        #undef expr_as_ternary
        #undef clone_as_ternary
    case EXPR_VA_ARG:
        #define expr_as_va_arg ((ast_expr_va_arg_t*) expr)
        #define clone_as_va_arg ((ast_expr_va_arg_t*) clone)

        clone = malloc(sizeof(ast_expr_va_arg_t));
        clone_as_va_arg->va_list = ast_expr_clone(expr_as_va_arg->va_list);
        clone_as_va_arg->arg_type = ast_type_clone(&expr_as_va_arg->arg_type);
        break;

        #undef expr_as_va_arg
        #undef clone_as_va_arg
    case EXPR_INITLIST:
        #define expr_as_initlist ((ast_expr_initlist_t*) expr)
        #define clone_as_initlist ((ast_expr_initlist_t*) clone)

        clone = malloc(sizeof(ast_expr_initlist_t));
        clone_as_initlist->elements = malloc(sizeof(ast_expr_t*) * expr_as_initlist->length);
        clone_as_initlist->length = expr_as_initlist->length;

        for(length_t i = 0; i != expr_as_initlist->length; i++){
            clone_as_initlist->elements[i] = ast_expr_clone(expr_as_initlist->elements[i]);
        }

        break;

        #undef expr_as_initlist
        #undef clone_as_initlist
    case EXPR_POLYCOUNT:
        #define expr_as_polycount ((ast_expr_polycount_t*) expr)
        #define clone_as_polycount ((ast_expr_polycount_t*) clone)

        clone = malloc(sizeof(ast_expr_polycount_t));
        clone_as_polycount->name = strclone(expr_as_polycount->name);
        break;

        #undef expr_as_polycount
        #undef clone_as_polycount
    case EXPR_LLVM_ASM:
        #define expr_as_llvm_asm ((ast_expr_llvm_asm_t*) expr)
        #define clone_as_llvm_asm ((ast_expr_llvm_asm_t*) clone)

        clone = malloc(sizeof(ast_expr_llvm_asm_t));
        clone_as_llvm_asm->assembly = strclone(expr_as_llvm_asm->assembly);
        clone_as_llvm_asm->constraints = expr_as_llvm_asm->constraints;
        clone_as_llvm_asm->args = malloc(sizeof(ast_expr_t*) * expr_as_llvm_asm->arity);
        clone_as_llvm_asm->arity = expr_as_llvm_asm->arity;
        clone_as_llvm_asm->has_side_effects = expr_as_llvm_asm->has_side_effects;
        clone_as_llvm_asm->is_stack_align =expr_as_llvm_asm->is_stack_align;
        clone_as_llvm_asm->is_intel = expr_as_llvm_asm->is_intel;

        for(length_t i = 0; i != expr_as_llvm_asm->arity; i++){
            clone_as_llvm_asm->args[i] = ast_expr_clone(expr_as_llvm_asm->args[i]);
        }
        break;

        #undef expr_as_llvm_asm
        #undef clone_as_llvm_asm
    case EXPR_EMBED:
        #define expr_as_embed ((ast_expr_embed_t*) expr)
        #define clone_as_embed ((ast_expr_embed_t*) clone)

        clone = malloc(sizeof(ast_expr_embed_t));
        clone_as_embed->filename = strclone(expr_as_embed->filename);
        break;

        #undef expr_as_embed
        #undef clone_as_embed
    case EXPR_DECLARE: case EXPR_DECLAREUNDEF:
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF:
        #define expr_as_declare ((ast_expr_declare_t*) expr)
        #define clone_as_declare ((ast_expr_declare_t*) clone)

        clone = malloc(sizeof(ast_expr_declare_t));
        clone_as_declare->name = expr_as_declare->name;
        clone_as_declare->type = ast_type_clone(&expr_as_declare->type);
        clone_as_declare->value = expr_as_declare->value ? ast_expr_clone(expr_as_declare->value) : NULL;
        clone_as_declare->traits = expr_as_declare->traits;
        clone_as_declare->inputs = optional_ast_expr_list_clone(&expr_as_declare->inputs);
        break;

        #undef expr_as_declare
        #undef clone_as_declare
    case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
    case EXPR_LSHIFT_ASSIGN: case EXPR_RSHIFT_ASSIGN:
    case EXPR_LGC_LSHIFT_ASSIGN: case EXPR_LGC_RSHIFT_ASSIGN:
        #define expr_as_assign ((ast_expr_assign_t*) expr)
        #define clone_as_assign ((ast_expr_assign_t*) clone)

        clone = malloc(sizeof(ast_expr_assign_t));
        clone_as_assign->destination = expr_as_assign->destination ? ast_expr_clone(expr_as_assign->destination) : NULL;
        clone_as_assign->value = expr_as_assign->value ? ast_expr_clone(expr_as_assign->value) : NULL;
        clone_as_assign->is_pod = expr_as_assign->is_pod;
        break;

        #undef expr_as_assign
        #undef clone_as_assign
    case EXPR_RETURN:
        #define expr_as_return ((ast_expr_return_t*) expr)
        #define clone_as_return ((ast_expr_return_t*) clone)

        clone = malloc(sizeof(ast_expr_return_t));
        clone_as_return->value = expr_as_return->value ? ast_expr_clone(expr_as_return->value) : NULL;
        clone_as_return->last_minute = ast_expr_list_clone(&expr_as_return->last_minute);
        break;

        #undef expr_as_return
        #undef clone_as_return
    case EXPR_IF:
    case EXPR_UNLESS:
    case EXPR_WHILE:
    case EXPR_UNTIL:
    case EXPR_WHILECONTINUE:
    case EXPR_UNTILBREAK:
        #define expr_as_if ((ast_expr_if_t*) expr)
        #define clone_as_if ((ast_expr_if_t*) clone)

        clone = malloc(sizeof(ast_expr_if_t));
        clone_as_if->label = expr_as_if->label;
        clone_as_if->value = expr_as_if->value ? ast_expr_clone(expr_as_if->value) : NULL;
        clone_as_if->statements = ast_expr_list_clone(&expr_as_if->statements);
        break;
        
        #undef expr_as_if
        #undef clone_as_if
    case EXPR_IFELSE:
    case EXPR_UNLESSELSE:
        #define expr_as_ifelse ((ast_expr_ifelse_t*) expr)
        #define clone_as_ifelse ((ast_expr_ifelse_t*) clone)

        clone = malloc(sizeof(ast_expr_ifelse_t));
        clone_as_ifelse->label = expr_as_ifelse->label;
        clone_as_ifelse->value = ast_expr_clone(expr_as_ifelse->value);
        clone_as_ifelse->statements = ast_expr_list_clone(&expr_as_ifelse->statements);
        clone_as_ifelse->else_statements = ast_expr_list_clone(&expr_as_ifelse->else_statements);
        break;
        
        #undef expr_as_ifelse
        #undef clone_as_ifelse
    case EXPR_EACH_IN:
        #define expr_as_each_in ((ast_expr_each_in_t*) expr)
        #define clone_as_each_in ((ast_expr_each_in_t*) clone)

        clone = malloc(sizeof(ast_expr_each_in_t));
        clone_as_each_in->label = expr_as_each_in->label;
        clone_as_each_in->it_name = expr_as_each_in->it_name ? strclone(expr_as_each_in->it_name) : NULL;

        if(expr_as_each_in->it_type){
            clone_as_each_in->it_type = malloc(sizeof(ast_type_t));
            *clone_as_each_in->it_type = ast_type_clone(expr_as_each_in->it_type);
        } else {
            clone_as_each_in->it_type = NULL;
        }

        clone_as_each_in->low_array = expr_as_each_in->low_array ? ast_expr_clone(expr_as_each_in->low_array) : NULL;
        clone_as_each_in->length = expr_as_each_in->length ? ast_expr_clone(expr_as_each_in->length) : NULL;
        clone_as_each_in->list = expr_as_each_in->list ? ast_expr_clone(expr_as_each_in->list) : NULL;
        clone_as_each_in->statements = ast_expr_list_clone(&expr_as_each_in->statements);
        clone_as_each_in->is_static = expr_as_each_in->is_static;
        break;

        #undef expr_as_each_in
        #undef clone_as_each_in
    case EXPR_REPEAT:
        #define expr_as_repeat ((ast_expr_repeat_t*) expr)
        #define clone_as_repeat ((ast_expr_repeat_t*) clone)

        clone = malloc(sizeof(ast_expr_repeat_t));
        clone_as_repeat->label = expr_as_repeat->label;
        clone_as_repeat->limit = expr_as_repeat->limit ? ast_expr_clone(expr_as_repeat->limit) : NULL;
        clone_as_repeat->statements = ast_expr_list_clone(&expr_as_repeat->statements);
        clone_as_repeat->is_static = expr_as_repeat->is_static;
        clone_as_repeat->idx_overload_name = expr_as_repeat->idx_overload_name;
        break;

        #undef expr_as_repeat
        #undef clone_as_repeat
    case EXPR_BREAK_TO:
    case EXPR_CONTINUE_TO:
        #define expr_as_break_to ((ast_expr_break_to_t*) expr)
        #define clone_as_break_to ((ast_expr_break_to_t*) clone)

        clone = malloc(sizeof(ast_expr_break_to_t));
        clone_as_break_to->label_source = expr_as_break_to->label_source;
        clone_as_break_to->label = expr_as_break_to->label;
        break;

        #undef expr_as_break_to
        #undef clone_as_break_to
    case EXPR_SWITCH:
        #define expr_as_switch ((ast_expr_switch_t*) expr)
        #define clone_as_switch ((ast_expr_switch_t*) clone)

        clone = malloc(sizeof(ast_expr_switch_t));
        clone_as_switch->value = ast_expr_clone(expr_as_switch->value);
        clone_as_switch->cases = ast_case_list_clone(&expr_as_switch->cases);
        clone_as_switch->or_default = ast_expr_list_clone(&expr_as_switch->or_default);
        clone_as_switch->is_exhaustive = expr_as_switch->is_exhaustive;
        break;

        #undef expr_as_switch
        #undef clone_as_switch
    case EXPR_VA_COPY:
        #define expr_as_va_copy ((ast_expr_va_copy_t*) expr)
        #define clone_as_va_copy ((ast_expr_va_copy_t*) clone)

        clone = malloc(sizeof(ast_expr_va_copy_t));
        clone_as_va_copy->src_value = ast_expr_clone(expr_as_va_copy->src_value);
        clone_as_va_copy->dest_value = ast_expr_clone(expr_as_va_copy->dest_value);
        break;

        #undef expr_as_va_copy
        #undef clone_as_va_copy
    case EXPR_FOR:
        #define expr_as_for ((ast_expr_for_t*) expr)
        #define clone_as_for ((ast_expr_for_t*) clone)

        clone = malloc(sizeof(ast_expr_for_t));
        clone_as_for->label = expr_as_for->label;
        clone_as_for->condition = expr_as_for->condition ? ast_expr_clone(expr_as_for->condition) : NULL;
        clone_as_for->before = ast_expr_list_clone(&expr_as_for->before);
        clone_as_for->after = ast_expr_list_clone(&expr_as_for->after);
        clone_as_for->statements = ast_expr_list_clone(&expr_as_for->statements);
        break;

        #undef expr_as_for
        #undef clone_as_for
    case EXPR_DECLARE_NAMED_EXPRESSION:
        #define expr_as_declare_named_expression ((ast_expr_declare_named_expression_t*) expr)
        #define clone_as_declare_named_expression ((ast_expr_declare_named_expression_t*) clone)

        clone = malloc(sizeof(ast_expr_declare_named_expression_t));
        clone_as_declare_named_expression->named_expression = ast_named_expression_clone(&expr_as_declare_named_expression->named_expression);
        break;

        #undef expr_as_declare_named_expression
        #undef clone_as_declare_named_expression
    case EXPR_CONDITIONLESS_BLOCK:
        #define expr_as_block ((ast_expr_conditionless_block_t*) expr)
        #define clone_as_block ((ast_expr_conditionless_block_t*) clone)

        clone = malloc(sizeof(ast_expr_conditionless_block_t));
        clone_as_block->statements = ast_expr_list_clone(&expr_as_block->statements);
        break;
        
        #undef expr_as_conditionless_block
        #undef clone_as_conditionless_block
    default:
        die("ast_expr_clone() - Got unrecognized expression ID 0x%08X\n", expr->id);
    }

    clone->id = expr->id;
    clone->source = expr->source;
    return clone;
}

ast_expr_t *ast_expr_create_bool(adept_bool value, source_t source){
    ast_expr_boolean_t *expr = malloc(sizeof(ast_expr_boolean_t));

    *expr = (ast_expr_boolean_t){
        .id = EXPR_BOOLEAN,
        .source = source,
        .value = value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_long(adept_long value, source_t source){
    ast_expr_long_t *expr = malloc(sizeof(ast_expr_long_t));

    *expr = (ast_expr_long_t){
        .id = EXPR_LONG,
        .source = source,
        .value = value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_double(adept_double value, source_t source){
    ast_expr_double_t *expr = malloc(sizeof(ast_expr_double_t));

    *expr = (ast_expr_double_t){
        .id = EXPR_DOUBLE,
        .source = source,
        .value = value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_string(char *array, length_t length, source_t source){
    ast_expr_str_t *expr = malloc(sizeof(ast_expr_str_t));

    *expr = (ast_expr_str_t){
        .id = EXPR_STR,
        .source = source,
        .array = array,
        .length = length,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_cstring(char *value, source_t source){
    ast_expr_cstr_t *expr = malloc(sizeof(ast_expr_cstr_t));
    
    *expr = (ast_expr_cstr_t){
        .id = EXPR_CSTR,
        .source = source,
        .value = value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_null(source_t source){
    ast_expr_t *expr = malloc(sizeof(ast_expr_null_t));

    *expr = (ast_expr_null_t){
        .id = EXPR_NULL,
        .source = source,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_variable(weak_cstr_t name, source_t source){
    ast_expr_variable_t *expr = malloc(sizeof(ast_expr_variable_t));

    *expr = (ast_expr_variable_t){
        .id = EXPR_VARIABLE,
        .source = source,
        .name = name,
    };

    return (ast_expr_t*) expr;
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
    };
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
    ast_expr_ternary_t *expr = malloc(sizeof(ast_expr_ternary_t));
    
    *expr = (ast_expr_ternary_t){
        .id = EXPR_TERNARY,
        .source = source,
        .condition = condition,
        .if_true = if_true,
        .if_false = if_false,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_cast(ast_type_t to, ast_expr_t *from, source_t source){
    ast_expr_cast_t *expr = malloc(sizeof(ast_expr_cast_t));
    
    *expr = (ast_expr_cast_t){
        .id = EXPR_CAST,
        .to = to,
        .from = from,
        .source = source,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_phantom(ast_type_t ast_type, void *ir_value, source_t source, bool is_mutable){
    ast_expr_phantom_t *expr = malloc(sizeof(ast_expr_phantom_t));

    *expr = (ast_expr_phantom_t){
        .id = EXPR_PHANTOM,
        .source = source,
        .type = ast_type,
        .ir_value = ir_value,
        .is_mutable = is_mutable,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_typenameof(ast_type_t strong_type, source_t source){
    ast_expr_typenameof_t *expr = malloc(sizeof(ast_expr_typenameof_t));
    
    *expr = (ast_expr_typenameof_t){
        .id = EXPR_TYPENAMEOF,
        .source = source,
        .type = strong_type,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_embed(strong_cstr_t filename, source_t source){
    ast_expr_embed_t *expr = malloc(sizeof(ast_expr_embed_t));
    
    *expr = (ast_expr_embed_t){
        .id = EXPR_EMBED,
        .source = source,
        .filename = filename,
    };

    return (ast_expr_t*) expr;
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
    ast_expr_declare_t *expr = malloc(sizeof(ast_expr_declare_t));
    
    *expr = (ast_expr_declare_t){
        .id = expr_id,
        .source = source,
        .name = name,
        .traits = traits,
        .type = type,
        .value = value,
        .inputs = inputs,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_assignment(unsigned int stmt_id, source_t source, ast_expr_t *mutable_expression, ast_expr_t *value, bool is_pod){
    ast_expr_assign_t *expr = malloc(sizeof(ast_expr_assign_t));
    
    *expr = (ast_expr_assign_t){
        .id = stmt_id,
        .source = source,
        .destination = mutable_expression,
        .value = value,
        .is_pod = is_pod,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_return(source_t source, ast_expr_t *value, ast_expr_list_t last_minute){
    ast_expr_return_t *expr = malloc(sizeof(ast_expr_return_t));
    
    *expr = (ast_expr_return_t){
        .id = EXPR_RETURN,
        .source = source,
        .value = value,
        .last_minute = last_minute,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_member(ast_expr_t *value, strong_cstr_t member_name, source_t source){
    ast_expr_member_t *expr = malloc(sizeof(ast_expr_member_t));
    
    *expr = (ast_expr_member_t){
        .id = EXPR_MEMBER,
        .value = value,
        .member = member_name,
        .source = source,
    };

    return (ast_expr_t*) expr;
}
                
ast_expr_t *ast_expr_create_access(ast_expr_t *value, ast_expr_t *index, source_t source){
    ast_expr_array_access_t *expr = malloc(sizeof(ast_expr_array_access_t));
    
    *expr = (ast_expr_array_access_t){
        .id = EXPR_ARRAY_ACCESS,
        .source = source,
        .value = value,
        .index = index,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_va_arg(source_t source, ast_expr_t *va_list_value, ast_type_t arg_type){
    ast_expr_va_arg_t *expr = malloc(sizeof(ast_expr_va_arg_t));
    
    *expr = (ast_expr_va_arg_t){
        .id = EXPR_VA_ARG,
        .source = source,
        .va_list = va_list_value,
        .arg_type = arg_type,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_polycount(source_t source, strong_cstr_t name){
    ast_expr_polycount_t *expr = malloc(sizeof(ast_expr_polycount_t));

    *expr = (ast_expr_polycount_t){
        .id = EXPR_POLYCOUNT,
        .source = source,
        .name = name,
    };
    
    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_va_copy(source_t source, ast_expr_t *dest_value, ast_expr_t *src_value){
    ast_expr_va_copy_t *expr = malloc(sizeof(ast_expr_va_copy_t));
    
    *expr = (ast_expr_va_copy_t){
        .id = EXPR_VA_COPY,
        .source = source,
        .dest_value = dest_value,
        .src_value = src_value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_for(source_t source, weak_cstr_t label, ast_expr_list_t before, ast_expr_list_t after, ast_expr_t *condition, ast_expr_list_t statements){
    ast_expr_for_t *expr = malloc(sizeof(ast_expr_for_t));

    *expr = (ast_expr_for_t){
        .id = EXPR_FOR,
        .source = source,
        .label = label,
        .before = before,
        .after = after,
        .condition = condition,
        .statements = statements,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_unary(unsigned int expr_id, source_t source, ast_expr_t *value){
    ast_expr_unary_t *expr = malloc(sizeof(ast_expr_unary_t));
    
    *expr = (ast_expr_unary_t){
        .id = expr_id,
        .source = source,
        .value = value,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_initlist(source_t source, ast_expr_t **values, length_t length){
    ast_expr_initlist_t *expr = malloc(sizeof(ast_expr_initlist_t));

    *expr = (ast_expr_initlist_t){
        .id = EXPR_INITLIST,
        .source = source,
        .elements = values,
        .length = length,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_math(source_t source, unsigned int expr_id, ast_expr_t *left, ast_expr_t *right){
    ast_expr_math_t *expr = malloc(sizeof(ast_expr_math_t));
    
    *expr = (ast_expr_math_t){
        .id = expr_id,
        .source = source,
        .a = left,
        .b = right,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_switch(source_t source, ast_expr_t *value, ast_case_list_t cases, ast_expr_list_t or_default, bool is_exhaustive){
    ast_expr_switch_t *expr = malloc(sizeof(ast_expr_switch_t));
    
    *expr = (ast_expr_switch_t){
        .id = EXPR_SWITCH,
        .source = source,
        .value = value,
        .cases = cases,
        .or_default = or_default,
        .is_exhaustive = is_exhaustive,
    };

    return (ast_expr_t*) expr;
}

ast_expr_t *ast_expr_create_declare_named_expression(source_t source, ast_named_expression_t named_expression){
    ast_expr_declare_named_expression_t *expr = malloc(sizeof(ast_expr_declare_named_expression_t));

    *expr = (ast_expr_declare_named_expression_t){
        .id = EXPR_DECLARE_NAMED_EXPRESSION,
        .source = source,
        .named_expression = named_expression,
    };

    return (ast_expr_t*) expr;
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
