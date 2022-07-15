
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IR/ir.h"
#include "IR/ir_type.h"
#include "IR/ir_value.h"
#include "IR/ir_value_str.h"
#include "UTIL/color.h"
#include "UTIL/datatypes.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"

static strong_cstr_t literal_to_str(ir_value_t *value, weak_cstr_t typename){
    strong_cstr_t value_part;

    switch(value->type->kind){
    case TYPE_KIND_S8:
        value_part = int8_to_string(*typecast(adept_byte*, value->extra), "");
        break;
    case TYPE_KIND_U8:
        value_part = uint8_to_string(*typecast(adept_ubyte*, value->extra), "");
        break;
    case TYPE_KIND_S16:
        value_part = int16_to_string(*typecast(adept_short*, value->extra), "");
        break;
    case TYPE_KIND_U16:
        value_part = uint16_to_string(*typecast(adept_ushort*, value->extra), "");
        break;
    case TYPE_KIND_S32:
        value_part = int32_to_string(*typecast(adept_int*, value->extra), "");
        break;
    case TYPE_KIND_U32:
        value_part = uint32_to_string(*typecast(adept_uint*, value->extra), "");
        break;
    case TYPE_KIND_S64:
        value_part = int64_to_string(*typecast(adept_long*, value->extra), "");
        break;
    case TYPE_KIND_U64:
        value_part = uint64_to_string(*typecast(adept_ulong*, value->extra), "");
        break;
    case TYPE_KIND_FLOAT:
        value_part = float32_to_string(*typecast(adept_float*, value->extra), "");
        break;
    case TYPE_KIND_DOUBLE:
        value_part = float64_to_string(*typecast(adept_double*, value->extra), "");
        break;
    case TYPE_KIND_BOOLEAN:
        value_part = strclone(bool_to_string(*typecast(adept_bool*, value->extra)));
        break;
    case TYPE_KIND_POINTER:
        // '*ubyte' null-terminated string literal
        if(typecast(ir_type_t*, value->type->extra)->kind == TYPE_KIND_U8){
            char *string = (char*) value->extra;
            value_part = string_to_escaped_string(string, strlen(string), '\'');
        } else {
            die("ir_value_str() - Unrecognized literal with pointer type kind\n");
        }
        break;
    default:
        die("ir_value_str() - Unrecognized type kind %d\n", (int) value->value_type);
    }

    strong_cstr_t result = mallocandsprintf("%s %s", typename, value_part);
    free(value_part);
    return result;
}

static strong_cstr_t value_result_to_str(ir_value_t *value, weak_cstr_t typename){
    ir_value_result_t *reference = (ir_value_result_t*) value->extra;

    size_t max_size = strlen(typename) + 28;
    strong_cstr_t result = malloc(max_size);

    // ">|-0000000000| 0x00000000<"
    snprintf(result, max_size, "%s >|%d| 0x%08X<", typename, (int) reference->block_id, (int) reference->instruction_id);

    return result;
}

static strong_cstr_t literal_cstr_of_len_to_str(ir_value_t *value){
    ir_value_cstr_of_len_t *cstroflen = value->extra;

    strong_cstr_t string = string_to_escaped_string(cstroflen->array, cstroflen->size, '\"');
    strong_cstr_t result = mallocandsprintf("cstroflen %d %s", (int) cstroflen->size, string);
    free(string);

    return result;
}

static strong_cstr_t literal_func_addr_to_str(ir_value_t *value){
    ir_value_func_addr_t *func_addr = value->extra;

    char buffer[32];
    ir_implementation(func_addr->ir_func_id, 0x00, buffer);

    return mallocandsprintf("funcaddr 0x%s", buffer);
}

static strong_cstr_t literal_func_addr_by_name_to_str(ir_value_t *value){
    ir_value_func_addr_by_name_t *func_addr_by_name = value->extra;
    return mallocandsprintf("funcaddr %s", func_addr_by_name->name);
}

static strong_cstr_t literal_const_sizeof_to_str(ir_value_t *value){
    ir_type_t *type = ((ir_value_const_sizeof_t*) value->extra)->type;

    strong_cstr_t type_str = ir_type_str(type);
    strong_cstr_t result = mallocandsprintf("csizeof %s", type_str);
    free(type_str);

    return result;
}

static strong_cstr_t literal_const_add_to_str(ir_value_t *value){
    ir_value_const_math_t *const_add = value->extra;
    strong_cstr_t a_str = ir_value_str(const_add->a);
    strong_cstr_t b_str = ir_value_str(const_add->b);

    strong_cstr_t result = mallocandsprintf("cadd %s, %s", a_str, b_str);

    free(a_str);
    free(b_str);
    return result;
}

static strong_cstr_t const_cast_to_str(ir_value_t *value, const char *cast_kind){
    strong_cstr_t from = ir_value_str(value->extra);
    strong_cstr_t to = ir_type_str(value->type);

    strong_cstr_t result = mallocandsprintf("%s %s to %s", cast_kind, from, to);
    
    free(to);
    free(from);
    return result;
}

static strong_cstr_t const_struct_construction_to_str(ir_value_t *value){
    ir_value_struct_construction_t *construction = value->extra;
    strong_cstr_t of = ir_type_str(value->type);

    strong_cstr_t result = mallocandsprintf("const-construct %s (from %d values)", of, (int) construction->length);

    free(of);
    return result;
}

strong_cstr_t ir_value_str(ir_value_t *value){
    strong_cstr_t result;
    strong_cstr_t typename = ir_type_str(value->type);

    switch(value->value_type){
    case VALUE_TYPE_LITERAL:
        result = literal_to_str(value, typename);
        break;
    case VALUE_TYPE_RESULT:
        result = value_result_to_str(value, typename);
        break;
    case VALUE_TYPE_NULLPTR:
    case VALUE_TYPE_NULLPTR_OF_TYPE:
        result = strclone("null");
        break;
    case VALUE_TYPE_ARRAY_LITERAL:
        result = strclone("array-literal");
        break;
    case VALUE_TYPE_STRUCT_LITERAL:
        result = strclone("struct-literal");
        break;
    case VALUE_TYPE_ANON_GLOBAL:
        result = mallocandsprintf("anonglob %d", (int) typecast(ir_value_anon_global_t*, value->extra)->anon_global_id);
        break;
    case VALUE_TYPE_CONST_ANON_GLOBAL:
        result = mallocandsprintf("constanonglob %d", (int) typecast(ir_value_anon_global_t*, value->extra)->anon_global_id);
        break;
    case VALUE_TYPE_CSTR_OF_LEN:
        result = literal_cstr_of_len_to_str(value);
        break;
    case VALUE_TYPE_FUNC_ADDR:
        result = literal_func_addr_to_str(value);
        break;
    case VALUE_TYPE_FUNC_ADDR_BY_NAME:
        result = literal_func_addr_by_name_to_str(value);
        break;
    case VALUE_TYPE_CONST_SIZEOF:
        result = literal_const_sizeof_to_str(value);
        break;
    case VALUE_TYPE_CONST_ADD:
        result = literal_const_add_to_str(value);
        break;
    case VALUE_TYPE_CONST_BITCAST:
        result = const_cast_to_str(value, "cbc");
        break;
    case VALUE_TYPE_CONST_ZEXT:
        result = const_cast_to_str(value, "czext");
        break;
    case VALUE_TYPE_CONST_SEXT:
        result = const_cast_to_str(value, "csext");
        break;
    case VALUE_TYPE_CONST_FEXT:
        result = const_cast_to_str(value, "cfext");
        break;
    case VALUE_TYPE_CONST_TRUNC:
        result = const_cast_to_str(value, "ctrunc");
        break;
    case VALUE_TYPE_CONST_FTRUNC:
        result = const_cast_to_str(value, "cftrunc");
        break;
    case VALUE_TYPE_CONST_INTTOPTR:
        result = const_cast_to_str(value, "ci2p");
        break;
    case VALUE_TYPE_CONST_PTRTOINT:
        result = const_cast_to_str(value, "cp2i");
        break;
    case VALUE_TYPE_CONST_FPTOUI:
        result = const_cast_to_str(value, "cfp2ui");
        break;
    case VALUE_TYPE_CONST_FPTOSI:
        result = const_cast_to_str(value, "cfp2si");
        break;
    case VALUE_TYPE_CONST_UITOFP:
        result = const_cast_to_str(value, "cui2fp");
        break;
    case VALUE_TYPE_CONST_SITOFP:
        result = const_cast_to_str(value, "csi2fp");
        break;
    case VALUE_TYPE_CONST_REINTERPRET:
        result = const_cast_to_str(value, "creinterp");
        break;
    case VALUE_TYPE_STRUCT_CONSTRUCTION:
        result = const_struct_construction_to_str(value);
        break;
    default:
        die("ir_value_str() - Unrecognized value type\n");
    }

    free(typename);
    return result;
}

strong_cstr_t ir_values_str(ir_value_t **values, length_t length){
    string_builder_t builder;
    string_builder_init(&builder);

    for(length_t i = 0; i != length; i++){
        strong_cstr_t value_str = ir_value_str(values[i]);
        string_builder_append(&builder, value_str);
        free(value_str);

        if(i + 1 != length){
            string_builder_append(&builder, ", ");
        }
    }

    return strong_cstr_empty_if_null(string_builder_finalize(&builder));
}
