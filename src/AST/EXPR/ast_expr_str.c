
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/UTIL/string_builder_extensions.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"

/*
    Implementation details of the ast_expr_str(ast_expr_t*) function.
*/

static strong_cstr_t ast_expr_string_to_str(ast_expr_str_t *str_expr){
    return string_to_escaped_string(str_expr->array, str_expr->length, '"', true);
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
    weak_cstr_t contents = cstr_expr->value;
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
        if(contents[c] > 0x1F && contents[c] != '\\'){
            // If the it's a normal character, just copy it into the result buffer
            result[put_index++] = contents[c];
        } else {
            // Otherwise, write the corresponding escape sequence
            result[put_index++] = '\\';
    
            switch(contents[c]){
            case '\n': result[put_index++] = 'n';  break;
            case '\r': result[put_index++] = 'r';  break;
            case '\t': result[put_index++] = 't';  break;
            case '\b': result[put_index++] = 'b';  break;
            case '\'': result[put_index++] = '\''; break;
            case '\\': result[put_index++] = '\\'; break;
            case 0x1B: result[put_index++] = 'e';  break;
            default:   result[put_index++] = '?';  break;
            }
        }
    }

    // Last character in result is always '\''
    result[put_index++] = '\'';

    // And we must terminate it, since we plan on using it as a null-terminated string
    result[put_index++] = '\0';

    // Return ownership of allocated result buffer
    return result;
}

static weak_cstr_t ast_expr_math_get_operator(ast_expr_math_t *expr){
    switch(expr->id){
    case EXPR_ADD:            return "+";
    case EXPR_SUBTRACT:       return "-";
    case EXPR_MULTIPLY:       return "*";
    case EXPR_DIVIDE:         return "/";
    case EXPR_MODULUS:        return "%%";
    case EXPR_EQUALS:         return "==";
    case EXPR_NOTEQUALS:      return "!=";
    case EXPR_GREATER:        return ">";
    case EXPR_LESSER:         return "<";
    case EXPR_GREATEREQ:      return ">=";
    case EXPR_LESSEREQ:       return "<=";
    case EXPR_AND:            return "&&";
    case EXPR_OR:             return "||";
    case EXPR_BIT_AND:        return "&";
    case EXPR_BIT_OR:         return "|";
    case EXPR_BIT_XOR:        return "^";
    case EXPR_BIT_LSHIFT:     return "<<";
    case EXPR_BIT_RSHIFT:     return ">>";
    case EXPR_BIT_LGC_LSHIFT: return "<<<";
    case EXPR_BIT_LGC_RSHIFT: return ">>>";
    }

    return "¿";
}

static strong_cstr_t ast_expr_math_to_str(ast_expr_math_t *expr){
    strong_cstr_t a = ast_expr_str(expr->a);
    strong_cstr_t b = ast_expr_str(expr->b);
    weak_cstr_t operator = ast_expr_math_get_operator(expr);

    strong_cstr_t result = mallocandsprintf("(%s %s %s)", a, operator, b);

    free(a);
    free(b);
    return result;
}

static strong_cstr_t ast_expr_values_to_str(ast_expr_t **arg_values, length_t arity){
    string_builder_t builder;
    string_builder_init(&builder);
    string_builder_append_expr_list(&builder, arg_values, arity, false);
    return string_builder_finalize(&builder);
}

static strong_cstr_t ast_expr_call_to_str(ast_expr_call_t *call_expr){
    string_builder_t builder;
    string_builder_init(&builder);

    string_builder_append(&builder, call_expr->name);

    if(call_expr->is_tentative){
        string_builder_append_char(&builder, '?');
    }

    string_builder_append_expr_list(&builder, call_expr->args, call_expr->arity, true);

    if(call_expr->gives.elements_length > 0){
        string_builder_append(&builder, " ~> ");
        string_builder_append_type(&builder, &call_expr->gives);
    }

    return string_builder_finalize(&builder);
}

static strong_cstr_t ast_expr_call_method_to_str(ast_expr_call_method_t *method_expr){
    string_builder_t builder;
    string_builder_init(&builder);

    string_builder_append_expr(&builder, method_expr->value);
    string_builder_append_char(&builder, '.');

    if(method_expr->is_tentative){
        string_builder_append_char(&builder, '?');
    }

    string_builder_append(&builder, method_expr->name);
    string_builder_append_expr_list(&builder, method_expr->args, method_expr->arity, true);

    if(method_expr->gives.elements_length > 0){
        string_builder_append(&builder, " ~> ");
        string_builder_append_type(&builder, &method_expr->gives);
    }

    return string_builder_finalize(&builder);
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
    string_builder_t builder;
    string_builder_init(&builder);

    string_builder_append(&builder, "func ");

    if(func_addr_expr->tentative){
        string_builder_append(&builder, "null ");
    }
    
    string_builder_append_char(&builder, '&');
    string_builder_append(&builder, func_addr_expr->name);

    if(func_addr_expr->has_match_args){
        string_builder_append_type_list(&builder, func_addr_expr->match_args, func_addr_expr->match_args_length, true);
    }

    return string_builder_finalize(&builder);
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
    strong_cstr_t type = ast_type_str(&cast_expr->to);
    strong_cstr_t value = ast_expr_str(cast_expr->from);
    strong_cstr_t result = mallocandsprintf("cast %s (%s)", type, value);
    free(type);
    free(value);
    return result;
}

static strong_cstr_t ast_expr_unary_type_to_str(ast_expr_unary_type_t *unary_type_expr, const char *keyword){
    strong_cstr_t typename = ast_type_str(&unary_type_expr->type);
    strong_cstr_t result = mallocandsprintf("%s %s", keyword, typename);
    free(typename);
    return result;
}

static strong_cstr_t ast_expr_new_to_str(ast_expr_new_t *new_expr){
    strong_cstr_t type = ast_type_str(&new_expr->type);
    strong_cstr_t result;

    if(new_expr->amount == NULL){
        result = mallocandsprintf("new %s", type);
    } else {
        strong_cstr_t amount = ast_expr_str(new_expr->amount);
        result = mallocandsprintf("new %s * (%s)", type, amount);
        free(amount);
    }

    free(type);
    return result;
}

static strong_cstr_t ast_expr_new_cstring_to_str(ast_expr_new_cstring_t *new_cstring_expr){
    return mallocandsprintf("new '%s'", new_cstring_expr->value);
}

static strong_cstr_t ast_expr_enum_value_to_str(ast_expr_enum_value_t *enum_value_expr){
    return mallocandsprintf("%s::%s", enum_value_expr->enum_name, enum_value_expr->kind_name);
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

static strong_cstr_t ast_expr_ternary_to_str(ast_expr_ternary_t *ternary_expr){
    strong_cstr_t condition_str = ast_expr_str(ternary_expr->condition);
    strong_cstr_t true_str      = ast_expr_str(ternary_expr->if_true);
    strong_cstr_t false_str     = ast_expr_str(ternary_expr->if_false);

    strong_cstr_t result = mallocandsprintf("(%s ? %s : %s)", condition_str, true_str, false_str);

    free(condition_str);
    free(true_str);
    free(false_str);
    return result;
}

static strong_cstr_t ast_expr_va_arg_to_str(ast_expr_va_arg_t *va_arg_expr){
    strong_cstr_t value_str = ast_expr_str(va_arg_expr->va_list);
    strong_cstr_t type_str = ast_type_str(&va_arg_expr->arg_type);

    strong_cstr_t result = mallocandsprintf("va_arg(%s, %s)", value_str, type_str);

    free(value_str);
    free(type_str);
    return result;
}

static strong_cstr_t ast_expr_initlist_to_str(ast_expr_initlist_t *initlist_expr){
    string_builder_t builder;
    string_builder_init(&builder);
    string_builder_append_char(&builder, '{');

    for(length_t i = 0; i != initlist_expr->length; i++){
        if(i != 0){
            string_builder_append(&builder, ", ");
        }

        string_builder_append_expr(&builder, initlist_expr->elements[i]);
    }

    string_builder_append_char(&builder, '}');
    return string_builder_finalize(&builder);
}

static strong_cstr_t ast_expr_embed_to_str(ast_expr_embed_t *embed_expr){
    strong_cstr_t filename_string_escaped = string_to_escaped_string(embed_expr->filename, strlen(embed_expr->filename), '"', true);
    strong_cstr_t result = mallocandsprintf("embed %s", filename_string_escaped);
    free(filename_string_escaped);
    return result;
}

static strong_cstr_t ast_expr_declare_to_str(ast_expr_declare_t *declaration){
    bool is_undef = declaration->id == EXPR_ILDECLAREUNDEF;

    strong_cstr_t typename = ast_type_str(&declaration->type);
    strong_cstr_t value = declaration->value ? ast_expr_str(declaration->value) : NULL;
    strong_cstr_t result;

    weak_cstr_t keyword = is_undef ? "undef" : "def";

    if(value){
        result = mallocandsprintf("%s %s %s = %s", keyword, declaration->name, typename, value);
        free(value);
    } else {
        result = mallocandsprintf("%s %s %s", keyword, declaration->name, typename);
    }
    
    free(typename);
    return result;
}

strong_cstr_t ast_expr_str(ast_expr_t *expr){
    switch(expr->id){
    case EXPR_BYTE:
        return int8_to_string(typecast(ast_expr_byte_t*, expr)->value, "sb");
    case EXPR_SHORT:
        return int16_to_string(typecast(ast_expr_short_t*, expr)->value, "ss");
    case EXPR_USHORT:
        return uint16_to_string(typecast(ast_expr_ushort_t*, expr)->value, "us");
    case EXPR_INT:
        return int32_to_string(typecast(ast_expr_int_t*, expr)->value, "si");
    case EXPR_UINT:
        return uint32_to_string(typecast(ast_expr_uint_t*, expr)->value, "ui");
    case EXPR_LONG:
        return int64_to_string(typecast(ast_expr_long_t*, expr)->value, "sl");
    case EXPR_ULONG:
        return uint64_to_string(typecast(ast_expr_ulong_t*, expr)->value, "ul");
    case EXPR_USIZE:
        return uint64_to_string(typecast(ast_expr_usize_t*, expr)->value, "uz");
    case EXPR_GENERIC_INT:
        return int64_to_string(typecast(ast_expr_generic_int_t*, expr)->value, "");
    case EXPR_FLOAT:
        return float32_to_string(typecast(ast_expr_float_t*, expr)->value, "f");
    case EXPR_DOUBLE:
        return float64_to_string(typecast(ast_expr_double_t*, expr)->value, "d");
    case EXPR_GENERIC_FLOAT:
        return float64_to_string(typecast(ast_expr_generic_float_t*, expr)->value, "");
    case EXPR_BOOLEAN:
        return strclone(bool_to_string(typecast(ast_expr_boolean_t*, expr)->value));
    case EXPR_STR:
        return ast_expr_string_to_str((ast_expr_str_t*) expr);
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
        return strclone(typecast(ast_expr_variable_t*, expr)->name);
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
        return ast_expr_unary_type_to_str((ast_expr_sizeof_t*) expr, "sizeof");
    case EXPR_SIZEOF_VALUE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "sizeof(%s)");
    case EXPR_PHANTOM:
        return strclone("~phantom~");
    case EXPR_CALL_METHOD:
        return ast_expr_call_method_to_str((ast_expr_call_method_t*) expr);
    case EXPR_NOT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "!(%s)");
    case EXPR_NEW:
        return ast_expr_new_to_str((ast_expr_new_t*) expr);
    case EXPR_NEW_CSTRING:
        return ast_expr_new_cstring_to_str((ast_expr_new_cstring_t*) expr);
    case EXPR_ENUM_VALUE:
        return ast_expr_enum_value_to_str((ast_expr_enum_value_t*) expr);
    case EXPR_BIT_COMPLEMENT:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "~(%s)");
    case EXPR_NEGATE:
        return ast_expr_unary_to_str((ast_expr_unary_t*) expr, "-(%s)");
    case EXPR_STATIC_ARRAY:
        return ast_expr_static_data_to_str((ast_expr_static_data_t*) expr, "static %s { %s }");
    case EXPR_STATIC_STRUCT:
        return ast_expr_static_data_to_str((ast_expr_static_data_t*) expr, "static %s ( %s )");
    case EXPR_TYPEINFO:
        return ast_expr_unary_type_to_str((ast_expr_typenameof_t*) expr, "typeinfo");
    case EXPR_TERNARY:
        return ast_expr_ternary_to_str((ast_expr_ternary_t*) expr);
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
    case EXPR_VA_ARG:
        return ast_expr_va_arg_to_str((ast_expr_va_arg_t*) expr);
    case EXPR_INITLIST:
        return ast_expr_initlist_to_str((ast_expr_initlist_t*) expr);
    case EXPR_POLYCOUNT:
        return mallocandsprintf("$#%s", ((ast_expr_polycount_t*) expr)->name);
    case EXPR_TYPENAMEOF:
        return ast_expr_unary_type_to_str((ast_expr_typenameof_t*) expr, "typenameof");
    case EXPR_LLVM_ASM:
        return strclone("llvm_asm <...> { ... }");
    case EXPR_EMBED:
        return ast_expr_embed_to_str((ast_expr_embed_t*) expr);
    case EXPR_ALIGNOF:
        return ast_expr_unary_type_to_str((ast_expr_alignof_t*) expr, "alignof");
    case EXPR_ILDECLARE: case EXPR_ILDECLAREUNDEF:
        return ast_expr_declare_to_str((ast_expr_declare_t*) expr);
    default:
        return strclone("<unknown expression>");
    }

    return NULL; // Should never be reached
}
