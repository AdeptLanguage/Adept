
#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_make.h"
#include "AST/ast_type.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

errorcode_t ast_resolve_type_polymorphs(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_type_t *in_type, ast_type_t *out_type){
    ast_elem_t **elements = NULL;
    length_t length = 0;
    length_t capacity = 0;

    for(length_t i = 0; i != in_type->elements_length; i++){
        expand((void**) &elements, sizeof(ast_elem_t*), length, &capacity, 1, 4);

        unsigned int elem_id = in_type->elements[i]->id;

        switch(elem_id){
        case AST_ELEM_FUNC: {
                ast_elem_func_t *func = (ast_elem_func_t*) in_type->elements[i];
                
                ast_type_t *arg_types = malloc(sizeof(ast_type_t) * func->arity);
                ast_type_t *return_type = malloc(sizeof(ast_type_t));

                for(length_t i = 0; i != func->arity; i++){
                    if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &func->arg_types[i], &arg_types[i])){
                        ast_types_free_fully(arg_types, i);
                        free(return_type);
                        return FAILURE;
                    }
                }

                if(ast_resolve_type_polymorphs(compiler, type_table, catalog, func->return_type, return_type)){
                    ast_types_free_fully(arg_types, func->arity);
                    free(return_type);
                    return FAILURE;
                }

                elements[length++] = ast_elem_func_make(func->source, arg_types, func->arity, return_type, func->traits, true);
            }
            break;
        case AST_ELEM_GENERIC_BASE: {
                ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) in_type->elements[i];

                if(generic_base_elem->name_is_polymorphic){
                    compiler_panic(compiler, generic_base_elem->source, "INTERNAL ERROR: Polymorphic names for polymorphic struct unimplemented for ast_resolve_type_polymorphs");
                    return FAILURE;
                }

                ast_type_t *resolved = malloc(sizeof(ast_type_t) * generic_base_elem->generics_length);

                for(length_t i = 0; i != generic_base_elem->generics_length; i++){
                    if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &generic_base_elem->generics[i], &resolved[i])){
                        ast_types_free_fully(resolved, i);
                        return FAILURE;
                    }
                }

                elements[length++] = ast_elem_generic_base_make(strclone(generic_base_elem->name), generic_base_elem->source, resolved, generic_base_elem->generics_length);
            }
            break;
        case AST_ELEM_POLYMORPH: {
                // Find the determined type for the polymorphic type variable
                ast_elem_polymorph_t *polymorphic_element = (ast_elem_polymorph_t*) in_type->elements[i];
                ast_poly_catalog_type_t *type_var = ast_poly_catalog_find_type(catalog, polymorphic_element->name);

                if(type_var == NULL){
                    compiler_panicf(compiler, in_type->source, "Undetermined polymorphic type variable '$%s'", polymorphic_element->name);
                    return FAILURE;
                }

                // Replace the polymorphic type variable with the determined type
                expand((void**) &elements, sizeof(ast_elem_t*), length, &capacity, type_var->binding.elements_length, 4);

                for(length_t j = 0; j != type_var->binding.elements_length; j++){
                    elements[length++] = ast_elem_clone(type_var->binding.elements[j]);
                }
            }
            break;
        case AST_ELEM_POLYCOUNT: {
                // Find the determined type for the polymorphic count variable
                ast_elem_polycount_t *polycount_elem = (ast_elem_polycount_t*) in_type->elements[i];
                ast_poly_catalog_count_t *count_var = ast_poly_catalog_find_count(catalog, polycount_elem->name);

                if(count_var == NULL){
                    compiler_panicf(compiler, in_type->source, "Undetermined polymorphic count variable '$#%s'", polycount_elem->name);
                    return FAILURE;
                }

                // Replace the polymorphic type variable with the determined type
                elements[length++] = ast_elem_fixed_array_make(polycount_elem->source, count_var->binding);
            }
            break;
        default:
            // Clone any non-polymorphic type elements
            elements[length++] = ast_elem_clone(in_type->elements[i]);
        }
    }

    // If output destination is the same place as the input location, clear input before writing
    if(out_type == NULL)    out_type = in_type;
    if(out_type == in_type) ast_type_free(in_type);

    *out_type = (ast_type_t){
        .elements = elements,
        .elements_length = length,
        .source = in_type->source,
    };

    // Since we are past the parsing phase, we can check that RTTI isn't disabled
    if(type_table && !(compiler->traits & COMPILER_NO_TYPEINFO)){
        type_table_give(type_table, out_type, NULL);
    }

    return SUCCESS;
}

errorcode_t ast_resolve_expr_polymorphs(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_t *expr){
    switch(expr->id){
    case EXPR_RETURN: {
            ast_expr_return_t *return_stmt = (ast_expr_return_t*) expr;
            if(return_stmt->value != NULL && ast_resolve_expr_polymorphs(compiler, type_table, catalog, return_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_CALL: {
            ast_expr_call_t *call_stmt = (ast_expr_call_t*) expr;
            if(ast_resolve_exprs_polymorphs(compiler, type_table, catalog, call_stmt->args, call_stmt->arity)) return FAILURE;

            if(call_stmt->gives.elements_length != 0){
                if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &call_stmt->gives, NULL)) return FAILURE;
            }
        }
        break;
    case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
            ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) expr;

            if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &declare_stmt->type, NULL)) return FAILURE;

            if(declare_stmt->value != NULL){
                if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, declare_stmt->value)) return FAILURE;
            }

            if(declare_stmt->inputs.has){
                if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &declare_stmt->inputs.value)) return FAILURE;
            }
        }
        break;
    case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
    case EXPR_LSHIFT_ASSIGN: case EXPR_RSHIFT_ASSIGN:
    case EXPR_LGC_LSHIFT_ASSIGN: case EXPR_LGC_RSHIFT_ASSIGN: {
            ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) expr;

            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, assign_stmt->destination)
            || ast_resolve_expr_polymorphs(compiler, type_table, catalog, assign_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
            ast_expr_if_t *conditional = (ast_expr_if_t*) expr;

            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, conditional->value)) return FAILURE;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &conditional->statements)) return FAILURE;
        }
        break;
    case EXPR_IFELSE: case EXPR_UNLESSELSE: {
            ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) expr;
            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, complex_conditional->value)) return FAILURE;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &complex_conditional->statements)) return FAILURE;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &complex_conditional->else_statements)) return FAILURE;
        }
        break;
    case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
            ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) expr;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &conditional->statements)) return FAILURE;
        }
        break;
    case EXPR_CALL_METHOD: {
            ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) expr;

            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, call_stmt->value)) return FAILURE;
            if(ast_resolve_exprs_polymorphs(compiler, type_table, catalog, call_stmt->args, call_stmt->arity)) return FAILURE;

            if(call_stmt->gives.elements_length != 0){
                if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &call_stmt->gives, NULL)) return FAILURE;
            }
        }
        break;
    case EXPR_DELETE: {
            ast_expr_unary_t *delete_stmt = (ast_expr_unary_t*) expr;
            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, delete_stmt->value)) return FAILURE;
        }
        break;
    case EXPR_EACH_IN: {
            ast_expr_each_in_t *loop = (ast_expr_each_in_t*) expr;

            if(ast_resolve_type_polymorphs(compiler, type_table, catalog, loop->it_type, NULL)
            || (loop->low_array && ast_resolve_expr_polymorphs(compiler, type_table, catalog, loop->low_array))
            || (loop->length && ast_resolve_expr_polymorphs(compiler, type_table, catalog, loop->length))
            || (loop->list && ast_resolve_expr_polymorphs(compiler, type_table, catalog, loop->list))
            || (ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &loop->statements))) return FAILURE;
        }
        break;
    case EXPR_REPEAT: {
            ast_expr_repeat_t *loop = (ast_expr_repeat_t*) expr;

            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, loop->limit)) return FAILURE;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &loop->statements)) return FAILURE;
        }
        break;
    case EXPR_SWITCH: {
            ast_expr_switch_t *switch_stmt = (ast_expr_switch_t*) expr;

            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, switch_stmt->value)) return FAILURE;
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &switch_stmt->or_default)) return FAILURE;

            for(length_t c = 0; c != switch_stmt->cases.length; c++){
                ast_case_t *expr_case = &switch_stmt->cases.cases[c];
                if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, expr_case->condition)) return FAILURE;
                if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &expr_case->statements)) return FAILURE;
            }
        }
        break;
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
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
    case EXPR_BIT_LGC_LSHIFT:
    case EXPR_BIT_LGC_RSHIFT:
    case EXPR_AND:
    case EXPR_OR:
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_math_t*) expr)->a)
        || ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_math_t*) expr)->b)) return FAILURE;
        break;
    case EXPR_MEMBER:
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_member_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) expr;

            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &func_addr->match_args[a], NULL)) return FAILURE;
            }
        }
        break;
    case EXPR_AT:
    case EXPR_ARRAY_ACCESS:
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_array_access_t*) expr)->index)) return FAILURE;
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_array_access_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_CAST:
        if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &((ast_expr_cast_t*) expr)->to, NULL)) return FAILURE;
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_cast_t*) expr)->from)) return FAILURE;
        break;
    case EXPR_SIZEOF:
    case EXPR_ALIGNOF:
    case EXPR_TYPEINFO:
    case EXPR_TYPENAMEOF:
        if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &((ast_expr_unary_type_t*) expr)->type, NULL)) return FAILURE;
        break;
    case EXPR_NEW:
        if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &((ast_expr_new_t*) expr)->type, NULL)) return FAILURE;
        if(((ast_expr_new_t*) expr)->amount != NULL && ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_new_t*) expr)->amount)) return FAILURE;

        if(((ast_expr_new_t*) expr)->inputs.has){
            if(ast_resolve_expr_list_polymorphs(compiler, type_table, catalog, &((ast_expr_new_t*) expr)->inputs.value)) return FAILURE;
        }
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ((ast_expr_unary_t*) expr)->value)) return FAILURE;
        break;
    case EXPR_STATIC_ARRAY: {
            ast_expr_static_data_t *static_array = (ast_expr_static_data_t*) expr;
            if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &static_array->type, NULL)) return FAILURE;
            if(ast_resolve_exprs_polymorphs(compiler, type_table, catalog, static_array->values, static_array->length)) return FAILURE;
        }
        break;
    case EXPR_STATIC_STRUCT: {
            if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &((ast_expr_static_data_t*) expr)->type, NULL)) return FAILURE;
        }
        break;
    case EXPR_TERNARY: {
            ast_expr_ternary_t *ternary = (ast_expr_ternary_t*) expr;
            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ternary->condition)) return FAILURE;
            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ternary->if_true))   return FAILURE;
            if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, ternary->if_false))  return FAILURE;
        }
        break;
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            ast_expr_inline_declare_t *def = (ast_expr_inline_declare_t*) expr;

            if(ast_resolve_type_polymorphs(compiler, type_table, catalog, &def->type, NULL)
            || (def->value != NULL && ast_resolve_expr_polymorphs(compiler, type_table, catalog, def->value))){
                return FAILURE;
            }
        }
        break;
    case EXPR_POLYCOUNT: {
            // Find the determined type for the polymorphic count variable
            ast_poly_catalog_count_t *count_var = ast_poly_catalog_find_count(catalog, ((ast_expr_polycount_t*) expr)->name);

            if(count_var == NULL){
                compiler_panicf(compiler, expr->source, "Undetermined polymorphic count variable '$#%s'", ((ast_expr_polycount_t*) expr)->name);
                return FAILURE;
            }

            // Replace the polymorphic count variable with the determined count
            // DANGEROUS: Assuming that 'sizeof(ast_expr_polycount_t) <= sizeof(ast_expr_usize_t)'
            static_assert(sizeof(ast_expr_polycount_t) <= sizeof(ast_expr_usize_t), "The following code assumes sizeof(ast_expr_polycount_t) <= sizeof(ast_expr_usize_t)");
            ast_expr_free(expr);
            expr->id = EXPR_USIZE;
            ((ast_expr_usize_t*) expr)->value = count_var->binding;
        }
        break;
    default: break;
        // Ignore this statement, it doesn't contain any expressions that we need to worry about
    }
    
    return SUCCESS;
}

errorcode_t ast_resolve_exprs_polymorphs(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_t **exprs, length_t count){
    for(length_t i = 0; i != count; i++){
        if(ast_resolve_expr_polymorphs(compiler, type_table, catalog, exprs[i])) return FAILURE;
    }
    return SUCCESS;
}

errorcode_t ast_resolve_expr_list_polymorphs(compiler_t *compiler, type_table_t *type_table, ast_poly_catalog_t *catalog, ast_expr_list_t *exprs){
    return ast_resolve_exprs_polymorphs(compiler, type_table, catalog, exprs->statements, exprs->length);
}
