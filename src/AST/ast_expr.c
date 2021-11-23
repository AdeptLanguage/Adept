
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"

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

static strong_cstr_t ast_expr_ubyte_to_str(ast_expr_ubyte_t *ubyte_expr){
    // Converts a literal ubyte expression into a human-readable string
    // ast_expr_ubyte_t   --->   "0x0Aub"

    strong_cstr_t result = malloc(7);
    sprintf(result, "0x%02Xub", (unsigned int) ubyte_expr->value);
    return result;
}

static strong_cstr_t ast_expr_cstr_to_str(ast_expr_cstr_t *cstr_expr){
    // Converts a literal c-string expression into a human-readable string
    // ast_expr_cstr_t   --->   "'Hello World'"

    // Raw contents and length of contents of c-string
    char *contents = cstr_expr->value;
    length_t contents_length = strlen(contents);

    // Next index into local buffer to write characters into
    length_t put_index = 1;

    // Number of characters in raw string that will need to be escaped
    length_t special_characters = 0;

    // Count number of characters in c-string that will need
    // to be escaped
    for(length_t i = 0; i != contents_length; i++)
        if(contents[i] <= 0x1F || contents[i] == '\\')
            special_characters++;

    // Allocate space for stringified result
    strong_cstr_t result = malloc(contents_length + special_characters + 3);

    // First character in result is always '\''
    result[0] = '\'';

    for(length_t c = 0; c != contents_length; c++){
        // If the it's a normal character, just copy it into the result buffer
        if(contents[c] > 0x1F && contents[c] != '\\'){
            result[put_index++] = contents[c];
            continue;
        }
        
        // Otherwise, write the corresponding escape sequence
        result[put_index++] = '\\';

        switch(contents[c]){
        case '\n': result[put_index++] = 'n';  break;
        case '\r': result[put_index++] = 'r';  break;
        case '\t': result[put_index++] = 't';  break;
        case '\b': result[put_index++] = 'b';  break;
        case '\e': result[put_index++] = 'e';  break;
        case '\'': result[put_index++] = '\''; break;
        case '\\': result[put_index++] = '\\'; break;
        default:   result[put_index++] = '?';  break;
        }
    }

    // Last character in result is always '\''
    result[put_index++] = '\'';

    // And we must terminate it, since we plan on using it as a null-terminated string
    result[put_index++] = '\0';

    // Return ownership of allocated result buffer
    return result;
}

static strong_cstr_t ast_expr_math_to_str(ast_expr_math_t *expr){
    strong_cstr_t a = ast_expr_str(expr->a);
    strong_cstr_t b = ast_expr_str(expr->b);
    strong_cstr_t operator = NULL;

    switch(expr->id){
    case EXPR_ADD:            operator = "+";  break;
    case EXPR_SUBTRACT:       operator = "-";  break;
    case EXPR_MULTIPLY:       operator = "*";  break;
    case EXPR_DIVIDE:         operator = "/";  break;
    case EXPR_MODULUS:        operator = "%%"; break;
    case EXPR_EQUALS:         operator = "=="; break;
    case EXPR_NOTEQUALS:      operator = "!="; break;
    case EXPR_GREATER:        operator = ">";  break;
    case EXPR_LESSER:         operator = "<";  break;
    case EXPR_GREATEREQ:      operator = ">="; break;
    case EXPR_LESSEREQ:       operator = "<="; break;
    case EXPR_AND:            operator = "&&"; break;
    case EXPR_OR:             operator = "||"; break;
    case EXPR_BIT_AND:        operator = "&";  break;
    case EXPR_BIT_OR:         operator = "|";  break;
    case EXPR_BIT_XOR:        operator = "^";  break;
    case EXPR_BIT_LSHIFT:     operator = "<<"; break;
    case EXPR_BIT_RSHIFT:     operator = ">>"; break;
    case EXPR_BIT_LGC_LSHIFT: operator = "<<<"; break;
    case EXPR_BIT_LGC_RSHIFT: operator = ">>>"; break;
    default:                  operator = "¿";
    }

    // Create string representation for math expression
    strong_cstr_t result = mallocandsprintf("(%s %s %s)", a, operator, b);

    // Free temporary heap-allocated string representations of operands
    free(a);
    free(b);

    // Return ownership of allocated result C-string
    return result;
}

static strong_cstr_t ast_expr_values_to_str(ast_expr_t **arg_values, length_t arity){
    // DANGEROUS: This function contains bare-bones string manipulation
    // Modify with caution

    // Short-term cache for string representations of values
    strong_cstr_t *args = malloc(sizeof(char*) * arity); 
    length_t *arg_lengths = malloc(sizeof(length_t) * arity); 

    // Calculate resulting buffer size
    length_t result_buffer_size = 0;

    // Each value will be contained in result, so enough space is
    // needed for all of them
    for(length_t i = 0; i != arity; i++){
        args[i] = ast_expr_str(arg_values[i]);
        arg_lengths[i] = strlen(args[i]);
        result_buffer_size += arg_lengths[i];
    }

    // We also need one character for '\0', and also max(0, arity - 1) ','s
    result_buffer_size += 1 + (arity <= 1 ? 0 : 2 * (arity - 1));

    // Now that we now the required capacity,
    // Allocate buffer to store resulting string representation of value list
    strong_cstr_t result = malloc(result_buffer_size);

    // Index to write into buffer
    length_t put_index = 0;

    // Concatenate args onto value list representation
    for(length_t i = 0; i != arity; i++){
        // Append argument
        memcpy(&result[put_index], args[i], arg_lengths[i]);
        put_index += arg_lengths[i];

        // Append ', ' if applicable
        if(i + 1 != arity){
            result[put_index++] = ',';
            result[put_index++] = ' ';
        }
    }

    result[put_index] = '\0';

    freestrs(args, arity);
    free(arg_lengths);
    return result;
}

static strong_cstr_t ast_expr_call_to_str(ast_expr_call_t *call_expr){
    strong_cstr_t gives_return_type = call_expr->gives.elements_length ? ast_type_str(&call_expr->gives) : NULL;
    strong_cstr_t args = ast_expr_values_to_str(call_expr->args, call_expr->arity);
    weak_cstr_t   tentative_flag = call_expr->is_tentative ? "?" : "";

    strong_cstr_t result = gives_return_type ?
        mallocandsprintf("%s%s(%s) ~> %s", call_expr->name, tentative_flag, args, gives_return_type) :
        mallocandsprintf("%s%s(%s)", call_expr->name, tentative_flag, args);

    free(args);
    free(gives_return_type);
    return result;
}

static strong_cstr_t ast_expr_call_method_to_str(ast_expr_call_method_t *call_method_expr){
    strong_cstr_t gives_return_type = call_method_expr->gives.elements_length ? ast_type_str(&call_method_expr->gives) : NULL;
    strong_cstr_t subject = ast_expr_str(call_method_expr->value);
    strong_cstr_t args = ast_expr_values_to_str(call_method_expr->args, call_method_expr->arity);
    weak_cstr_t   tentative_flag = call_method_expr->is_tentative ? "?" : "";
    
    strong_cstr_t result = gives_return_type ?
        mallocandsprintf("%s.%s%s(%s) ~> %s", subject, tentative_flag, call_method_expr->name, args, gives_return_type) :
        mallocandsprintf("%s.%s%s(%s)", subject, tentative_flag, call_method_expr->name, args);

    free(args);
    free(subject);
    free(gives_return_type);
    return result;
}

static strong_cstr_t ast_expr_member_to_str(ast_expr_member_t *member_expr){
    strong_cstr_t subject = ast_expr_str(member_expr->value);
    strong_cstr_t result = mallocandsprintf("%s.%s", subject, member_expr->member);
    free(subject);
    return result;
}

static strong_cstr_t ast_expr_unary_to_str(ast_expr_unary_t *unary_expr, const char *format){
    // USAGE: Expects single '%s' in 'format'
    strong_cstr_t inner = ast_expr_str(unary_expr->value);
    strong_cstr_t result = mallocandsprintf(format, inner);
    free(inner);
    return result;
}

static strong_cstr_t ast_expr_func_addr_to_str(ast_expr_func_addr_t *func_addr_expr){
    // TODO: Include variant that has support for printing "match args"
    return mallocandsprintf("func %s&%s", func_addr_expr->tentative ? "null " : "", func_addr_expr->name);
}

static strong_cstr_t ast_expr_array_access_to_str(ast_expr_array_access_t *array_access_expr, const char *format){
    strong_cstr_t value1 = ast_expr_str(array_access_expr->value);
    strong_cstr_t value2 = ast_expr_str(array_access_expr->index);
    strong_cstr_t result = mallocandsprintf(format, value1, value1);
    free(value1);
    free(value2);
    return result;
}

static strong_cstr_t ast_expr_cast_to_str(ast_expr_cast_t *cast_expr){
    char *type = ast_type_str(&cast_expr->to);
    char *value = ast_expr_str(cast_expr->from);
    strong_cstr_t result = mallocandsprintf("cast %s (%s)", type, value);
    free(type);
    free(value);
    return result;
}

static strong_cstr_t ast_expr_sizeof_like_to_str(ast_type_t *type, const char *kind_name){
    strong_cstr_t typename = ast_type_str(type);
    strong_cstr_t result = mallocandsprintf("%s %s", kind_name, typename);
    free(typename);
    return result;
}

static strong_cstr_t ast_expr_static_data_to_str(ast_expr_static_data_t *data_expr, const char *format){
    // USAGE: Expects two '%s' in 'format'

    strong_cstr_t type = ast_type_str(&data_expr->type);
    strong_cstr_t values = ast_expr_values_to_str(data_expr->values, data_expr->length);
    strong_cstr_t result = mallocandsprintf(format, type, values);
    free(type);
    free(values);
    return result;
}

strong_cstr_t ast_expr_str(ast_expr_t *expr){
    // NOTE: Returns an allocated string representation of the expression
    // TODO: CLEANUP: Cleanup this messy code

    char *representation;

    switch(expr->id){
    case EXPR_BYTE:
        return int8_to_string(
                ((ast_expr_byte_t*) expr)->value,
                "sb" // Integer Suffix
            );
    case EXPR_SHORT:
        return int16_to_string(
                ((ast_expr_short_t*) expr)->value,
                "ss" // Integer Suffix
            );
    case EXPR_USHORT:
        return uint16_to_string(
                ((ast_expr_ushort_t*) expr)->value,
                "us" // Integer Suffix
            );
    case EXPR_INT:
        return int32_to_string(
                ((ast_expr_int_t*) expr)->value,
                "si" // Integer Suffix
            );
    case EXPR_UINT:
        return uint32_to_string(
                ((ast_expr_uint_t*) expr)->value,
                "ui" // Integer Suffix
            );
    case EXPR_LONG:
        return int64_to_string(
                ((ast_expr_long_t*) expr)->value,
                "sl" // Integer Suffix
            );
    case EXPR_ULONG:
        return uint64_to_string(
                ((ast_expr_ulong_t*) expr)->value,
                "ul" // Integer Suffix
            );
    case EXPR_USIZE:
        return uint64_to_string(
                ((ast_expr_usize_t*) expr)->value,
                "uz" // Integer Suffix
            );
    case EXPR_GENERIC_INT:
        return int64_to_string(
                ((ast_expr_generic_int_t*) expr)->value,
                "" // Integer Suffix
            );
    case EXPR_FLOAT:
        return float32_to_string(
                ((ast_expr_float_t*) expr)->value,
                "f" // Floating Point Suffix
            );
    case EXPR_DOUBLE:
        return float64_to_string(
                ((ast_expr_double_t*) expr)->value,
                "d" // Floating Point Suffix
            );
    case EXPR_GENERIC_FLOAT:
        return float64_to_string(
                ((ast_expr_generic_float_t*) expr)->value,
                "" // Floating Point Suffix
            );
    case EXPR_BOOLEAN:
        return strclone(
                ((ast_expr_boolean_t*) expr)->value ? "true" : "false"
            );
    case EXPR_STR:
        return string_to_escaped_string(
                ((ast_expr_str_t*) expr)->array,
                ((ast_expr_str_t*) expr)->length,
                '"'
            );
    case EXPR_NULL:
        return strclone("null");
    case EXPR_CSTR:
        return ast_expr_cstr_to_str((ast_expr_cstr_t*) expr);
    case EXPR_UBYTE:
        return ast_expr_ubyte_to_str((ast_expr_ubyte_t*) expr);
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
        return ast_expr_math_to_str((ast_expr_math_t*) expr);
    case EXPR_CALL:
        return ast_expr_call_to_str((ast_expr_call_t*) expr);
    case EXPR_VARIABLE:
        return strclone(((ast_expr_variable_t*) expr)->name);
    case EXPR_MEMBER:
        return ast_expr_member_to_str((ast_expr_member_t*) expr);
    case EXPR_ADDRESS:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "&%s");
    case EXPR_FUNC_ADDR:
        return ast_expr_func_addr_to_str((ast_expr_func_addr_t*) expr);
    case EXPR_DEREFERENCE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "*(%s)");
    case EXPR_ARRAY_ACCESS:
        return ast_expr_array_access_to_str((ast_expr_array_access_t*) expr, "%s[%s]");
    case EXPR_AT:
        return ast_expr_array_access_to_str((ast_expr_array_access_t*) expr, "%s at (%s)");
    case EXPR_CAST:
        return ast_expr_cast_to_str((ast_expr_cast_t*) expr);
    case EXPR_SIZEOF:
        return ast_expr_sizeof_like_to_str(&((ast_expr_sizeof_t*) expr)->type, "sizeof");
    case EXPR_SIZEOF_VALUE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "sizeof(%s)");
    case EXPR_PHANTOM:
        return strclone("~phantom~");
    case EXPR_CALL_METHOD:
        return ast_expr_call_method_to_str((ast_expr_call_method_t*) expr);
    case EXPR_NOT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "!(%s)");
    case EXPR_NEW: {
            char *type_str = ast_type_str(&((ast_expr_new_t*) expr)->type);

            if( ((ast_expr_new_t*) expr)->amount == NULL ){
                representation = mallocandsprintf("new %s", type_str);
            } else {
                char *a_str = ast_expr_str(((ast_expr_new_t*) expr)->amount);
                representation = mallocandsprintf("new %s * (%s)", type_str, a_str);
                free(a_str);
            }

            free(type_str);
            return representation;
        }
    case EXPR_NEW_CSTRING: {
            char *value = ((ast_expr_new_cstring_t*) expr)->value;
            representation = mallocandsprintf("new '%s'", value);
            return representation;
        }
    case EXPR_ENUM_VALUE: {
            const char *enum_name = ((ast_expr_enum_value_t*) expr)->enum_name;
            const char *kind_name = ((ast_expr_enum_value_t*) expr)->kind_name;
            if(enum_name == NULL) enum_name = "<contextual>";

            representation = mallocandsprintf("%s::%s", enum_name, kind_name);
            return representation;
        }
    case EXPR_BIT_COMPLEMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "~(%s)");
    case EXPR_NEGATE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "-(%s)");
    case EXPR_STATIC_ARRAY:
        return ast_expr_static_data_to_str((ast_expr_static_data_t*) expr, "static %s { %s }");
    case EXPR_STATIC_STRUCT:
        return ast_expr_static_data_to_str((ast_expr_static_data_t*) expr, "static %s ( %s )");
    case EXPR_TYPEINFO: {
            char *type_str = ast_type_str( &(((ast_expr_typeinfo_t*) expr)->target) );
            representation = mallocandsprintf("typeinfo %s", type_str);
            free(type_str);
            return representation;
        }
    case EXPR_TERNARY: {
            char *condition_str = ast_expr_str(((ast_expr_ternary_t*) expr)->condition);
            char *true_str = ast_expr_str(((ast_expr_ternary_t*) expr)->if_true);
            char *false_str = ast_expr_str(((ast_expr_ternary_t*) expr)->if_false);

            representation = mallocandsprintf("(%s ? %s : %s)", condition_str, true_str, false_str);

            free(condition_str);
            free(true_str);
            free(false_str);
            return representation;
        }
    case EXPR_PREINCREMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "++(%s)");
    case EXPR_PREDECREMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "--(%s)");
    case EXPR_POSTINCREMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "(%s)++");
    case EXPR_POSTDECREMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "(%s)--");
    case EXPR_TOGGLE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "(%s)!!");
    case EXPR_VA_ARG: {
            char *value_str = ast_expr_str( ((ast_expr_va_arg_t*) expr)->va_list );
            char *type_str = ast_type_str( &(((ast_expr_va_arg_t*) expr)->arg_type) );
            representation = mallocandsprintf("va_arg(%s, %s)", value_str, type_str);
            free(value_str);
            free(type_str);
            return representation;
        }
    case EXPR_INITLIST: {
            char *compound = NULL;
            length_t compound_length = 0;
            length_t compound_capacity = 0;

            for(length_t i = 0; i != ((ast_expr_initlist_t*) expr)->length; i++){
                char *s = ast_expr_str(((ast_expr_initlist_t*) expr)->elements[i]);
                length_t s_length = strlen(s);

                if(i != 0){
                    expand((void**) &compound, sizeof(char), compound_length, &compound_capacity, 3, 256);
                    memcpy(&compound[compound_length], ", ", 3);
                    compound_length += 2;
                }

                expand((void**) &compound, sizeof(char), compound_length, &compound_capacity, s_length + 1, 256);
                memcpy(&compound[compound_length], s, s_length + 1);
                compound_length += s_length;

                free(s);
            }

            representation = mallocandsprintf("{%s}", compound ? compound : "");
            free(compound);
            return representation;
        }
    case EXPR_POLYCOUNT:
        return mallocandsprintf("$#%s", ((ast_expr_polycount_t*) expr)->name);
    case EXPR_TYPENAMEOF: {
            strong_cstr_t typename = ast_type_str( &(((ast_expr_typenameof_t*) expr)->type) );
            strong_cstr_t result = mallocandsprintf("typenameof %s", typename);
            free(typename);
            return result;
        }
    case EXPR_LLVM_ASM: {
            return strclone("llvm_asm <...> { ... }");
        }
    case EXPR_EMBED: {
            ast_expr_embed_t *embed = (ast_expr_embed_t*) expr;
            strong_cstr_t filename_string_escaped = string_to_escaped_string(embed->filename, strlen(embed->filename), '"');
            strong_cstr_t result = mallocandsprintf("embed %s", filename_string_escaped);

            free(filename_string_escaped);
            return result;
        }
    case EXPR_ALIGNOF:
        return ast_expr_sizeof_like_to_str(&((ast_expr_alignof_t*) expr)->type, "alignof");
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF: {
            bool is_undef = (expr->id == EXPR_ILDECLAREUNDEF);

            ast_expr_declare_t *declaration = (ast_expr_declare_t*) expr;
            char *typename = ast_type_str(&declaration->type);
            char *value = NULL;

            if(declaration->value){
                value = ast_expr_str(declaration->value);
            }

            if(declaration->value){
                representation = mallocandsprintf("%s %s %s = %s", is_undef ? "undef" : "def", declaration->name, typename, value);
                free(value);
            } else {
                representation = mallocandsprintf("%s %s %s", is_undef ? "undef" : "def", declaration->name, typename);
            }
            
            free(typename);
            return representation;
        }
    default:
        return strclone("<unknown expression>");
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

        if(((ast_expr_call_t*) expr)->gives.elements_length != 0){
            ast_type_free(&((ast_expr_call_t*) expr)->gives);
        }
        break;
    case EXPR_VARIABLE:
        // name is a weak_cstr_t so we don't have to worry about it
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
    case EXPR_SIZEOF_VALUE:
        ast_expr_free_fully( ((ast_expr_sizeof_value_t*) expr)->value );
        break;
    case EXPR_PHANTOM:
        ast_type_free( &((ast_expr_phantom_t*) expr)->type );
        break;
    case EXPR_CALL_METHOD:
        ast_expr_free_fully( ((ast_expr_call_method_t*) expr)->value );
        ast_exprs_free_fully(((ast_expr_call_method_t*) expr)->args, ((ast_expr_call_method_t*) expr)->arity);

        if(((ast_expr_call_method_t*) expr)->gives.elements_length != 0){
            ast_type_free(&((ast_expr_call_method_t*) expr)->gives);
        }
        break;
    case EXPR_VA_ARG:
        ast_expr_free_fully( ((ast_expr_va_arg_t*) expr)->va_list );
        ast_type_free( &(((ast_expr_va_arg_t*) expr)->arg_type) );
        break;
    case EXPR_INITLIST:
        ast_exprs_free_fully(((ast_expr_initlist_t*) expr)->elements, ((ast_expr_initlist_t*) expr)->length);
        break;
    case EXPR_POLYCOUNT:
        free(((ast_expr_polycount_t*) expr)->name);
        break;
    case EXPR_TYPENAMEOF:
        ast_type_free( &(((ast_expr_typenameof_t*) expr)->type) );
        break;
    case EXPR_LLVM_ASM:
        free(((ast_expr_llvm_asm_t*) expr)->assembly);
        ast_exprs_free_fully(((ast_expr_llvm_asm_t*) expr)->args, ((ast_expr_llvm_asm_t*) expr)->arity);
        break;
    case EXPR_EMBED:
        free(((ast_expr_embed_t*) expr)->filename);
        break;
    case EXPR_ALIGNOF:
        ast_type_free( &((ast_expr_alignof_t*) expr)->type );
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
        ast_expr_free_fully( ((ast_expr_unary_t*) expr)->value );
        break;
    case EXPR_FUNC_ADDR:
        if(((ast_expr_func_addr_t*) expr)->match_args)
            ast_types_free_fully(((ast_expr_func_addr_t*) expr)->match_args, ((ast_expr_func_addr_t*) expr)->match_args_length);
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
    case EXPR_TYPEINFO: {
            ast_type_free( &((ast_expr_typeinfo_t*) expr)->target );
        }
        break;
    case EXPR_TERNARY:
        ast_expr_free_fully( ((ast_expr_ternary_t*) expr)->condition );
        ast_expr_free_fully( ((ast_expr_ternary_t*) expr)->if_true );
        ast_expr_free_fully( ((ast_expr_ternary_t*) expr)->if_false );
        break;
    case EXPR_RETURN:
        if(((ast_expr_return_t*) expr)->value != NULL){
            ast_expr_free_fully( ((ast_expr_return_t*) expr)->value );
        }
        ast_free_statements_fully(((ast_expr_return_t*) expr)->last_minute.statements, ((ast_expr_return_t*) expr)->last_minute.length);
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
    case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN:
    case EXPR_MULTIPLY_ASSIGN: case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN:
    case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN: case EXPR_XOR_ASSIGN:
    case EXPR_LS_ASSIGN: case EXPR_RS_ASSIGN:
    case EXPR_LGC_LS_ASSIGN: case EXPR_LGC_RS_ASSIGN:
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
        ast_expr_free_fully(((ast_expr_each_in_t*) expr)->list);
        ast_free_statements_fully(((ast_expr_each_in_t*) expr)->statements, ((ast_expr_each_in_t*) expr)->statements_length);
        break;
    case EXPR_REPEAT:
        ast_expr_free_fully(((ast_expr_repeat_t*) expr)->limit);
        ast_free_statements_fully(((ast_expr_repeat_t*) expr)->statements, ((ast_expr_repeat_t*) expr)->statements_length);
        break;
    case EXPR_SWITCH:
        ast_expr_free_fully(((ast_expr_switch_t*) expr)->value);
        ast_free_statements_fully(((ast_expr_switch_t*) expr)->default_statements, ((ast_expr_switch_t*) expr)->default_statements_length);

        for(length_t c = 0; c != ((ast_expr_switch_t*) expr)->cases_length; c++){
            ast_case_t *expr_case = &((ast_expr_switch_t*) expr)->cases[c];
            ast_expr_free_fully(expr_case->condition);
            ast_free_statements_fully(expr_case->statements, expr_case->statements_length);
        }

        free(((ast_expr_switch_t*) expr)->cases);
        break;
    case EXPR_VA_COPY:
        ast_expr_free_fully(((ast_expr_va_copy_t*) expr)->dest_value);
        ast_expr_free_fully(((ast_expr_va_copy_t*) expr)->src_value);
        break;
    case EXPR_FOR: {
            ast_expr_for_t *for_loop = (ast_expr_for_t*) expr;
            ast_free_statements_fully(for_loop->before.statements, for_loop->before.length);
            ast_free_statements_fully(for_loop->after.statements, for_loop->after.length);
            if(for_loop->condition) ast_expr_free_fully(for_loop->condition);
            ast_free_statements_fully(for_loop->statements.statements, for_loop->statements.length);
        }
        break;
    case EXPR_DECLARE_CONSTANT: {
            ast_expr_declare_constant_t *declare_constant = (ast_expr_declare_constant_t*) expr;
            ast_free_constants(&declare_constant->constant, 1);
        }
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
    // NOTE: Exclusively statement expressions are currently unimplemented
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
        #define expr_as_sizeof ((ast_expr_sizeof_t*) expr)
        #define clone_as_sizeof ((ast_expr_sizeof_t*) clone)

        clone = malloc(sizeof(ast_expr_sizeof_t));
        clone_as_sizeof->type = ast_type_clone(&expr_as_sizeof->type);
        break;

        #undef expr_as_sizeof
        #undef clone_as_sizeof
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
    case EXPR_TYPEINFO:
        #define expr_as_typeinfo ((ast_expr_typeinfo_t*) expr)
        #define clone_as_typeinfo ((ast_expr_typeinfo_t*) clone)

        clone = malloc(sizeof(ast_expr_typeinfo_t));
        clone_as_typeinfo->target = ast_type_clone(&expr_as_typeinfo->target);
        break;

        #undef expr_as_typeinfo
        #undef clone_as_typeinfo
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
    case EXPR_TYPENAMEOF:
        #define expr_as_typenameof ((ast_expr_typenameof_t*) expr)
        #define clone_as_typenameof ((ast_expr_typenameof_t*) clone)

        clone = malloc(sizeof(ast_expr_typenameof_t));
        clone_as_typenameof->type = ast_type_clone(&expr_as_typenameof->type);
        break;

        #undef expr_as_typenameof
        #undef clone_as_typenameof
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
    case EXPR_ALIGNOF:
        #define expr_as_alignof ((ast_expr_alignof_t*) expr)
        #define clone_as_alignof ((ast_expr_alignof_t*) clone)

        clone = malloc(sizeof(ast_expr_alignof_t));
        clone_as_alignof->type = ast_type_clone(&expr_as_alignof->type);
        break;

        #undef expr_as_alignof
        #undef clone_as_alignof
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
        clone_as_return->last_minute.length = expr_as_return->last_minute.length;
        clone_as_return->last_minute.capacity = expr_as_return->last_minute.length; // (using 'length' on purpose)
        clone_as_return->last_minute.statements = malloc(sizeof(ast_expr_t*) * expr_as_return->last_minute.length);
        
        for(length_t s = 0; s != expr_as_return->last_minute.length; s++){
            clone_as_return->last_minute.statements[s] = ast_expr_clone(expr_as_return->last_minute.statements[s]);
        }
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
        clone_as_if->statements_length = expr_as_if->statements_length;
        clone_as_if->statements_capacity = expr_as_if->statements_length; // (statements_length is on purpose)
        clone_as_if->statements = malloc(sizeof(ast_expr_t*) * expr_as_if->statements_length);

        for(length_t s = 0; s != expr_as_if->statements_length; s++){
            clone_as_if->statements[s] = ast_expr_clone(expr_as_if->statements[s]);
        }
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
        clone_as_ifelse->statements_length = expr_as_ifelse->statements_length;
        clone_as_ifelse->statements_capacity = expr_as_ifelse->statements_length; // (statements_length is on purpose)
        clone_as_ifelse->statements = malloc(sizeof(ast_expr_t*) * expr_as_ifelse->statements_length);
        clone_as_ifelse->else_statements_length = expr_as_ifelse->else_statements_length;
        clone_as_ifelse->else_statements_capacity = expr_as_ifelse->else_statements_length; // (else_statements_length is on purpose)
        clone_as_ifelse->else_statements = malloc(sizeof(ast_expr_t*) * expr_as_ifelse->else_statements_length);

        for(length_t s = 0; s != expr_as_ifelse->statements_length; s++){
            clone_as_ifelse->statements[s] = ast_expr_clone(expr_as_ifelse->statements[s]);
        }

        for(length_t e = 0; e != expr_as_ifelse->else_statements_length; e++){
            clone_as_ifelse->else_statements[e] = ast_expr_clone(expr_as_ifelse->else_statements[e]);
        }
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
        }

        clone_as_each_in->low_array = expr_as_each_in->low_array ? ast_expr_clone(expr_as_each_in->low_array) : NULL;
        clone_as_each_in->length = expr_as_each_in->length ? ast_expr_clone(expr_as_each_in->length) : NULL;
        clone_as_each_in->list = expr_as_each_in->list ? ast_expr_clone(expr_as_each_in->list) : NULL;
        clone_as_each_in->statements_length = expr_as_each_in->statements_length;
        clone_as_each_in->statements_capacity = expr_as_each_in->statements_length; // (statements_length is on purpose)
        clone_as_each_in->statements = malloc(sizeof(ast_expr_t*) * expr_as_each_in->statements_length);
        clone_as_each_in->is_static = expr_as_each_in->is_static;

        for(length_t s = 0; s != expr_as_each_in->statements_length; s++){
            clone_as_each_in->statements[s] = ast_expr_clone(expr_as_each_in->statements[s]);
        }
        break;

        #undef expr_as_each_in
        #undef clone_as_each_in
    case EXPR_REPEAT:
        #define expr_as_repeat ((ast_expr_repeat_t*) expr)
        #define clone_as_repeat ((ast_expr_repeat_t*) clone)

        clone = malloc(sizeof(ast_expr_repeat_t));
        clone_as_repeat->label = expr_as_repeat->label;
        clone_as_repeat->limit = expr_as_repeat->limit ? ast_expr_clone(expr_as_repeat->limit) : NULL;
        clone_as_repeat->statements_length = expr_as_repeat->statements_length;
        clone_as_repeat->statements_capacity = expr_as_repeat->statements_length; // (statements_length is on purpose)
        clone_as_repeat->statements = malloc(sizeof(ast_expr_t*) * expr_as_repeat->statements_length);
        clone_as_repeat->is_static = expr_as_repeat->is_static;
        clone_as_repeat->idx_overload_name = expr_as_repeat->idx_overload_name;
        
        for(length_t s = 0; s != expr_as_repeat->statements_length; s++){
            clone_as_repeat->statements[s] = ast_expr_clone(expr_as_repeat->statements[s]);
        }
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

        clone_as_switch->cases = malloc(sizeof(ast_case_t) * expr_as_switch->cases_length);
        clone_as_switch->cases_length = expr_as_switch->cases_length;
        clone_as_switch->cases_capacity = expr_as_switch->cases_length; // (on purpose)
        for(length_t c = 0; c != expr_as_switch->cases_length; c++){
            ast_case_t *expr_case = &expr_as_switch->cases[c];
            ast_case_t *clone_case = &clone_as_switch->cases[c];

            clone_case->condition = ast_expr_clone(expr_case->condition);
            clone_case->source = expr_case->source;
            clone_case->statements_length = expr_case->statements_length;
            clone_case->statements_capacity = expr_case->statements_capacity; // (on purpose)

            clone_case->statements = malloc(sizeof(ast_expr_t*) * expr_case->statements_length);
            for(length_t s = 0; s != expr_case->statements_length; s++){
                clone_case->statements[s] = ast_expr_clone(expr_case->statements[s]);
            }
        }

        clone_as_switch->default_statements = malloc(sizeof(ast_expr_t*) * expr_as_switch->default_statements_length);
        for(length_t s = 0; s != expr_as_switch->default_statements_length; s++){
            clone_as_switch->default_statements[s] = ast_expr_clone(expr_as_switch->default_statements[s]);
        }
        
        clone_as_switch->default_statements_length = expr_as_switch->default_statements_length;
        clone_as_switch->default_statements_capacity = expr_as_switch->default_statements_length; // (on purpose)
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
        
        clone_as_for->before.statements = malloc(sizeof(ast_expr_t*) * expr_as_for->before.length);
        clone_as_for->before.length = expr_as_for->before.length;
        clone_as_for->before.capacity = expr_as_for->before.length; // (on purpose)

        for(length_t i = 0; i != expr_as_for->before.length; i++){
            clone_as_for->before.statements[i] = ast_expr_clone(expr_as_for->before.statements[i]);
        }
        
        clone_as_for->after.statements = malloc(sizeof(ast_expr_t*) * expr_as_for->after.length);
        clone_as_for->after.length = expr_as_for->after.length;
        clone_as_for->after.capacity = expr_as_for->after.length; // (on purpose)

        for(length_t i = 0; i != expr_as_for->after.length; i++){
            clone_as_for->after.statements[i] = ast_expr_clone(expr_as_for->after.statements[i]);
        }
        
        clone_as_for->statements.statements = malloc(sizeof(ast_expr_t*) * expr_as_for->statements.length);
        clone_as_for->statements.length = expr_as_for->statements.length;
        clone_as_for->statements.capacity = expr_as_for->statements.length; // (on purpose)

        for(length_t i = 0; i != expr_as_for->statements.length; i++){
            clone_as_for->statements.statements[i] = ast_expr_clone(expr_as_for->statements.statements[i]);
        }
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
    default:
        internalerrorprintf("ast_expr_clone received unimplemented expression id 0x%08X\n", expr->id);
        redprintf("Returning NULL... a crash will probably follow\n");
        return NULL;
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

    if(gives && gives->elements_length != 0)
        out_expr->gives = *gives;
    else
        memset(&out_expr->gives, 0, sizeof(ast_type_t));
    
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

    if(gives && gives->elements_length != 0)
        out_expr->gives = *gives;
    else
        memset(&out_expr->gives, 0, sizeof(ast_type_t));
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

void ast_expr_list_append(ast_expr_list_t *list, ast_expr_t *value){
    expand((void**) &list->statements, sizeof(ast_expr_t*), list->length, &list->capacity, 1, 4);
    list->statements[list->length++] = value;
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
