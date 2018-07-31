
#include "util.h"
#include "search.h"
#include "ast_expr.h"
#include "inference.h"

int infer(compiler_t *compiler, object_t *object){
    ast_t *ast = &object->ast;

    // Sort aliases and constants so we can binary search on them later
    qsort(ast->aliases, ast->aliases_length, sizeof(ast_alias_t), ast_aliases_cmp);
    qsort(ast->constants, ast->constants_length, sizeof(ast_constant_t), ast_constants_cmp);

    for(length_t a = 0; a != ast->aliases_length; a++){
        if(infer_type_aliases(compiler, object, &ast->aliases[a].type)) return 1;
    }

    for(length_t s = 0; s != ast->structs_length; s++){
        ast_struct_t *st = &ast->structs[s];
        for(length_t f = 0; f != st->field_count; f++){
            if(infer_type_aliases(compiler, object, &st->field_types[f])) return 1;
        }
    }

    for(length_t g = 0; g != ast->globals_length; g++){
        if(infer_type_aliases(compiler, object, &ast->globals[g].type)) return 1;
        ast_expr_t **global_initial = &ast->globals[g].initial;
        if(*global_initial != NULL){
            unsigned int default_primitive = ast_primitive_from_ast_type(&ast->globals[g].type);
            if(infer_expr(compiler, object, NULL, global_initial, default_primitive)) return 1;
        }
    }

    if(infer_in_funcs(compiler, object, ast->funcs, ast->funcs_length)) return 1;
    return 0;
}

int infer_in_funcs(compiler_t *compiler, object_t *object, ast_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        // Resolve aliases in function return type
        if(infer_type_aliases(compiler, object, &funcs[f].return_type)) return 1;

        // Resolve aliases in function arguments
        for(length_t a = 0; a != funcs[f].arity; a++){
            if(infer_type_aliases(compiler, object, &funcs[f].arg_types[a])) return 1;
        }

        // Resolve aliases in variable types
        ast_var_list_t *var_list = &funcs[f].var_list;
        if(var_list != NULL) for(length_t v = 0; v != var_list->length; v++){
            if(infer_type_aliases(compiler, object, var_list->types[v])) return 1;
        }

        // Infer expressions in statements
        if(infer_in_stmts(compiler, object, &funcs[f], funcs[f].statements, funcs[f].statements_length)) return 1;
    }
    return 0;
}

int infer_in_stmts(compiler_t *compiler, object_t *object, ast_func_t *func, ast_expr_t **statements, length_t statements_length){
    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN: {
                ast_expr_return_t *return_stmt = (ast_expr_return_t*) statements[s];
                if(return_stmt->value != NULL && infer_expr(compiler, object, func, &return_stmt->value, ast_primitive_from_ast_type(&func->return_type))) return 1;
            }
            break;
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = (ast_expr_call_t*) statements[s];
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(compiler, object, func, &call_stmt->args[a], EXPR_NONE)) return 1;
                }
            }
            break;
        case EXPR_DECLARE: {
                ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) statements[s];
                if(infer_type_aliases(compiler, object, &declare_stmt->type)) return 1;
                if(declare_stmt->value != NULL){
                    if(infer_expr(compiler, object, func, &declare_stmt->value, ast_primitive_from_ast_type(&declare_stmt->type))) return 1;
                }
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
        case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN: {
                ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) statements[s];
                if(infer_expr(compiler, object, func, &assign_stmt->destination, EXPR_NONE)) return 1;
                if(infer_expr(compiler, object, func, &assign_stmt->value, EXPR_NONE)) return 1;
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
                ast_expr_if_t *conditional = (ast_expr_if_t*) statements[s];
                if(infer_expr(compiler, object, func, &conditional->value, EXPR_NONE)) return 1;
                if(infer_in_stmts(compiler, object, func, conditional->statements, conditional->statements_length)) return 1;
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) statements[s];
                if(infer_expr(compiler, object, func, &complex_conditional->value, EXPR_NONE)) return 1;
                if(infer_in_stmts(compiler, object, func, complex_conditional->statements, complex_conditional->statements_length)) return 1;
                if(infer_in_stmts(compiler, object, func, complex_conditional->else_statements, complex_conditional->else_statements_length)) return 1;
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) statements[s];
                if(infer_in_stmts(compiler, object, func, conditional->statements, conditional->statements_length)) return 1;
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                if(infer_expr(compiler, object, func, &call_stmt->value, EXPR_NONE)) return 1;
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(compiler, object, func, &call_stmt->args[a], EXPR_NONE)) return 1;
                }
            }
            break;
        case EXPR_DELETE: {
                ast_expr_delete_t *delete_stmt = (ast_expr_delete_t*) statements[s];
                if(infer_expr(compiler, object, func, &delete_stmt->value, EXPR_NONE)) return 1;
            }
            break;
        default: break;
            // Ignore this statement, it doesn't contain any expressions that we need to worry about
        }
    }
    return 0;
}

int infer_expr(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type){
    // NOTE: The 'ast_expr_t*' pointed to by 'root' may be written to
    // NOTE: 'default_assigned_type' is used as a default assigned type if generics aren't resolved within the expression
    // NOTE: if 'default_assigned_type' is EXPR_NONE, then the most suitable type for the given generics is chosen
    // NOTE: 'ast_func' can be NULL

    undetermined_type_list_t undetermined;
    undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
    undetermined.expressions_length = 0;
    undetermined.expressions_capacity = 4;
    undetermined.determined = EXPR_NONE;

    if(infer_expr_inner(compiler, object, ast_func, root, &undetermined)){
        free(undetermined.expressions);
        return 1;
    }

    // Determine all of the undetermined types
    if(undetermined.determined == EXPR_NONE){
        if(default_assigned_type == EXPR_NONE){
            default_assigned_type = generics_primitive_type(undetermined.expressions,  undetermined.expressions_length);
        }

        if(determine_undetermined(compiler, object, &undetermined, default_assigned_type)){
            free(undetermined.expressions);
            return 1;
        }
    }

    // <expressions inside undetermined.expressions are just references so we dont free those>
    free(undetermined.expressions);
    return 0;
}

int infer_expr_inner(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **expr, undetermined_type_list_t *undetermined){
    // NOTE: 'ast_func' can be NULL
    // NOTE: The 'ast_expr_t*' pointed to by 'expr' may be written to

    ast_expr_t **a, **b;

    // Expand list if needed
    if(undetermined->expressions_length == undetermined->expressions_capacity){
        undetermined->expressions_capacity *= 2;
        ast_expr_t **new_expressions = malloc(sizeof(ast_expr_t*) * undetermined->expressions_capacity);
        memcpy(new_expressions, undetermined->expressions, sizeof(ast_expr_t*) * undetermined->expressions_length);
        free(undetermined->expressions);
        undetermined->expressions = new_expressions;
    }

    switch((*expr)->id){
    case EXPR_NONE: break;
    case EXPR_BYTE:
    case EXPR_UBYTE:
    case EXPR_SHORT:
    case EXPR_USHORT:
    case EXPR_INT:
    case EXPR_UINT:
    case EXPR_LONG:
    case EXPR_ULONG:
    case EXPR_FLOAT:
    case EXPR_DOUBLE:
    case EXPR_BOOLEAN:
        if(undetermined->determined != EXPR_NONE){
            if(resolve_generics(compiler, object, expr, 1, undetermined->determined)) return 1;
        }
        else if(determine_undetermined(compiler, object, undetermined, (*expr)->id)) return 1;
        break;
    case EXPR_VARIABLE:
        if(undetermined->determined != EXPR_NONE){
            if(resolve_generics(compiler, object, expr, 1, undetermined->determined)) return 1;
        }
        else {
            unsigned int var_expr_id;
            bool found_variable = false;
            char *variable_name = ((ast_expr_variable_t*) *expr)->name;
            ast_type_t *variable_type;

            if(ast_func == NULL){
                compiler_panicf(compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                return 1;
            }

            for(length_t v = 0; v != ast_func->var_list.length; v++){
                if(strcmp(variable_name, ast_func->var_list.names[v]) == 0){
                    variable_type = ast_func->var_list.types[v];
                    found_variable = true;
                    break;
                }
            }

            ast_t *ast = &object->ast;

            // Search in globals if local one couldn't be found
            if(!found_variable){
                for(length_t g = 0; g != ast->globals_length; g++){
                    if(strcmp(variable_name, ast->globals[g].name) == 0){
                        variable_type = &ast->globals[g].type;
                        found_variable = true;
                        break;
                    }
                }
            }

            // See if this is a constant
            if(!found_variable){
                length_t constant_index = find_constant(ast->constants, ast->constants_length, variable_name);

                if(constant_index != -1){
                    // Constant does exist, substitute it's value

                    // DANGEROUS: Manually freeing variable expression
                    free(*expr);

                    *expr = ast_expr_clone(ast->constants[constant_index].expression); // Clone constant expression
                    if(infer_expr_inner(compiler, object, ast_func, expr, undetermined)) return 1;
                    break; // The identifier has been resovled, so break
                }
            }

            if(!found_variable){
                compiler_panicf(compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                const char *nearest = ast_var_list_nearest(&ast_func->var_list, variable_name);
                if(nearest) printf("\nDid you mean '%s'?\n", nearest);
                return 1;
            }

            unsigned int primitive = generics_primitive_type(undetermined->expressions, undetermined->expressions_length);

            if(primitive == EXPR_NONE) return 0; // Don't determine if there aren't any generics to determine
            var_expr_id = ast_primitive_from_ast_type(variable_type);

            if(var_expr_id != EXPR_NONE){
                if(determine_undetermined(compiler, object, undetermined, var_expr_id)) return 1;
            }
        }
        break;
    case EXPR_STR: break;
    case EXPR_CSTR: break;
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
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr_inner(compiler, object, ast_func, a, undetermined)) return 1;
        if(infer_expr_inner(compiler, object, ast_func, b, undetermined)) return 1;
        break;
    case EXPR_AND:
    case EXPR_OR:
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr(compiler, object, ast_func, a, EXPR_NONE)) return 1;
        if(infer_expr(compiler, object, ast_func, b, EXPR_NONE)) return 1;
        break;
    case EXPR_CALL:
        for(length_t i = 0; i != ((ast_expr_call_t*) *expr)->arity; i++){
            if(infer_expr(compiler, object, ast_func, &((ast_expr_call_t*) *expr)->args[i], EXPR_NONE)) return 1;
        }
        break;
    case EXPR_GENERIC_INT:
    case EXPR_GENERIC_FLOAT:
        if(undetermined->determined != EXPR_NONE){
            if(resolve_generics(compiler, object, expr, 1, undetermined->determined)) return 1;
        }
        else undetermined->expressions[undetermined->expressions_length++] = *expr;
        break;
    case EXPR_MEMBER:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_member_t*) *expr)->value, EXPR_NONE)) return 1;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) *expr;
            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(infer_type_aliases(compiler, object, &func_addr->match_args[a].type)) return 1;
            }
        }
        break;
    case EXPR_ADDRESS:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_address_t*) *expr)->value, EXPR_NONE)) return 1;
        break;
    case EXPR_DEREFERENCE:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_deref_t*) *expr)->value, EXPR_NONE)) return 1;
        break;
    case EXPR_NULL:
        break;
    case EXPR_ARRAY_ACCESS:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_array_access_t*) *expr)->index, EXPR_NONE)) return 1;
        break;
    case EXPR_CAST:
        if(infer_type_aliases(compiler, object, &((ast_expr_cast_t*) *expr)->to)) return 1;
        if(infer_expr(compiler, object, ast_func, &((ast_expr_cast_t*) *expr)->from, EXPR_NONE)) return 1;
        break;
    case EXPR_SIZEOF:
        if(infer_type_aliases(compiler, object, &((ast_expr_sizeof_t*) *expr)->type)) return 1;
        break;
    case EXPR_CALL_METHOD:
        if(infer_expr_inner(compiler, object, ast_func, &((ast_expr_call_method_t*) *expr)->value, undetermined)) return 1;
        for(length_t i = 0; i != ((ast_expr_call_method_t*) *expr)->arity; i++){
            if(infer_expr(compiler, object, ast_func, &((ast_expr_call_method_t*) *expr)->args[i], EXPR_NONE)) return 1;
        }
        break;
    case EXPR_NOT:
        if(infer_expr_inner(compiler, object, ast_func, &((ast_expr_not_t*) *expr)->value, undetermined)) return 1;
        break;
    case EXPR_NEW:
        if(((ast_expr_new_t*) *expr)->amount != NULL && infer_expr(compiler, object, ast_func, &(((ast_expr_new_t*) *expr)->amount), EXPR_NONE)) return 1;
        break;
    default:
        compiler_panic(compiler, (*expr)->source, "INTERNAL ERROR: Unimplemented expression type inside infer_expr_inner");
        return 1;
    }

    return 0;
}

int determine_undetermined(compiler_t *compiler, object_t *object, undetermined_type_list_t *undetermined, unsigned int target_primitive){
    assert(undetermined->determined == EXPR_NONE); // Shouldn't have already been determined
    undetermined->determined = target_primitive;
    if(resolve_generics(compiler, object, undetermined->expressions, undetermined->expressions_length, target_primitive)) return 1;
    undetermined->expressions_length = 0;
    return 0;
}

int resolve_generics(compiler_t *compiler, object_t *object, ast_expr_t **expressions, length_t expressions_length, unsigned int target_primitive){
    // NOTE: Some dangerous memory stuff goes on in this function
    //           This may break on certain platforms if they use unconventional type sizes that break the C standards
    // NOTE: This function takes all of the generic expressions in the list 'expressions' and attempts to
    //           convert them to the primitive type of 'target_primitive'

    for(length_t e = 0; e != expressions_length; e++){
        ast_expr_t *expr = expressions[e];

        switch(expr->id){
        case EXPR_GENERIC_INT:
            switch(target_primitive){
            case EXPR_BOOLEAN: {
                     // This is ok because the memory that was allocated is larger than needed
                    ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_int_t*) expr)->value != 0);
                }
                break;
            case EXPR_BYTE: case EXPR_UBYTE: {
                    char tmp = (char) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_byte_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_SHORT: case EXPR_USHORT: {
                    short tmp = (short) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_short_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_INT: case EXPR_UINT: case EXPR_LONG: case EXPR_ULONG:
                break; // No changes special changes necessary
            case EXPR_FLOAT: case EXPR_DOUBLE: {
                    double tmp = (double) ((ast_expr_generic_int_t*) expr)->value;
                    ((ast_expr_double_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                }
                break;
            default:
                compiler_panic(compiler, expr->source, "INTERNAL ERROR: Unrecognized target primitive in resolve_generics()");
                return 1; // Unknown assigned type
            }
            expr->id = target_primitive;
            break;
        case EXPR_GENERIC_FLOAT:
            switch(target_primitive){
            case EXPR_BOOLEAN: {
                     // This is ok because the memory that was allocated is larger than needed
                    ((ast_expr_boolean_t*) expr)->value = (((ast_expr_generic_float_t*) expr)->value != 0);
                    compiler_warnf(compiler, expr->source, "Implicitly converting generic float value to a bool");
                }
                break;
            case EXPR_BYTE: case EXPR_UBYTE: {
                    char tmp = (char) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_byte_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                    compiler_warnf(compiler, expr->source, "Implicitly converting generic float value to a %s", target_primitive == EXPR_BYTE ? "byte" : "ubyte");
                }
                break;
            case EXPR_SHORT: case EXPR_USHORT: {
                    short tmp = (short) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_short_t*) expr)->value = tmp; // This is ok because the memory that was allocated is larger than needed
                    compiler_warnf(compiler, expr->source, "Implicitly converting generic float value to a %s", target_primitive == EXPR_SHORT ? "short" : "ushort");
                }
                break;
            case EXPR_INT: case EXPR_UINT: case EXPR_LONG: case EXPR_ULONG: {
                    long long tmp = (long long) ((ast_expr_generic_float_t*) expr)->value;
                    ((ast_expr_int_t*) expr)->value = tmp; // // This is ok because the memory that was allocated is larger than needed
                }
                break;
            case EXPR_FLOAT: case EXPR_DOUBLE:
                break; // No changes special changes necessary
            default:
                compiler_panic(compiler, expr->source, "INTERNAL ERROR: Unrecognized target primitive in resolve_generics()");
                return 1; // Unknown assigned type
            }
            expr->id = target_primitive;
            break;
        // Ignore any non-generic expressions
        }
    }

    return 0;
}

unsigned int generics_primitive_type(ast_expr_t **expressions, length_t expressions_length){
    // NOTE: This function determines what the primitive type should be of a list of
    //           generic expressions 'expressions' that don't have any outside influence
    //           with concrete primitives
    //       e.g. the primitve type of (10 + 8.0 + 3) is a double
    //       e.g. the primitve type of (10 + 3) is an int

    // Do best to find a common type for generic types
    char generics = 0x00;
    #define FLAG_GENERIC_INT   0x01
    #define FLAG_GENERIC_FLOAT 0x02

    for(length_t i = 0; i != expressions_length; i++){
        if(expressions[i]->id == EXPR_GENERIC_INT)   {generics |= FLAG_GENERIC_INT;   continue;}
        if(expressions[i]->id == EXPR_GENERIC_FLOAT) {generics |= FLAG_GENERIC_FLOAT; continue;}
    }

    // Resolve from generics encountered:
    unsigned int assigned_type = EXPR_NONE;

    if((generics & FLAG_GENERIC_INT) && (generics & FLAG_GENERIC_FLOAT)) assigned_type = EXPR_DOUBLE;
    else if(generics & FLAG_GENERIC_INT)   assigned_type = EXPR_INT;
    else if(generics & FLAG_GENERIC_FLOAT) assigned_type = EXPR_DOUBLE;

    #undef FLAG_GENERIC_INT
    #undef FLAG_GENERIC_FLOAT
    return assigned_type;
}

unsigned int ast_primitive_from_ast_type(ast_type_t *type){
    // NOTE: Returns EXPR_NONE when no suitable fit primitive

    if(type->elements_length != 1) return EXPR_NONE;
    if(type->elements[0]->id != AST_ELEM_BASE) return EXPR_NONE;

    char *base = ((ast_elem_base_t*) type->elements[0])->base;
    const length_t primitives_length = 12;
    const char * const primitives[] = {
        "bool", "byte", "double", "float", "int", "long", "short", "ubyte", "uint", "ulong", "ushort", "usize"
    };

    int array_index = binary_string_search(primitives, primitives_length, base);

    switch(array_index){
    case  0: return EXPR_BOOLEAN;
    case  1: return EXPR_BYTE;
    case  2: return EXPR_DOUBLE;
    case  3: return EXPR_FLOAT;
    case  4: return EXPR_INT;
    case  5: return EXPR_LONG;
    case  6: return EXPR_SHORT;
    case  7: return EXPR_UBYTE;
    case  8: return EXPR_UINT;
    case  9: return EXPR_ULONG;
    case 10: return EXPR_USHORT;
    case 11: return EXPR_ULONG;
    case -1: return EXPR_NONE;
    }

    return EXPR_NONE; // Should never be reached
}

int infer_type_aliases(compiler_t *compiler, object_t *object, ast_type_t *type){
    // NOTE: Expands 'type' by resolving any aliases

    ast_alias_t *aliases = object->ast.aliases;
    length_t aliases_length = object->ast.aliases_length;

    ast_elem_t **new_elements = NULL;
    length_t length = 0;
    length_t capacity = 0;

    for(length_t e = 0; e != type->elements_length; e++){
        if(type->elements[e]->id == AST_ELEM_BASE){
            int alias_index = find_alias(aliases, aliases_length, ((ast_elem_base_t*) type->elements[e])->base);
            if(alias_index != -1){
                // NOTE: The alias target type was already resolved of any aliases,
                //       so we don't have to scan the new elements
                ast_type_t cloned = ast_type_clone(&aliases[alias_index].type);
                expand((void**) &new_elements, sizeof(ast_elem_t*), length, &capacity, cloned.elements_length, 4);

                // Move all the elements from the cloned type to this type
                for(length_t m = 0; m != cloned.elements_length; m++){
                    new_elements[length++] = cloned.elements[m];
                }

                // DANGEROUS: Manually deleting ast_elem_base_t
                free(((ast_elem_base_t*) type->elements[e])->base);
                free(type->elements[e]);

                free(cloned.elements);
                continue; // Don't do normal stuff that follows
            }
        }
        else if(type->elements[e]->id == AST_ELEM_FUNC){
            for(length_t a = 0; a != ((ast_elem_func_t*) type->elements[e])->arity; a++){
                if(infer_type_aliases(compiler, object, &((ast_elem_func_t*) type->elements[e])->arg_types[a])) return 1;
            }

            if(infer_type_aliases(compiler, object, ((ast_elem_func_t*) type->elements[e])->return_type)) return 1;
        }

        // If not an alias, continue on as usual
        expand((void**) &new_elements, sizeof(ast_elem_t*), length, &capacity, 1, 4);
        new_elements[length++] = type->elements[e];
    }

    // Overwrite changes
    free(type->elements);
    type->elements = new_elements;
    type->elements_length = length;

    return 0;
}
