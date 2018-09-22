
#include "INFER/infer.h"
#include "AST/ast_expr.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "UTIL/levenshtein.h"

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
            if(infer_expr(compiler, object, NULL, global_initial, default_primitive, NULL)) return 1;
        }
    }

    if(infer_in_funcs(compiler, object, ast->funcs, ast->funcs_length)) return 1;
    return 0;
}

int infer_in_funcs(compiler_t *compiler, object_t *object, ast_func_t *funcs, length_t funcs_length){
    for(length_t f = 0; f != funcs_length; f++){
        infer_var_scope_t func_scope;
        infer_var_scope_init(&func_scope, NULL);

        // Resolve aliases in function return type
        if(infer_type_aliases(compiler, object, &funcs[f].return_type)) return 1;

        // Resolve aliases in function arguments
        for(length_t a = 0; a != funcs[f].arity; a++){
            if(infer_type_aliases(compiler, object, &funcs[f].arg_types[a])) return 1;

            if(!(funcs[f].traits & AST_FUNC_FOREIGN)){
                infer_var_scope_add_variable(&func_scope, funcs[f].arg_names[a], &funcs[f].arg_types[a]);
            }
        }

        // Infer expressions in statements
        if(infer_in_stmts(compiler, object, &funcs[f], funcs[f].statements, funcs[f].statements_length, &func_scope)){
            infer_var_scope_free(&func_scope);
            return 1;
        }
        infer_var_scope_free(&func_scope);
    }
    return 0;
}

int infer_in_stmts(compiler_t *compiler, object_t *object, ast_func_t *func, ast_expr_t **statements, length_t statements_length, infer_var_scope_t *scope){
    for(length_t s = 0; s != statements_length; s++){
        switch(statements[s]->id){
        case EXPR_RETURN: {
                ast_expr_return_t *return_stmt = (ast_expr_return_t*) statements[s];
                if(return_stmt->value != NULL && infer_expr(compiler, object, func, &return_stmt->value, ast_primitive_from_ast_type(&func->return_type), scope)) return 1;
            }
            break;
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = (ast_expr_call_t*) statements[s];
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(compiler, object, func, &call_stmt->args[a], EXPR_NONE, scope)) return 1;
                }
            }
            break;
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) statements[s];
                if(infer_type_aliases(compiler, object, &declare_stmt->type)) return 1;
                if(declare_stmt->value != NULL){
                    if(infer_expr(compiler, object, func, &declare_stmt->value, ast_primitive_from_ast_type(&declare_stmt->type), scope)) return 1;
                }

                infer_var_scope_add_variable(scope, declare_stmt->name, &declare_stmt->type);
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
        case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN: {
                ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) statements[s];
                if(infer_expr(compiler, object, func, &assign_stmt->destination, EXPR_NONE, scope)) return 1;
                if(infer_expr(compiler, object, func, &assign_stmt->value, EXPR_NONE, scope)) return 1;
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
                ast_expr_if_t *conditional = (ast_expr_if_t*) statements[s];
                if(infer_expr(compiler, object, func, &conditional->value, EXPR_NONE, scope)) return 1;

                infer_var_scope_push(&scope);
                if(infer_in_stmts(compiler, object, func, conditional->statements, conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                ast_expr_ifelse_t *complex_conditional = (ast_expr_ifelse_t*) statements[s];
                if(infer_expr(compiler, object, func, &complex_conditional->value, EXPR_NONE, scope)) return 1;

                infer_var_scope_push(&scope);
                if(infer_in_stmts(compiler, object, func, complex_conditional->statements, complex_conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                
                infer_var_scope_pop(&scope);
                infer_var_scope_push(&scope);
                if(infer_in_stmts(compiler, object, func, complex_conditional->else_statements, complex_conditional->else_statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK: {
                ast_expr_whilecontinue_t *conditional = (ast_expr_whilecontinue_t*) statements[s];
                infer_var_scope_push(&scope);
                if(infer_in_stmts(compiler, object, func, conditional->statements, conditional->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                if(infer_expr(compiler, object, func, &call_stmt->value, EXPR_NONE, scope)) return 1;
                for(length_t a = 0; a != call_stmt->arity; a++){
                    if(infer_expr(compiler, object, func, &call_stmt->args[a], EXPR_NONE, scope)) return 1;
                }
            }
            break;
        case EXPR_DELETE: {
                ast_expr_unary_t *delete_stmt = (ast_expr_unary_t*) statements[s];
                if(infer_expr(compiler, object, func, &delete_stmt->value, EXPR_NONE, scope)) return 1;
            }
            break;
        case EXPR_EACH_IN: {
                ast_expr_each_in_t *loop = (ast_expr_each_in_t*) statements[s];
                if(infer_type_aliases(compiler, object, loop->it_type)) return 1;
                if(infer_expr(compiler, object, func, &loop->low_array, EXPR_NONE, scope)) return 1;
                if(infer_expr(compiler, object, func, &loop->length, EXPR_ULONG, scope)) return 1;
 
                infer_var_scope_push(&scope);
                infer_var_scope_add_variable(scope, "idx", ast_get_usize(&object->ast));
                infer_var_scope_add_variable(scope, loop->it_name ? loop->it_name : "it", loop->it_type);

                if(infer_in_stmts(compiler, object, func, loop->statements, loop->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        case EXPR_REPEAT: {
                ast_expr_repeat_t *loop = (ast_expr_repeat_t*) statements[s];
                if(infer_expr(compiler, object, func, &loop->limit, EXPR_ULONG, scope)) return 1;
 
                infer_var_scope_push(&scope);
                infer_var_scope_add_variable(scope, "idx", ast_get_usize(&object->ast));

                if(infer_in_stmts(compiler, object, func, loop->statements, loop->statements_length, scope)){
                    infer_var_scope_pop(&scope);
                    return 1;
                }
                infer_var_scope_pop(&scope);
            }
            break;
        default: break;
            // Ignore this statement, it doesn't contain any expressions that we need to worry about
        }
    }
    return 0;
}

int infer_expr(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **root, unsigned int default_assigned_type, infer_var_scope_t *scope){
    // NOTE: The 'ast_expr_t*' pointed to by 'root' may be written to
    // NOTE: 'default_assigned_type' is used as a default assigned type if generics aren't resolved within the expression
    // NOTE: if 'default_assigned_type' is EXPR_NONE, then the most suitable type for the given generics is chosen
    // NOTE: 'ast_func' can be NULL

    undetermined_expr_list_t undetermined;
    undetermined.expressions = malloc(sizeof(ast_expr_t*) * 4);
    undetermined.expressions_length = 0;
    undetermined.expressions_capacity = 4;
    undetermined.solution = EXPR_NONE;

    if(infer_expr_inner(compiler, object, ast_func, root, &undetermined, scope)){
        free(undetermined.expressions);
        return 1;
    }

    // Determine all of the undetermined types
    if(undetermined.solution == EXPR_NONE){
        if(default_assigned_type == EXPR_NONE){
            default_assigned_type = generics_primitive_type(undetermined.expressions,  undetermined.expressions_length);
        }

        if(default_assigned_type != EXPR_NONE){
            if(undetermined_expr_list_give_solution(compiler, object, &undetermined, default_assigned_type)){
                free(undetermined.expressions);
                return 1;
            }
        }
    }

    // <expressions inside undetermined.expressions are just references so we dont free those>
    free(undetermined.expressions);
    return 0;
}

int infer_expr_inner(compiler_t *compiler, object_t *object, ast_func_t *ast_func, ast_expr_t **expr, undetermined_expr_list_t *undetermined, infer_var_scope_t *scope){
    // NOTE: 'ast_func' can be NULL
    // NOTE: The 'ast_expr_t*' pointed to by 'expr' may be written to

    ast_expr_t **a, **b;

    // Expand list if needed
    expand((void**) &undetermined->expressions, sizeof(ast_expr_t*), undetermined->expressions_length,
        &undetermined->expressions_capacity, 1, 2);

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
        if(undetermined_expr_list_give_solution(compiler, object, undetermined, (*expr)->id)) return 1;
        break;
    case EXPR_VARIABLE: {
            bool found_variable = false;
            char *variable_name = ((ast_expr_variable_t*) *expr)->name;
            ast_type_t *variable_type;

            if(ast_func == NULL || scope == NULL){
                compiler_panicf(compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                return 1;
            }

            infer_var_t *infer_var = infer_var_scope_find(scope, variable_name);

            if(infer_var != NULL){
                variable_type = infer_var->type;
                found_variable = true;
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
                    if(infer_expr_inner(compiler, object, ast_func, expr, undetermined, scope)) return 1;

                    break; // The identifier has been resovled, so break
                }
            }

            if(!found_variable){
                compiler_panicf(compiler, (*expr)->source, "Undeclared variable '%s'", variable_name);
                const char *nearest = infer_var_scope_nearest(scope, variable_name);
                if(nearest) printf("\nDid you mean '%s'?\n", nearest);
                return 1;
            }

            // Return if we already know the solution primitive
            if(undetermined->solution == EXPR_NONE) return 0;

            unsigned int var_expr_primitive = ast_primitive_from_ast_type(variable_type);
            if(undetermined_expr_list_give_solution(compiler, object, undetermined, var_expr_primitive)) return 1;
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
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr_inner(compiler, object, ast_func, a, undetermined, scope)) return 1;
        if(infer_expr_inner(compiler, object, ast_func, b, undetermined, scope)) return 1;
        break;
    case EXPR_AND:
    case EXPR_OR:
        a = &((ast_expr_math_t*) *expr)->a;
        b = &((ast_expr_math_t*) *expr)->b;
        if(infer_expr(compiler, object, ast_func, a, EXPR_NONE, scope)) return 1;
        if(infer_expr(compiler, object, ast_func, b, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_CALL:
        for(length_t i = 0; i != ((ast_expr_call_t*) *expr)->arity; i++){
            if(infer_expr(compiler, object, ast_func, &((ast_expr_call_t*) *expr)->args[i], EXPR_NONE, scope)) return 1;
        }
        break;
    case EXPR_GENERIC_INT:
    case EXPR_GENERIC_FLOAT:
        if(undetermined_expr_list_give_generic(compiler, object, undetermined, expr)) return 1;
        break;
    case EXPR_MEMBER:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_member_t*) *expr)->value, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr = (ast_expr_func_addr_t*) *expr;
            if(func_addr->match_args != NULL) for(length_t a = 0; a != func_addr->match_args_length; a++){
                if(infer_type_aliases(compiler, object, &func_addr->match_args[a].type)) return 1;
            }
        }
        break;
    case EXPR_ADDRESS:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_unary_t*) *expr)->value, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_DEREFERENCE:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_unary_t*) *expr)->value, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_NULL:
        break;
    case EXPR_ARRAY_ACCESS:
        if(infer_expr(compiler, object, ast_func, &((ast_expr_array_access_t*) *expr)->index, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_CAST:
        if(infer_type_aliases(compiler, object, &((ast_expr_cast_t*) *expr)->to)) return 1;
        if(infer_expr(compiler, object, ast_func, &((ast_expr_cast_t*) *expr)->from, EXPR_NONE, scope)) return 1;
        break;
    case EXPR_SIZEOF:
        if(infer_type_aliases(compiler, object, &((ast_expr_sizeof_t*) *expr)->type)) return 1;
        break;
    case EXPR_CALL_METHOD:
        if(infer_expr_inner(compiler, object, ast_func, &((ast_expr_call_method_t*) *expr)->value, undetermined, scope)) return 1;
        for(length_t i = 0; i != ((ast_expr_call_method_t*) *expr)->arity; i++){
            if(infer_expr(compiler, object, ast_func, &((ast_expr_call_method_t*) *expr)->args[i], EXPR_NONE, scope)) return 1;
        }
        break;
    case EXPR_NOT:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NEGATE:
        if(infer_expr_inner(compiler, object, ast_func, &((ast_expr_unary_t*) *expr)->value, undetermined, scope)) return 1;
        break;
    case EXPR_NEW:
        if(infer_type_aliases(compiler, object, &((ast_expr_new_t*) *expr)->type)) return 1;
        if(((ast_expr_new_t*) *expr)->amount != NULL && infer_expr(compiler, object, ast_func, &(((ast_expr_new_t*) *expr)->amount), EXPR_NONE, scope)) return 1;
        break;
    case EXPR_NEW_CSTRING:
        break;
    default:
        compiler_panic(compiler, (*expr)->source, "INTERNAL ERROR: Unimplemented expression type inside infer_expr_inner");
        return 1;
    }

    return 0;
}

int undetermined_expr_list_give_solution(compiler_t *compiler, object_t *object, undetermined_expr_list_t *undetermined, unsigned int solution_primitive){
    // Ensure solution is real
    if(solution_primitive == EXPR_NONE) return 0;

    // Ensure we don't already have a solution
    if(undetermined->solution != EXPR_NONE) return 0;
    
    // Remember the solution
    undetermined->solution = solution_primitive;

    // Resolve any generics already in the list
    if(resolve_generics(compiler, object, undetermined->expressions, undetermined->expressions_length, solution_primitive)){
        return 1;
    }

    undetermined->expressions_length = 0;
    return 0;
}

int undetermined_expr_list_give_generic(compiler_t *compiler, object_t *object, undetermined_expr_list_t *undetermined, ast_expr_t **expr){
    if(undetermined->solution != EXPR_NONE){
        // Resolve the generic if we already know what type it should be
        if(resolve_generics(compiler, object, expr, 1, undetermined->solution)) return 1;
    } else {
        // Otherwise, add it to the list of undetermined expressions
        undetermined->expressions[undetermined->expressions_length++] = *expr;
    }
    return 0;
}

int resolve_generics(compiler_t *compiler, object_t *object, ast_expr_t **expressions, length_t expressions_length, unsigned int target_primitive){
    // NOTE: Some dangerous memory stuff goes on in this function
    //           This may break on certain platforms if they use unconventional type sizes that break the C standards
    // NOTE: This function attempts to convert generics expressions into the 'target_primitive'
    // NOTE: Shouldn't be called outside of resolve_undetermined_expressions

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

void infer_var_scope_init(infer_var_scope_t *out_scope, infer_var_scope_t *parent){
    out_scope->parent = parent;
    out_scope->list.variables = NULL;
    out_scope->list.length = 0;
    out_scope->list.capacity = 0;
}

void infer_var_scope_free(infer_var_scope_t *scope){
    free(scope->list.variables);
}

void infer_var_scope_push(infer_var_scope_t **scope){
    infer_var_scope_t *new_scope = malloc(sizeof(infer_var_scope_t));
    infer_var_scope_init(new_scope, *scope);
    *scope = new_scope;
} 

void infer_var_scope_pop(infer_var_scope_t **scope){
    if((*scope)->parent == NULL){
        redprintf("INTERNAL ERROR: TRIED TO POP INFER VARIABLE SCOPE WITH NO PARENT\n");
        return;
    }

    infer_var_scope_t *parent = (*scope)->parent;
    infer_var_scope_free(*scope);
    free(*scope);

    *scope = parent;
}

infer_var_t* infer_var_scope_find(infer_var_scope_t *scope, char *name){
    for(length_t i = 0; i != scope->list.length; i++){
        if(strcmp(scope->list.variables[i].name, name) == 0){
            return &scope->list.variables[i];
        }
    }

    if(scope->parent){
        return infer_var_scope_find(scope->parent, name);
    } else {
        return NULL;
    }
}

void infer_var_scope_add_variable(infer_var_scope_t *scope, char *name, ast_type_t *type){
    infer_var_list_t *list = &scope->list;
    expand((void**) &list->variables, sizeof(infer_var_t), list->length, &list->capacity, 1, 4);

    infer_var_t *var = &list->variables[list->length++];
    var->name = name;
    var->type = type;
}

const char* infer_var_scope_nearest(infer_var_scope_t *scope, char *name){
    char *nearest_name = NULL;
    infer_var_scope_nearest_inner(scope, name, &nearest_name, NULL);
    return nearest_name;
}

void infer_var_scope_nearest_inner(infer_var_scope_t *scope, char *name, char **out_nearest_name, int *out_distance){
    // NOTE: out_nearest_name must be a valid pointer
    // NOTE: out_distance may be NULL

    char *scope_name = NULL, *parent_name = NULL;
    int scope_distance = -1, parent_distance = -1;

    infer_var_list_nearest(&scope->list, name, &scope_name, &scope_distance);

    if(scope->parent == NULL){
        *out_nearest_name = scope_name;
        if(out_distance) *out_distance = scope_distance;
        return;
    }

    infer_var_list_nearest(&scope->parent->list, name, &parent_name, &parent_distance);

    if(scope_distance <= parent_distance){
        *out_nearest_name = scope_name;
        if(out_distance) *out_distance = scope_distance;
    } else {
        *out_nearest_name = parent_name;
        if(out_distance) *out_distance = parent_distance;
    }
}

void infer_var_list_nearest(infer_var_list_t *list, char *name, char **out_nearest_name, int *out_distance){
    // NOTE: out_nearest_name must be a valid pointer
    // NOTE: out_distance may be NULL

    // Default to no nearest value
    *out_nearest_name = NULL;
    if(out_distance) *out_distance = -1;

    // Stack array to contain all of the distances
    // NOTE: This may be bad if the length of the variable list is really long
    length_t list_length = list->length;
    int distances[list_length];

    // Calculate distance for every variable name
    for(length_t i = 0; i != list_length; i++){
        distances[i] = levenshtein(name, list->variables[i].name);
    }

    // Minimum number of changes to override NULL
    length_t minimum = 3;

    // Find the name with the shortest distance
    for(length_t i = 0; i != list_length; i++){
        if(distances[i] < minimum){
            minimum = distances[i];
            *out_nearest_name = list->variables[i].name;
        }
    }

    // Output minimum distance if a name close enough was found
    if(out_distance && *out_nearest_name) *out_distance = minimum;
}
