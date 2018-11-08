
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "UTIL/color.h"

strong_cstr_t ast_expr_str(ast_expr_t *expr){
    // NOTE: Returns an allocated string representation of the expression

    char *representation, *a, *b;
    length_t a_len, b_len;

    switch(expr->id){
    case EXPR_BYTE:
        representation = malloc(21);
        sprintf(representation, "%dsb", (int) ((ast_expr_byte_t*) expr)->value);
        return representation;
    case EXPR_UBYTE:
        representation = malloc(7);
        sprintf(representation, "0x%02Xub", (unsigned int) ((ast_expr_ubyte_t*) expr)->value);
        return representation;
    case EXPR_SHORT:
        representation = malloc(21);
        sprintf(representation, "%ldss", (long int) ((ast_expr_short_t*) expr)->value);
        return representation;
    case EXPR_USHORT:
        representation = malloc(21);
        sprintf(representation, "%ldus", (long int) ((ast_expr_ushort_t*) expr)->value);
        return representation;
    case EXPR_INT:
        representation = malloc(21);
        sprintf(representation, "%ldsi", (long int) ((ast_expr_int_t*) expr)->value);
        return representation;
    case EXPR_UINT:
        representation = malloc(21);
        sprintf(representation, "%ldui", (long int) ((ast_expr_uint_t*) expr)->value);
        return representation;
    case EXPR_LONG:
        representation = malloc(21);
        sprintf(representation, "%ldsl", (long int) ((ast_expr_long_t*) expr)->value);
        return representation;
    case EXPR_ULONG:
        representation = malloc(21);
        sprintf(representation, "%ldul", (long int) ((ast_expr_ulong_t*) expr)->value);
        return representation;
    case EXPR_GENERIC_INT:
        representation = malloc(21);
        sprintf(representation, "%ldgi", (long int) ((ast_expr_generic_int_t*) expr)->value);
        return representation;
    case EXPR_FLOAT:
        representation = malloc(21);
        sprintf(representation, "%06.6ff", ((ast_expr_float_t*) expr)->value);
        return representation;
    case EXPR_DOUBLE:
        representation = malloc(21);
        sprintf(representation, "%06.6fd", ((ast_expr_float_t*) expr)->value);
        return representation;
    case EXPR_BOOLEAN:
        representation = malloc(6);
        strcpy(representation, ((ast_expr_boolean_t*) expr)->value ? "true" : "false");
        return representation;
    case EXPR_GENERIC_FLOAT:
        representation = malloc(21);
        sprintf(representation, "%06.6fgf", ((ast_expr_generic_float_t*) expr)->value);
        return representation;
    case EXPR_NULL:
        representation = malloc(5);
        memcpy(representation, "null", 5);
        return representation;
    case EXPR_STR: {
            length_t put_index = 1;
            length_t special_characters = 0;

            for(length_t s = 0; s != ((ast_expr_str_t*) expr)->length; s++){
                if(((ast_expr_str_t*) expr)->array[s] <= 0x1F || ((ast_expr_str_t*) expr)->array[s] == '\\') special_characters++;
            }

            representation = malloc(((ast_expr_str_t*) expr)->length + special_characters + 3);
            representation[0] = '\"';

            for(length_t i = 0; i != ((ast_expr_str_t*) expr)->length; i++){
                switch(((ast_expr_str_t*) expr)->array[i]){
                case '\0':
                    representation[put_index++] = '\\';
                    representation[put_index++] = '0';
                    break;
                case '\t':
                    representation[put_index++] = '\\';
                    representation[put_index++] = 't';
                    break;
                case '\n':
                    representation[put_index++] = '\\';
                    representation[put_index++] = '0';
                    break;
                case '\r':
                    representation[put_index++] = '\\';
                    representation[put_index++] = 'r';
                    break;
                case '\b':
                    representation[put_index++] = '\\';
                    representation[put_index++] = 'b';
                    break;
                case '\e':
                    representation[put_index++] = '\\';
                    representation[put_index++] = 'e';
                    break;
                case '\\':
                    representation[put_index++] = '\\';
                    representation[put_index++] = '\\';
                    break;
                case '"':
                    representation[put_index++] = '\\';
                    representation[put_index++] = '"';
                    break;
                default:
                    representation[put_index++] = ((ast_expr_str_t*) expr)->array[i];
                }
            }

            representation[put_index++] = '"';
            representation[put_index++] = '\0';
            return representation;
        }
    case EXPR_CSTR: {
            char *string_data = ((ast_expr_cstr_t*) expr)->value;
            length_t string_data_length = strlen(string_data);
            length_t put_index = 1;
            length_t special_characters = 0;

            for(length_t s = 0; s != string_data_length; s++){
                if(string_data[s] <= 0x1F || string_data[s] == '\\') special_characters++;
            }

            representation = malloc(string_data_length + special_characters + 3);
            representation[0] = '\'';

            for(length_t c = 0; c != string_data_length; c++){
                if(string_data[c] > 0x1F && string_data[c] != '\\') representation[put_index++] = string_data[c];
                else {
                    switch(string_data[c]){
                    case '\n':
                        representation[put_index++] = '\\';
                        representation[put_index++] = 'n';
                        break;
                    case '\r':
                        representation[put_index++] = '\\';
                        representation[put_index++] = 'r';
                        break;
                    case '\t':
                        representation[put_index++] = '\\';
                        representation[put_index++] = 't';
                        break;
                    case '\b':
                        representation[put_index++] = '\\';
                        representation[put_index++] = 'b';
                        break;
                    case '\e':
                        representation[put_index++] = '\\';
                        representation[put_index++] = 'e';
                        break;
                    case '\'':
                        representation[put_index++] = '\\';
                        representation[put_index++] = '\'';
                        break;
                    case '\\':
                        representation[put_index++] = '\\';
                        representation[put_index++] = '\\';
                        break;
                    default:
                        representation[put_index++] = '\\';
                        representation[put_index++] = '?';
                        break;
                    }
                }
            }

            representation[put_index++] = '\'';
            representation[put_index++] = '\0';
            return representation;
        }
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
        a = ast_expr_str(((ast_expr_math_t*) expr)->a);
        b = ast_expr_str(((ast_expr_math_t*) expr)->b);
        a_len = strlen(a);
        b_len = strlen(b);

        // No optimal for speed, but reduces code redundancy
        switch(expr->id){
        case EXPR_ADD:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " + ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_SUBTRACT:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " - ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_MULTIPLY:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " * ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_DIVIDE:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " / ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_MODULUS:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " % ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_EQUALS:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " == ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_NOTEQUALS:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " != ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_GREATER:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " > ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_LESSER:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " < ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_GREATEREQ:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " >= ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_LESSEREQ:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " <= ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_AND:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " && ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_OR:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " || ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_BIT_AND:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " & ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_BIT_OR:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " | ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_BIT_XOR:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " ^ ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
            break;
        case EXPR_BIT_LSHIFT:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " << ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_BIT_RSHIFT:
            representation = malloc(a_len + b_len + 7);
            memcpy(&representation[a_len + 1], " >> ", 4);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 5], ")", 2);
            break;
        case EXPR_BIT_LGC_LSHIFT:
            representation = malloc(a_len + b_len + 8);
            memcpy(&representation[a_len + 1], " <<< ", 5);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 6], ")", 2);
            break;
        case EXPR_BIT_LGC_RSHIFT:
            representation = malloc(a_len + b_len + 8);
            memcpy(&representation[a_len + 1], " >>> ", 5);
            memcpy(&representation[a_len + 5], b, b_len);
            memcpy(&representation[a_len + b_len + 6], ")", 2);
            break;
        default:
            representation = malloc(a_len + b_len + 6);
            memcpy(&representation[a_len + 1], " ¿ ", 3);
            memcpy(&representation[a_len + 4], b, b_len);
            memcpy(&representation[a_len + b_len + 4], ")", 2);
        }

        representation[0] = '(';
        memcpy(&representation[1], a, a_len);
        free(a);
        free(b);
        return representation;
    case EXPR_CALL: {
            // CLEANUP: This code is kinda scrappy, but hey it works

            ast_expr_call_t *call_expr = (ast_expr_call_t*) expr;
            char **arg_strings = malloc(sizeof(char*) * call_expr->arity);
            length_t args_representation_length = 0;

            for(length_t i = 0; i != call_expr->arity; i++){
                arg_strings[i] = ast_expr_str(call_expr->args[i]);
                args_representation_length += strlen(arg_strings[i]);
            }

            // (..., ..., ...)
            args_representation_length += (call_expr->arity == 0) ? 2 : 2 * call_expr->arity;
            char *args_representation = malloc(args_representation_length + 1);
            args_representation[0] = '(';

            length_t args_representation_index = 1;
            for(length_t i = 0; i != call_expr->arity; i++){
                length_t arg_string_length = strlen(arg_strings[i]);
                memcpy(&args_representation[args_representation_index], arg_strings[i], arg_string_length);
                args_representation_index += arg_string_length;
                if(i + 1 != call_expr->arity){
                    memcpy(&args_representation[args_representation_index], ", ", 2);
                    args_representation_index += 2;
                }
            }

            memcpy(&args_representation[args_representation_index], ")", 2);
            args_representation_index += 1;

            // name   |   (arg1, arg2, arg3)   |   \0
            length_t name_length = strlen(call_expr->name);
            representation = malloc(name_length + args_representation_length + 1);
            memcpy(representation, call_expr->name, name_length);
            memcpy(&representation[name_length], args_representation, args_representation_length + 1);
            for(length_t i = 0; i != call_expr->arity; i++) free(arg_strings[i]);
            free(arg_strings);
            free(args_representation);
            return representation;
        }
    case EXPR_VARIABLE:
        representation = malloc(strlen(((ast_expr_variable_t*) expr)->name) + 1);
        sprintf(representation, "%s", ((ast_expr_variable_t*) expr)->name);
        return representation;
    case EXPR_MEMBER: {
            char *value_str = ast_expr_str(((ast_expr_member_t*) expr)->value);
            representation = malloc(strlen(value_str) + strlen(((ast_expr_member_t*) expr)->member) + 2);
            sprintf(representation, "%s.%s", value_str, ((ast_expr_member_t*) expr)->member);
            free(value_str);
            return representation;
        }
    case EXPR_ADDRESS: {
            char *value_str = ast_expr_str(((ast_expr_unary_t*) expr)->value);
            representation = malloc(strlen(value_str) + 2);
            sprintf(representation, "&%s", value_str);
            free(value_str);
            return representation;
        }
    case EXPR_FUNC_ADDR: {
            ast_expr_func_addr_t *func_addr_expr = (ast_expr_func_addr_t*) expr;
            representation = malloc(strlen(func_addr_expr->name) + 7);
            sprintf(representation, "func &%s", func_addr_expr->name);
            return representation;
        }
        break;
    case EXPR_DEREFERENCE: {
            char *value_str = ast_expr_str(((ast_expr_unary_t*) expr)->value);
            representation = malloc(strlen(value_str) + 4);
            sprintf(representation, "*(%s)", value_str);
            free(value_str);
            return representation;
        }
    case EXPR_ARRAY_ACCESS: {
            char *value_str1 = ast_expr_str(((ast_expr_array_access_t*) expr)->value);
            char *value_str2 = ast_expr_str(((ast_expr_array_access_t*) expr)->index);
            representation = malloc(strlen(value_str1) + strlen(value_str2) + 3);
            sprintf(representation, "%s[%s]", value_str1, value_str2);
            free(value_str1);
            free(value_str2);
            return representation;
        }
    case EXPR_AT: {
            char *value_str1 = ast_expr_str(((ast_expr_array_access_t*) expr)->value);
            char *value_str2 = ast_expr_str(((ast_expr_array_access_t*) expr)->index);
            representation = malloc(strlen(value_str1) + strlen(value_str2) + 3);
            sprintf(representation, "%s at (%s)", value_str1, value_str2);
            free(value_str1);
            free(value_str2);
            return representation;
        }
    case EXPR_CAST: {
            char *type_str = ast_type_str(&((ast_expr_cast_t*) expr)->to);
            char *value_str = ast_expr_str(((ast_expr_cast_t*) expr)->from);
            representation = malloc(strlen(type_str) + strlen(value_str) + 9);
            sprintf(representation, "cast %s (%s)", type_str, value_str);
            free(type_str);
            free(value_str);
            return representation;
        }
    case EXPR_SIZEOF: {
            char *type_str = ast_type_str(&((ast_expr_sizeof_t*) expr)->type);
            representation = malloc(strlen(type_str) + 8);
            sprintf(representation, "sizeof %s", type_str);
            free(type_str);
            return representation;
        }
    case EXPR_CALL_METHOD: {
            // CLEANUP: This code is kinda scrappy, but hey it works

            ast_expr_call_method_t *call_expr = (ast_expr_call_method_t*) expr;
            char **arg_strings = malloc(sizeof(char*) * call_expr->arity);
            length_t args_representation_length = 0;

            for(length_t i = 0; i != call_expr->arity; i++){
                arg_strings[i] = ast_expr_str(call_expr->args[i]);
                args_representation_length += strlen(arg_strings[i]);
            }

            char *value_rep = ast_expr_str(call_expr->value);
            length_t value_rep_length = strlen(value_rep);

            // (..., ..., ...)
            args_representation_length += (call_expr->arity == 0) ? 2 : 2 * call_expr->arity;
            char *args_representation = malloc(args_representation_length + 1);

            length_t args_representation_index = 0;
            args_representation[args_representation_index++] = '(';

            for(length_t i = 0; i != call_expr->arity; i++){
                length_t arg_string_length = strlen(arg_strings[i]);
                memcpy(&args_representation[args_representation_index], arg_strings[i], arg_string_length);
                args_representation_index += arg_string_length;
                if(i + 1 != call_expr->arity){
                    memcpy(&args_representation[args_representation_index], ", ", 2);
                    args_representation_index += 2;
                }
            }

            memcpy(&args_representation[args_representation_index], ")", 2);
            args_representation_index += 1;

            // name   |   (arg1, arg2, arg3)   |   \0
            length_t name_length = strlen(call_expr->name);
            representation = malloc(value_rep_length + 1 + name_length + args_representation_length + 1);
            sprintf(representation, "%s.%s%s", value_rep, call_expr->name, args_representation);
            for(length_t i = 0; i != call_expr->arity; i++) free(arg_strings[i]);
            free(value_rep);
            free(arg_strings);
            free(args_representation);
            return representation;
        }
    case EXPR_NOT: {
            char *value_str = ast_expr_str(((ast_expr_unary_t*) expr)->value);
            representation = malloc(strlen(value_str) + 4);
            sprintf(representation, "!(%s)", value_str);
            free(value_str);
            return representation;
        }
    case EXPR_NEW: {
            char *type_str = ast_type_str(&((ast_expr_new_t*) expr)->type);

            if( ((ast_expr_new_t*) expr)->amount == NULL ){
                representation = malloc(strlen(type_str) + 5);
                sprintf(representation, "new %s", type_str);
            } else {
                char *a_str = ast_expr_str(((ast_expr_new_t*) expr)->amount);
                representation = malloc(strlen(a_str) + strlen(type_str) + 10);
                sprintf(representation, "new %s * (%s)", type_str, a_str);
                free(a_str);
            }

            free(type_str);
            return representation;
        }
    case EXPR_NEW_CSTRING: {
            char *value = ((ast_expr_new_cstring_t*) expr)->value;
            representation = malloc(strlen(value) + 7);
            sprintf(representation, "new '%s'", value);
            return representation;
        }
    case EXPR_ENUM_VALUE: {
            const char *enum_name = ((ast_expr_enum_value_t*) expr)->enum_name;
            const char *kind_name = ((ast_expr_enum_value_t*) expr)->kind_name;
            if(enum_name == NULL) enum_name = "<contextual>";

            representation = malloc(strlen(enum_name) + strlen(kind_name) + 3);
            sprintf(representation, "%s::%s", enum_name, kind_name);
            return representation;
        }
    case EXPR_BIT_COMPLEMENT: {
            char *value_str = ast_expr_str(((ast_expr_unary_t*) expr)->value);
            representation = malloc(strlen(value_str) + 4);
            sprintf(representation, "~(%s)", value_str);
            free(value_str);
            return representation;
        }
    case EXPR_NEGATE: {
            char *value_str = ast_expr_str(((ast_expr_unary_t*) expr)->value);
            representation = malloc(strlen(value_str) + 4);
            sprintf(representation, "-(%s)", value_str);
            free(value_str);
            return representation;
        }
    case EXPR_STATIC_ARRAY: {
            char *type_str = ast_type_str( &(((ast_expr_static_data_t*) expr)->type) );
            representation = malloc(strlen(type_str) + 14);
            sprintf(representation, "static %s {...}", type_str);
            free(type_str);
            return representation;
        }
    case EXPR_STATIC_STRUCT: {
            char *type_str = ast_type_str( &(((ast_expr_static_data_t*) expr)->type) );
            representation = malloc(strlen(type_str) + 14);
            sprintf(representation, "static %s (...)", type_str);
            free(type_str);
            return representation;
        }
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            bool is_undef = (expr->id == EXPR_ILDECLAREUNDEF);

            ast_expr_declare_t *declaration = (ast_expr_declare_t*) expr;
            char *typename = ast_type_str(&declaration->type);
            char *value = NULL;

            if(declaration->value){
                value = ast_expr_str(declaration->value);
            }

            representation = malloc(strlen(declaration->name) + strlen(typename) + (value ? strlen(value) + 3: 0) + 8);

            if(declaration->value){
                sprintf(representation, "%s %s %s = %s", is_undef ? "undef" : "def", declaration->name, typename, value);
                free(value);
            } else {
                sprintf(representation, "%s %s %s", is_undef ? "undef" : "def", declaration->name, typename);
            }
            
            free(typename);
            return representation;
        }
    default:
        representation = malloc(21);
        memcpy(representation, "<unknown expression>", 21);
        return representation;
    }

    return NULL; // Should never be reached
}

void ast_expr_free(ast_expr_t *expr){
    if(!expr) return;

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
        ast_expr_free_fully( ((ast_expr_math_t*) expr)->a );
        ast_expr_free_fully( ((ast_expr_math_t*) expr)->b );
        break;
    case EXPR_CALL:
        ast_exprs_free_fully(((ast_expr_call_t*) expr)->args, ((ast_expr_call_t*) expr)->arity);
        break;
    case EXPR_MEMBER:
        ast_expr_free_fully( ((ast_expr_member_t*) expr)->value );
        free(((ast_expr_member_t*) expr)->member);
        break;
    case EXPR_ARRAY_ACCESS:
        ast_expr_free_fully( ((ast_expr_array_access_t*) expr)->value );
        ast_expr_free_fully( ((ast_expr_array_access_t*) expr)->index );
        break;
    case EXPR_AT:
        ast_expr_free_fully( ((ast_expr_array_access_t*) expr)->value );
        ast_expr_free_fully( ((ast_expr_array_access_t*) expr)->index );
        break;
    case EXPR_CAST:
        ast_type_free( &((ast_expr_cast_t*) expr)->to );
        ast_expr_free_fully( ((ast_expr_cast_t*) expr)->from );
        break;
    case EXPR_SIZEOF:
        ast_type_free( &((ast_expr_sizeof_t*) expr)->type );
        break;
    case EXPR_CALL_METHOD:
        ast_expr_free_fully( ((ast_expr_call_method_t*) expr)->value );
        ast_exprs_free_fully(((ast_expr_call_method_t*) expr)->args, ((ast_expr_call_method_t*) expr)->arity);
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NOT:
    case EXPR_NEGATE:
    case EXPR_DELETE:
        ast_expr_free_fully( ((ast_expr_unary_t*) expr)->value );
        break;
    case EXPR_NEW:
        ast_type_free( &((ast_expr_new_t*) expr)->type );
        if(((ast_expr_new_t*) expr)->amount != NULL) ast_expr_free_fully( ((ast_expr_new_t*) expr)->amount );
        break;
    case EXPR_STATIC_ARRAY: case EXPR_STATIC_STRUCT: {
            ast_type_free( &((ast_expr_static_data_t*) expr)->type );
            ast_exprs_free_fully(((ast_expr_static_data_t*) expr)->values, ((ast_expr_static_data_t*) expr)->length);
        }
        break;
    case EXPR_RETURN:
        if(((ast_expr_return_t*) expr)->value != NULL){
            ast_expr_free_fully( ((ast_expr_return_t*) expr)->value );
        }
        break;
    case EXPR_DECLARE: case EXPR_ILDECLARE:
        ast_type_free(&((ast_expr_declare_t*) expr)->type);
        if(((ast_expr_declare_t*) expr)->value != NULL){
            ast_expr_free_fully(((ast_expr_declare_t*) expr)->value);
        }
        break;
    case EXPR_DECLAREUNDEF: case EXPR_ILDECLAREUNDEF:
        ast_type_free(&((ast_expr_declare_t*) expr)->type);
        break;
    case EXPR_ASSIGN: case EXPR_ADDASSIGN: case EXPR_SUBTRACTASSIGN:
    case EXPR_MULTIPLYASSIGN: case EXPR_DIVIDEASSIGN: case EXPR_MODULUSASSIGN:
        ast_expr_free_fully(((ast_expr_assign_t*) expr)->destination);
        ast_expr_free_fully(((ast_expr_assign_t*) expr)->value);
        break;
    case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL:
        ast_expr_free_fully(((ast_expr_if_t*) expr)->value);
        ast_free_statements_fully(((ast_expr_if_t*) expr)->statements, ((ast_expr_if_t*) expr)->statements_length);
        break;
    case EXPR_IFELSE: case EXPR_UNLESSELSE:
        ast_expr_free_fully(((ast_expr_ifelse_t*) expr)->value);
        ast_free_statements_fully(((ast_expr_ifelse_t*) expr)->statements, ((ast_expr_ifelse_t*) expr)->statements_length);
        ast_free_statements_fully(((ast_expr_ifelse_t*) expr)->else_statements, ((ast_expr_ifelse_t*) expr)->else_statements_length);
        break;
    case EXPR_WHILECONTINUE: case EXPR_UNTILBREAK:
        ast_free_statements_fully(((ast_expr_whilecontinue_t*) expr)->statements, ((ast_expr_whilecontinue_t*) expr)->statements_length);
        break;
    case EXPR_EACH_IN:
        free(((ast_expr_each_in_t*) expr)->it_name);
        if(((ast_expr_each_in_t*) expr)->it_type){
            ast_type_free_fully(((ast_expr_each_in_t*) expr)->it_type);
        }
        ast_expr_free_fully(((ast_expr_each_in_t*) expr)->low_array);
        ast_expr_free_fully(((ast_expr_each_in_t*) expr)->length);
        ast_free_statements_fully(((ast_expr_each_in_t*) expr)->statements, ((ast_expr_each_in_t*) expr)->statements_length);
        break;
    case EXPR_REPEAT:
        ast_expr_free_fully(((ast_expr_repeat_t*) expr)->limit);
        ast_free_statements_fully(((ast_expr_repeat_t*) expr)->statements, ((ast_expr_repeat_t*) expr)->statements_length);
        break;
    }
}

void ast_expr_free_fully(ast_expr_t *expr){
    if(!expr) return;

    ast_expr_free(expr);
    free(expr);
}

void ast_exprs_free(ast_expr_t **exprs, length_t length){
    for(length_t e = 0; e != length; e++){
        ast_expr_free(exprs[e]);
    }
}

void ast_exprs_free_fully(ast_expr_t **exprs, length_t length){
    for(length_t e = 0; e != length; e++){
        ast_expr_free(exprs[e]);
        free(exprs[e]);
    }
    free(exprs);
}

ast_expr_t *ast_expr_clone(ast_expr_t* expr){
    // NOTE: Exclusive statement expressions are currently unimplemented
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
    case EXPR_FLOAT:         MACRO_VALUE_CLONE(ast_expr_float_t); break;
    case EXPR_DOUBLE:        MACRO_VALUE_CLONE(ast_expr_double_t); break;
    case EXPR_BOOLEAN:       MACRO_VALUE_CLONE(ast_expr_boolean_t); break;
    case EXPR_GENERIC_INT:   MACRO_VALUE_CLONE(ast_expr_generic_int_t); break;
    case EXPR_GENERIC_FLOAT: MACRO_VALUE_CLONE(ast_expr_generic_float_t); break;
    case EXPR_CSTR:          MACRO_VALUE_CLONE(ast_expr_cstr_t); break;
    case EXPR_NULL: break;
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
        clone = malloc(sizeof(ast_expr_math_t));
        ((ast_expr_math_t*) clone)->a = ast_expr_clone(((ast_expr_math_t*) expr)->a);
        ((ast_expr_math_t*) clone)->b = ast_expr_clone(((ast_expr_math_t*) expr)->b);
        break;
    case EXPR_CALL:
        clone = malloc(sizeof(ast_expr_call_t));
        ((ast_expr_call_t*) clone)->name = ((ast_expr_call_t*) expr)->name;
        ((ast_expr_call_t*) clone)->args = malloc(sizeof(ast_expr_t*) * ((ast_expr_call_t*) expr)->arity);
        ((ast_expr_call_t*) clone)->arity = ((ast_expr_call_t*) expr)->arity;

        for(length_t i = 0; i != ((ast_expr_call_t*) expr)->arity; i++){
            ((ast_expr_call_t*) clone)->args[i] = ast_expr_clone(((ast_expr_call_t*) expr)->args[i]);
        }
        break;
    case EXPR_VARIABLE:
        clone = malloc(sizeof(ast_expr_variable_t));
        ((ast_expr_variable_t*) clone)->name = ((ast_expr_variable_t*) expr)->name;
    case EXPR_MEMBER:
        clone = malloc(sizeof(ast_expr_member_t));
        ((ast_expr_member_t*) clone)->value = ast_expr_clone(((ast_expr_member_t*) expr)->value);
        ((ast_expr_member_t*) clone)->member = malloc(strlen(((ast_expr_member_t*) expr)->member) + 1);
        strcpy(((ast_expr_member_t*) clone)->member, ((ast_expr_member_t*) clone)->member);
        break;
    case EXPR_ADDRESS:
    case EXPR_DEREFERENCE:
    case EXPR_BIT_COMPLEMENT:
    case EXPR_NOT:
    case EXPR_NEGATE:
        clone = malloc(sizeof(ast_expr_unary_t));
        ((ast_expr_unary_t*) clone)->value = ast_expr_clone(((ast_expr_unary_t*) expr)->value);
        break;
    case EXPR_FUNC_ADDR: {
            clone = malloc(sizeof(ast_expr_func_addr_t));

            #define macro_clone_ref (((ast_expr_func_addr_t*) clone))
            #define macro_expr_ref (((ast_expr_func_addr_t*) expr))

            macro_clone_ref->name = macro_expr_ref->name;
            macro_clone_ref->traits = macro_expr_ref->traits;
            macro_clone_ref->match_args_length = macro_expr_ref->match_args_length;
            if(macro_expr_ref->match_args == NULL){
                macro_clone_ref->match_args = NULL;
            } else {
                macro_clone_ref->match_args =
                    malloc(sizeof(ast_unnamed_arg_t) * macro_expr_ref->match_args_length);

                for(length_t a = 0; a != macro_clone_ref->match_args_length; a++){
                    macro_clone_ref->match_args[a].type = ast_type_clone(&macro_expr_ref->match_args[a].type);
                    macro_clone_ref->match_args[a].source = macro_expr_ref->match_args[a].source;
                    macro_clone_ref->match_args[a].flow = macro_expr_ref->match_args[a].flow;
                }
            }

            #undef macro_clone_ref
            #undef macro_expr_ref
        }
        break;
    case EXPR_ARRAY_ACCESS:
        clone = malloc(sizeof(ast_expr_array_access_t));
        ((ast_expr_array_access_t*) clone)->value = ast_expr_clone(((ast_expr_array_access_t*) expr)->value);
        ((ast_expr_array_access_t*) clone)->index = ast_expr_clone(((ast_expr_array_access_t*) expr)->index);
        break;
    case EXPR_AT:
        clone = malloc(sizeof(ast_expr_array_access_t));
        ((ast_expr_array_access_t*) clone)->value = ast_expr_clone(((ast_expr_array_access_t*) expr)->value);
        ((ast_expr_array_access_t*) clone)->index = ast_expr_clone(((ast_expr_array_access_t*) expr)->index);
        break;
    case EXPR_CAST:
        clone = malloc(sizeof(ast_expr_cast_t));
        ((ast_expr_cast_t*) clone)->from = ast_expr_clone(((ast_expr_cast_t*) expr)->from);
        ((ast_expr_cast_t*) clone)->to = ast_type_clone(&((ast_expr_cast_t*) expr)->to);
        break;
    case EXPR_SIZEOF:
        clone = malloc(sizeof(ast_expr_sizeof_t));
        ((ast_expr_sizeof_t*) clone)->type = ast_type_clone(&((ast_expr_sizeof_t*) expr)->type);
        break;
    case EXPR_CALL_METHOD:
        clone = malloc(sizeof(ast_expr_call_method_t));
        ((ast_expr_call_method_t*) clone)->name = ((ast_expr_call_method_t*) expr)->name;
        ((ast_expr_call_method_t*) clone)->args = malloc(sizeof(ast_expr_t*) * ((ast_expr_call_method_t*) expr)->arity);
        ((ast_expr_call_method_t*) clone)->arity = ((ast_expr_call_method_t*) expr)->arity;
        ((ast_expr_call_method_t*) clone)->value = ast_expr_clone(((ast_expr_call_method_t*) expr)->value);

        for(length_t i = 0; i != ((ast_expr_call_method_t*) expr)->arity; i++){
            ((ast_expr_call_method_t*) clone)->args[i] = ast_expr_clone(((ast_expr_call_method_t*) expr)->args[i]);
        }
        break;
    case EXPR_NEW:
        clone = malloc(sizeof(ast_expr_new_t));
        ((ast_expr_new_t*) clone)->type = ast_type_clone(&((ast_expr_new_t*) expr)->type);

        if(((ast_expr_new_t*) expr)->amount == NULL) ((ast_expr_new_t*) clone)->amount = NULL;
        else ((ast_expr_new_t*) clone)->amount = ast_expr_clone(((ast_expr_new_t*) expr)->amount);
        break;
    case EXPR_NEW_CSTRING:
        clone = malloc(sizeof(ast_expr_new_cstring_t));
        ((ast_expr_new_cstring_t*) clone)->value = ((ast_expr_new_cstring_t*) expr)->value;
        break;
    case EXPR_STATIC_ARRAY: case EXPR_STATIC_STRUCT:
        clone = malloc(sizeof(ast_expr_static_data_t));
        ((ast_expr_static_data_t*) clone)->type = ast_type_clone(&((ast_expr_static_data_t*) expr)->type);
        ((ast_expr_static_data_t*) clone)->values = malloc(sizeof(ast_expr_t*) * ((ast_expr_static_data_t*) expr)->length);
        ((ast_expr_static_data_t*) clone)->length = ((ast_expr_static_data_t*) expr)->length;

        for(length_t i = 0; i != ((ast_expr_static_data_t*) expr)->length; i++){
            ((ast_expr_static_data_t*) clone)->values[i] = ast_expr_clone(((ast_expr_static_data_t*) expr)->values[i]);
        }
        break;
    default:
        redprintf("INTERNAL ERROR: ast_expr_clone received unimplemented expression id 0x%08X\n", expr->id);
        redprintf("Returning NULL... a crash will probably follow\n");
        return NULL;
    }

    clone->id = expr->id;
    clone->source = expr->source;
    return clone;
}

void ast_expr_create_bool(ast_expr_t **out_expr, bool value, source_t source){
    *out_expr = malloc(sizeof(ast_expr_boolean_t));
    ((ast_expr_boolean_t*) *out_expr)->id = EXPR_BOOLEAN;
    ((ast_expr_boolean_t*) *out_expr)->value = value;
    ((ast_expr_boolean_t*) *out_expr)->source = source;
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

void ast_expr_create_call(ast_expr_t **out_expr, weak_cstr_t name, length_t arity, ast_expr_t **args, source_t source){
    *out_expr = malloc(sizeof(ast_expr_call_t));
    ((ast_expr_call_t*) *out_expr)->id = EXPR_CALL;
    ((ast_expr_call_t*) *out_expr)->name = name;
    ((ast_expr_call_t*) *out_expr)->arity = arity;
    ((ast_expr_call_t*) *out_expr)->args = args;
    ((ast_expr_call_t*) *out_expr)->source = source;
}

void ast_expr_create_enum_value(ast_expr_t **out_expr, weak_cstr_t name, weak_cstr_t kind, source_t source){
    *out_expr = malloc(sizeof(ast_expr_enum_value_t));
    ((ast_expr_enum_value_t*) *out_expr)->id = EXPR_ENUM_VALUE;
    ((ast_expr_enum_value_t*) *out_expr)->enum_name = name;
    ((ast_expr_enum_value_t*) *out_expr)->kind_name = kind;
    ((ast_expr_enum_value_t*) *out_expr)->source = source;
}

void ast_expr_create_cast(ast_expr_t **out_expr, ast_type_t to, ast_expr_t *from, source_t source){
    *out_expr = malloc(sizeof(ast_expr_cast_t));
    ((ast_expr_cast_t*) *out_expr)->id = EXPR_CAST;
    ((ast_expr_cast_t*) *out_expr)->to = to;
    ((ast_expr_cast_t*) *out_expr)->from = from;
    ((ast_expr_cast_t*) *out_expr)->source = source;
}

void ast_expr_list_init(ast_expr_list_t *list, length_t capacity){
    if(capacity == 0){
        list->statements = NULL;
    } else {
        list->statements = malloc(sizeof(ast_expr_t*) * capacity);
    }
    list->length = 0;
    list->capacity = capacity;
}

const char *global_expression_rep_table[] = {
    "<none>",                  // 0x00000000
    "<byte literal>",          // 0x00000001
    "<ubyte literal>",         // 0x00000002
    "<short literal>",         // 0x00000003
    "<ushort literal>",        // 0x00000004
    "<int literal>",           // 0x00000005
    "<uint literal>",          // 0x00000006
    "<long literal>",          // 0x00000007
    "<ulong literal>",         // 0x00000008
    "<float literal>",         // 0x00000009
    "<double literal>",        // 0x0000000A
    "<boolean literal>",       // 0x0000000B
    "<string literal>",        // 0x0000000C
    "<length string literal>", // 0x0000000D
    "null",                    // 0x0000000E
    "<generic int>",           // 0x0000000F
    "<generic float>",         // 0x00000010
    "+",                       // 0x00000011
    "-",                       // 0x00000012
    "*",                       // 0x00000013
    "/",                       // 0x00000014
    "%",                       // 0x00000015
    "==",                      // 0x00000016
    "!=",                      // 0x00000017
    ">",                       // 0x00000018
    "<",                       // 0x00000019
    ">=",                      // 0x0000001A
    "<=",                      // 0x0000001B
    "&&",                      // 0x0000001C
    "||",                      // 0x0000001D
    "!",                       // 0x0000001E
    "&",                       // 0x0000001F
    "|",                       // 0x00000020
    "^",                       // 0x00000021
    "~",                       // 0x00000022
    "<<",                      // 0x00000023
    ">>",                      // 0x00000024
    "<<<",                     // 0x00000025
    ">>>",                     // 0x00000026
    "-",                       // 0x00000027
    "<at>",                    // 0x00000028
    "<call>",                  // 0x00000029
    "<variable>",              // 0x0000002A
    ".",                       // 0x0000002B
    "&",                       // 0x0000002C
    "func&",                   // 0x0000002D
    "*",                       // 0x0000002E
    "[]",                      // 0x0000002F
    "<cast>",                  // 0x00000030
    "<sizeof>",                // 0x00000031
    "<call method>",           // 0x00000032
    "<new>",                   // 0x00000033
    "<new cstring>",           // 0x00000034
    "<enum value>",            // 0x00000035
    "<enum value>",            // 0x00000036
    "<static array>",          // 0x00000037
    "<undef declaration>",     // 0x00000038
    "=",                       // 0x00000039
    "+=",                      // 0x0000003A
    "-=",                      // 0x0000003B
    "*=",                      // 0x0000003C
    "/=",                      // 0x0000003D
    "%=",                      // 0x0000003E
    "<return>",                // 0x0000003F
    "<if>",                    // 0x00000040
    "<unless>",                // 0x00000041
    "<if else>",               // 0x00000042
    "<unless else>",           // 0x00000043
    "<while>",                 // 0x00000044
    "<until>",                 // 0x00000045
    "<while continue>",        // 0x00000046
    "<until break>",           // 0x00000047
    "<each in>",               // 0x00000048
    "<repeat>",                // 0x00000049
    "<delete>",                // 0x0000004A
    "<break>",                 // 0x0000004B
    "<continue>",              // 0x0000004C
    "<break to>",              // 0x0000004D
    "<continue to>",           // 0x0000004E
};
