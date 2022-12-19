
#include <stdlib.h>

#include "AST/ast_expr.h"
#include "AST/ast_named_expression.h"
#include "AST/ast_type.h"

/*
    Implementation details of the ast_expr_free(ast_expr_t*) function.
*/

static void ast_expr_math_free(ast_expr_math_t *expr){
    ast_expr_free_fully(expr->a);
    ast_expr_free_fully(expr->b);
}

static void ast_expr_call_free(ast_expr_call_t *expr){
    ast_exprs_free_fully(expr->args, expr->arity);

    if(expr->gives.elements_length != 0){
        ast_type_free(&expr->gives);
    }

    free(expr->name);
}

static void ast_expr_member_free(ast_expr_member_t *expr){
    ast_expr_free_fully(expr->value);
    free(expr->member);
}

static void ast_expr_array_access_free(ast_expr_array_access_t *expr){
    ast_expr_free_fully(expr->value);
    ast_expr_free_fully(expr->index);
}

static void ast_expr_cast_free(ast_expr_cast_t *expr){
    ast_type_free(&expr->to);
    ast_expr_free_fully(expr->from);
}

static void ast_expr_unary_type_free(ast_expr_unary_type_t *expr){
    ast_type_free(&expr->type);
}

static void ast_expr_sizeof_value_free(ast_expr_sizeof_value_t *expr){
    ast_expr_free_fully(expr->value);
}

static void ast_expr_phantom_free(ast_expr_phantom_t *expr){
    ast_type_free(&expr->type);
}

static void ast_expr_call_method_free(ast_expr_call_method_t *expr){
    free(expr->name);

    ast_expr_free_fully(expr->value);
    ast_exprs_free_fully(expr->args, expr->arity);

    if(expr->gives.elements_length != 0){
        ast_type_free(&expr->gives);
    }
}

static void ast_expr_va_arg_free(ast_expr_va_arg_t *expr){
    ast_expr_free_fully(expr->va_list);
    ast_type_free(&expr->arg_type);
}

static void ast_expr_initlist_free(ast_expr_initlist_t *expr){
    ast_exprs_free_fully(expr->elements, expr->length);
}

static void ast_expr_polycount_free(ast_expr_polycount_t *expr){
    free(expr->name);
}

static void ast_expr_llvm_asm_free(ast_expr_llvm_asm_t *expr){
    free(expr->assembly);
    ast_exprs_free_fully(expr->args, expr->arity);
}

static void ast_expr_embed_free(ast_expr_embed_t *expr){
    free(expr->filename);
}

static void ast_expr_unary_free(ast_expr_unary_t *expr){
    ast_expr_free_fully(expr->value);
}

static void ast_expr_func_addr_free(ast_expr_func_addr_t *expr){
    if(expr->match_args){
        ast_types_free_fully(expr->match_args, expr->match_args_length);
    }
}

static void ast_expr_new_free(ast_expr_new_t *expr){
    ast_type_free(&expr->type);
    ast_expr_free_fully(expr->amount);

    if(expr->inputs.has){
        ast_exprs_free_fully(expr->inputs.value.expressions, expr->inputs.value.length);
    }
}

static void ast_expr_static_data_free(ast_expr_static_data_t *expr){
    ast_type_free(&expr->type);
    ast_exprs_free_fully(expr->values, expr->length);
}

static void ast_expr_ternary_free(ast_expr_ternary_t *expr){
    ast_expr_free_fully(expr->condition);
    ast_expr_free_fully(expr->if_true);
    ast_expr_free_fully(expr->if_false);
}

static void ast_expr_return_free(ast_expr_return_t *expr){
    ast_expr_free_fully(expr->value);
    ast_expr_list_free(&expr->last_minute);
}

static void ast_expr_declare_free(ast_expr_declare_t *expr){
    ast_type_free(&expr->type);
    ast_expr_free_fully(expr->value);

    if(expr->inputs.has){
        ast_expr_list_free(&expr->inputs.value);
    }
}

static void ast_expr_assign_free(ast_expr_assign_t *expr){
    ast_expr_free_fully(expr->destination);
    ast_expr_free_fully(expr->value);
}

static void ast_expr_conditional_free(ast_expr_conditional_t *expr){
    ast_expr_free_fully(expr->value);
    ast_expr_list_free(&expr->statements);
}

static void ast_expr_conditional_else_free(ast_expr_conditional_else_t *expr){
    ast_expr_free_fully(expr->value);
    ast_expr_list_free(&expr->statements);
    ast_expr_list_free(&expr->else_statements);
}

static void ast_expr_each_in_free(ast_expr_each_in_t *expr){
    free(expr->it_name);
    ast_type_free_fully(expr->it_type);
    ast_expr_free_fully(expr->low_array);
    ast_expr_free_fully(expr->length);
    ast_expr_free_fully(expr->list);
    ast_expr_list_free(&expr->statements);
}

static void ast_expr_repeat_free(ast_expr_repeat_t *expr){
    ast_expr_free_fully(expr->limit);
    ast_expr_list_free(&expr->statements);
}

static void ast_expr_switch_free(ast_expr_switch_t *expr){
    ast_expr_free_fully(expr->value);
    ast_expr_list_free(&expr->or_default);
    ast_case_list_free(&expr->cases);
}

static void ast_expr_va_copy_free(ast_expr_va_copy_t *expr){
    ast_expr_free_fully(expr->dest_value);
    ast_expr_free_fully(expr->src_value);
}

static void ast_expr_for_free(ast_expr_for_t *expr){
    ast_expr_list_free(&expr->before);
    ast_expr_list_free(&expr->after);
    ast_expr_free_fully(expr->condition);
    ast_expr_list_free(&expr->statements);
}

static void ast_expr_declare_named_expression_free(ast_expr_declare_named_expression_t *expr){
    ast_named_expression_free(&expr->named_expression);
}

void ast_expr_free(ast_expr_t *expr){
    if(expr == NULL) return;

    switch(expr->id){
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
        ast_expr_math_free((ast_expr_math_t*) expr);
        break;
    case EXPR_CALL:
        ast_expr_call_free((ast_expr_call_t*) expr);
        break;
    case EXPR_VARIABLE:
        // Nothing to free
        break;
    case EXPR_MEMBER:
        ast_expr_member_free((ast_expr_member_t*) expr);
        break;
    case EXPR_ARRAY_ACCESS:
    case EXPR_AT:
        ast_expr_array_access_free((ast_expr_array_access_t*) expr);
        break;
    case EXPR_CAST:
        ast_expr_cast_free((ast_expr_cast_t*) expr);
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_TYPENAMEOF:
    case EXPR_TYPEINFO:
        ast_expr_unary_type_free((ast_expr_unary_type_t*) expr);
        break;
    case EXPR_SIZEOF_VALUE:
        ast_expr_sizeof_value_free((ast_expr_sizeof_value_t*) expr);
        break;
    case EXPR_PHANTOM:
        ast_expr_phantom_free((ast_expr_phantom_t*) expr);
        break;
    case EXPR_CALL_METHOD:
        ast_expr_call_method_free((ast_expr_call_method_t*) expr);
        break;
    case EXPR_VA_ARG:
        ast_expr_va_arg_free((ast_expr_va_arg_t*) expr);
        break;
    case EXPR_INITLIST:
        ast_expr_initlist_free((ast_expr_initlist_t*) expr);
        break;
    case EXPR_POLYCOUNT:
        ast_expr_polycount_free((ast_expr_polycount_t*) expr);
        break;
    case EXPR_LLVM_ASM:
        ast_expr_llvm_asm_free((ast_expr_llvm_asm_t*) expr);
        break;
    case EXPR_EMBED:
        ast_expr_embed_free((ast_expr_embed_t*) expr);
        break;
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
        ast_expr_unary_free((ast_expr_unary_t*) expr);
        break;
    case EXPR_FUNC_ADDR:
        ast_expr_func_addr_free((ast_expr_func_addr_t*) expr);
        break;
    case EXPR_NEW:
        ast_expr_new_free((ast_expr_new_t*) expr);
        break;
    case EXPR_STATIC_ARRAY:
    case EXPR_STATIC_STRUCT:
        ast_expr_static_data_free((ast_expr_static_data_t*) expr);
        break;
    case EXPR_TERNARY:
        ast_expr_ternary_free((ast_expr_ternary_t*) expr);
        break;
    case EXPR_RETURN:
        ast_expr_return_free((ast_expr_return_t*) expr);
        break;
    case EXPR_DECLARE:
    case EXPR_ILDECLARE:
    case EXPR_DECLAREUNDEF:
    case EXPR_ILDECLAREUNDEF:
        ast_expr_declare_free((ast_expr_declare_t*) expr);
        break;
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
    case EXPR_LGC_RSHIFT_ASSIGN:
        ast_expr_assign_free((ast_expr_assign_t*) expr);
        break;
    case EXPR_IF:
    case EXPR_UNLESS:
    case EXPR_WHILE:
    case EXPR_UNTIL:
    case EXPR_WHILECONTINUE:
    case EXPR_UNTILBREAK:
        ast_expr_conditional_free((ast_expr_conditional_t*) expr);
        break;
    case EXPR_IFELSE:
    case EXPR_UNLESSELSE:
        ast_expr_conditional_else_free((ast_expr_conditional_else_t*) expr);
        break;
    case EXPR_EACH_IN:
        ast_expr_each_in_free((ast_expr_each_in_t*) expr);
        break;
    case EXPR_REPEAT:
        ast_expr_repeat_free((ast_expr_repeat_t*) expr);
        break;
    case EXPR_SWITCH:
        ast_expr_switch_free((ast_expr_switch_t*) expr);
        break;
    case EXPR_VA_COPY:
        ast_expr_va_copy_free((ast_expr_va_copy_t*) expr);
        break;
    case EXPR_FOR:
        ast_expr_for_free((ast_expr_for_t*) expr);
        break;
    case EXPR_DECLARE_NAMED_EXPRESSION:
        ast_expr_declare_named_expression_free((ast_expr_declare_named_expression_t*) expr);
        break;
    case EXPR_CONDITIONLESS_BLOCK:
        ast_expr_list_free(&((ast_expr_conditionless_block_t*) expr)->statements);
        break;
    case EXPR_ASSERT:
        ast_expr_free_fully(((ast_expr_assert_t*) expr)->assertion);
        ast_expr_free_fully(((ast_expr_assert_t*) expr)->message);
        break;
    }
}
