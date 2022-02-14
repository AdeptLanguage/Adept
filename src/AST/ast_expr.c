
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_constant.h"
#include "AST/ast_expr.h"
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
        break;

        #undef expr_as_declare
        #undef clone_as_declare
    case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
    case EXPR_LS_ASSIGN: case EXPR_RS_ASSIGN:
    case EXPR_LGC_LS_ASSIGN: case EXPR_LGC_RS_ASSIGN:
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
    case EXPR_DECLARE_CONSTANT:
        #define expr_as_declare_constant ((ast_expr_declare_constant_t*) expr)
        #define clone_as_declare_constant ((ast_expr_declare_constant_t*) clone)

        clone = malloc(sizeof(ast_expr_declare_constant_t));
        clone_as_declare_constant->constant = ast_constant_clone(&expr_as_declare_constant->constant);
        break;

        #undef expr_as_declare_constant
        #undef clone_as_declare_constant
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

void ast_expr_create_bool(ast_expr_t **out_expr, adept_bool value, source_t source){
    *out_expr = malloc(sizeof(ast_expr_boolean_t));
    ((ast_expr_boolean_t*) *out_expr)->id = EXPR_BOOLEAN;
    ((ast_expr_boolean_t*) *out_expr)->value = value;
    ((ast_expr_boolean_t*) *out_expr)->source = source;
}

void ast_expr_create_long(ast_expr_t **out_expr, adept_long value, source_t source){
    *out_expr = malloc(sizeof(ast_expr_long_t));
    ((ast_expr_long_t*) *out_expr)->id = EXPR_LONG;
    ((ast_expr_long_t*) *out_expr)->value = value;
    ((ast_expr_long_t*) *out_expr)->source = source;
}

void ast_expr_create_double(ast_expr_t **out_expr, adept_double value, source_t source){
    *out_expr = malloc(sizeof(ast_expr_double_t));
    ((ast_expr_double_t*) *out_expr)->id = EXPR_DOUBLE;
    ((ast_expr_double_t*) *out_expr)->value = value;
    ((ast_expr_double_t*) *out_expr)->source = source;
}

void ast_expr_create_string(ast_expr_t **out_expr, char *array, length_t length, source_t source){
    *out_expr = malloc(sizeof(ast_expr_str_t));
    ((ast_expr_str_t*) *out_expr)->id = EXPR_STR;
    ((ast_expr_str_t*) *out_expr)->array = array;
    ((ast_expr_str_t*) *out_expr)->length = length;
    ((ast_expr_str_t*) *out_expr)->source = source;
}

void ast_expr_create_cstring(ast_expr_t **out_expr, char *value, source_t source){
    *out_expr = malloc(sizeof(ast_expr_cstr_t));
    ((ast_expr_cstr_t*) *out_expr)->id = EXPR_CSTR;
    ((ast_expr_cstr_t*) *out_expr)->value = value;
    ((ast_expr_cstr_t*) *out_expr)->source = source;
}

void ast_expr_create_null(ast_expr_t **out_expr, source_t source){
    *out_expr = malloc(sizeof(ast_expr_null_t));
    ((ast_expr_null_t*) *out_expr)->id = EXPR_NULL;
    ((ast_expr_null_t*) *out_expr)->source = source;
}

void ast_expr_create_variable(ast_expr_t **out_expr, weak_cstr_t name, source_t source){
    *out_expr = malloc(sizeof(ast_expr_variable_t));
    ((ast_expr_variable_t*) *out_expr)->id = EXPR_VARIABLE;
    ((ast_expr_variable_t*) *out_expr)->name = name;
    ((ast_expr_variable_t*) *out_expr)->source = source;
}

void ast_expr_create_call(ast_expr_t **out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source){
    *out_expr = malloc(sizeof(ast_expr_call_t));
    ast_expr_create_call_in_place((ast_expr_call_t*) *out_expr, name, arity, args, is_tentative, gives, source);
}

void ast_expr_create_call_in_place(ast_expr_call_t *out_expr, strong_cstr_t name, length_t arity, ast_expr_t **args, bool is_tentative, ast_type_t *gives, source_t source){
    out_expr->id = EXPR_CALL;
    out_expr->name = name;
    out_expr->arity = arity;
    out_expr->args = args;
    out_expr->is_tentative = is_tentative;
    out_expr->source = source;

    if(gives && gives->elements_length != 0){
        out_expr->gives = *gives;
    } else {
        memset(&out_expr->gives, 0, sizeof(ast_type_t));
    }
    
    out_expr->only_implicit = false;
    out_expr->no_user_casts = false;
}

void ast_expr_create_call_method(ast_expr_t **out_expr, strong_cstr_t name, ast_expr_t *value, length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source){
    *out_expr = malloc(sizeof(ast_expr_call_method_t));
    ast_expr_create_call_method_in_place((ast_expr_call_method_t*) *out_expr, name, value, arity, args, is_tentative, allow_drop, gives, source);
}

void ast_expr_create_call_method_in_place(ast_expr_call_method_t *out_expr, strong_cstr_t name, ast_expr_t *value,
        length_t arity, ast_expr_t **args, bool is_tentative, bool allow_drop, ast_type_t *gives, source_t source){
    
    out_expr->id = EXPR_CALL_METHOD;
    out_expr->name = name;
    out_expr->arity = arity;
    out_expr->args = args;
    out_expr->value = value;
    out_expr->is_tentative = is_tentative;
    out_expr->allow_drop = allow_drop;
    out_expr->source = source;

    if(gives && gives->elements_length != 0){
        out_expr->gives = *gives;
    } else {
        memset(&out_expr->gives, 0, sizeof(ast_type_t));
    }
}

void ast_expr_create_enum_value(ast_expr_t **out_expr, weak_cstr_t name, weak_cstr_t kind, source_t source){
    *out_expr = malloc(sizeof(ast_expr_enum_value_t));
    ((ast_expr_enum_value_t*) *out_expr)->id = EXPR_ENUM_VALUE;
    ((ast_expr_enum_value_t*) *out_expr)->enum_name = name;
    ((ast_expr_enum_value_t*) *out_expr)->kind_name = kind;
    ((ast_expr_enum_value_t*) *out_expr)->source = source;
}

void ast_expr_create_ternary(ast_expr_t **out_expr, ast_expr_t *condition, ast_expr_t *if_true, ast_expr_t *if_false, source_t source){
    *out_expr = malloc(sizeof(ast_expr_ternary_t));
    ((ast_expr_ternary_t*) *out_expr)->id = EXPR_TERNARY;
    ((ast_expr_ternary_t*) *out_expr)->source = source;
    ((ast_expr_ternary_t*) *out_expr)->condition = condition;
    ((ast_expr_ternary_t*) *out_expr)->if_true = if_true;
    ((ast_expr_ternary_t*) *out_expr)->if_false = if_false;
}

void ast_expr_create_cast(ast_expr_t **out_expr, ast_type_t to, ast_expr_t *from, source_t source){
    *out_expr = malloc(sizeof(ast_expr_cast_t));
    ((ast_expr_cast_t*) *out_expr)->id = EXPR_CAST;
    ((ast_expr_cast_t*) *out_expr)->to = to;
    ((ast_expr_cast_t*) *out_expr)->from = from;
    ((ast_expr_cast_t*) *out_expr)->source = source;
}

void ast_expr_create_phantom(ast_expr_t **out_expr, ast_type_t ast_type, void *ir_value, source_t source, bool is_mutable){
    *out_expr = malloc(sizeof(ast_expr_phantom_t));
    ((ast_expr_phantom_t*) *out_expr)->id = EXPR_PHANTOM;
    ((ast_expr_phantom_t*) *out_expr)->source = source;
    ((ast_expr_phantom_t*) *out_expr)->type = ast_type;
    ((ast_expr_phantom_t*) *out_expr)->ir_value = ir_value;
    ((ast_expr_phantom_t*) *out_expr)->is_mutable = is_mutable;
}

void ast_expr_create_typenameof(ast_expr_t **out_expr, ast_type_t strong_type, source_t source){
    *out_expr = malloc(sizeof(ast_expr_typenameof_t));
    ((ast_expr_typenameof_t*) *out_expr)->id = EXPR_TYPENAMEOF;
    ((ast_expr_typenameof_t*) *out_expr)->source = source;
    ((ast_expr_typenameof_t*) *out_expr)->type = strong_type;
}

void ast_expr_create_embed(ast_expr_t **out_expr, strong_cstr_t filename, source_t source){
    *out_expr = malloc(sizeof(ast_expr_embed_t));
    ((ast_expr_embed_t*) *out_expr)->id = EXPR_EMBED;
    ((ast_expr_embed_t*) *out_expr)->source = source;
    ((ast_expr_embed_t*) *out_expr)->filename = filename;
}

void ast_expr_create_declaration(ast_expr_t **out_expr, unsigned int expr_id, source_t source, weak_cstr_t name, ast_type_t type, trait_t traits, ast_expr_t *value){
    *out_expr = malloc(sizeof(ast_expr_declare_t));
    ((ast_expr_declare_t*) *out_expr)->id = expr_id;
    ((ast_expr_declare_t*) *out_expr)->source = source;
    ((ast_expr_declare_t*) *out_expr)->name = name;
    ((ast_expr_declare_t*) *out_expr)->traits = traits;
    ((ast_expr_declare_t*) *out_expr)->type = type;
    ((ast_expr_declare_t*) *out_expr)->value = value;
}

void ast_expr_create_assignment(ast_expr_t **out_expr, unsigned int stmt_id, source_t source, ast_expr_t *mutable_expression, ast_expr_t *value, bool is_pod){
    *out_expr = malloc(sizeof(ast_expr_assign_t));
    ((ast_expr_assign_t*) *out_expr)->id = stmt_id;
    ((ast_expr_assign_t*) *out_expr)->source = source;
    ((ast_expr_assign_t*) *out_expr)->destination = mutable_expression;
    ((ast_expr_assign_t*) *out_expr)->value = value;
    ((ast_expr_assign_t*) *out_expr)->is_pod = is_pod;
}

void ast_expr_create_return(ast_expr_t **out_expr, source_t source, ast_expr_t *value, ast_expr_list_t last_minute){
    *out_expr = malloc(sizeof(ast_expr_return_t));
    ((ast_expr_return_t*) *out_expr)->id = EXPR_RETURN;
    ((ast_expr_return_t*) *out_expr)->source = source;
    ((ast_expr_return_t*) *out_expr)->value = value;
    ((ast_expr_return_t*) *out_expr)->last_minute = last_minute;
}

void ast_expr_create_member(ast_expr_t **out_expr, ast_expr_t *value, strong_cstr_t member_name, source_t source){
    *out_expr = malloc(sizeof(ast_expr_member_t));
    ((ast_expr_member_t*) *out_expr)->id = EXPR_MEMBER;
    ((ast_expr_member_t*) *out_expr)->value = value;
    ((ast_expr_member_t*) *out_expr)->member = member_name;
    ((ast_expr_member_t*) *out_expr)->source = source;
}
                
void ast_expr_create_access(ast_expr_t **out_expr, ast_expr_t *value, ast_expr_t *index, source_t source){
    *out_expr = malloc(sizeof(ast_expr_array_access_t));
    ((ast_expr_array_access_t*) *out_expr)->id = EXPR_ARRAY_ACCESS;
    ((ast_expr_array_access_t*) *out_expr)->source = source;
    ((ast_expr_array_access_t*) *out_expr)->value = value;
    ((ast_expr_array_access_t*) *out_expr)->index = index;
}

void ast_expr_list_init(ast_expr_list_t *list, length_t initial_capacity){
    if(initial_capacity == 0){
        list->statements = NULL;
    } else {
        list->statements = malloc(sizeof(ast_expr_t*) * initial_capacity);
    }
    list->length = 0;
    list->capacity = initial_capacity;
}

void ast_expr_list_free(ast_expr_list_t *list){
    ast_exprs_free_fully(list->statements, list->length);
}

ast_expr_list_t ast_expr_list_clone(ast_expr_list_t *list){
    ast_expr_list_t result;
    result.statements = malloc(sizeof(ast_expr_t*) * list->capacity);
    result.length = list->length;
    result.capacity = list->capacity;

    for(length_t i = 0; i != result.length; i++){
        result.statements[i] = ast_expr_clone(list->statements[i]);
    }

    return result;
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
    ast_case_t result;
    result.condition = ast_expr_clone(original->condition);
    result.statements = ast_expr_list_clone(&original->statements);
    result.source = original->source;
    return result;
}

ast_case_list_t ast_case_list_clone(ast_case_list_t *original){
    ast_case_list_t result;
    result.cases = malloc(sizeof(ast_case_t) * original->capacity);
    result.length = original->length;
    result.capacity = original->capacity;

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
