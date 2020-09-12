
#include "UTIL/util.h"
#include "BRIDGE/any.h"

const char *any_type_kind_names[] = {
    "void", "bool", "byte", "ubyte", "short", "ushort", "int", "uint",
    "long", "ulong", "float", "double", "pointer", "struct", "union", "function-pointer", "fixed-array"
};

void any_inject_ast(ast_t *ast){
    any_inject_ast_Any(ast);
    any_inject_ast_AnyType(ast);
    any_inject_ast_AnyTypeKind(ast);

    any_inject_ast_AnyPtrType(ast);
    any_inject_ast_AnyStructType(ast);
    any_inject_ast_AnyUnionType(ast);
    any_inject_ast_AnyFuncPtrType(ast);
    any_inject_ast_AnyFixedArrayType(ast);

    any_inject_ast___types__(ast);
    any_inject_ast___types_length__(ast);
    any_inject_ast___type_kinds__(ast);
    any_inject_ast___type_kinds_length__(ast);
}

void any_inject_ast_Any(ast_t *ast){

    /* struct Any (type *AnyType, placeholder ulong) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 2);
    names[0] = strclone("type");
    names[1] = strclone("placeholder");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 2);
    ast_type_make_base_ptr(&types[0], strclone("AnyType"));
    ast_type_make_base(&types[1], strclone("ulong"));

    ast_add_struct(ast, strclone("Any"), names, types, 2, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyType(ast_t *ast){

    /* struct AnyType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 4);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 4);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));

    ast_add_struct(ast, strclone("AnyType"), names, types, 4, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyTypeKind(ast_t *ast){

    /*
    enum AnyTypeKind (
        VOID, BOOL, BYTE, UBYTE, SHORT, USHORT, INT, UINT, LONG,
        ULONG, FLOAT, DOUBLE, PTR, STRUCT, FUNC_PTR, FIXED_ARRAY
    )
    */

    weak_cstr_t *kinds = malloc(sizeof(weak_cstr_t) * 16);
    kinds[0]  = "VOID";     kinds[1]  = "BOOL";
    kinds[2]  = "BYTE";     kinds[3]  = "UBYTE";
    kinds[4]  = "SHORT";    kinds[5]  = "USHORT";
    kinds[6]  = "INT";      kinds[7]  = "UINT";
    kinds[8]  = "LONG";     kinds[9]  = "ULONG";
    kinds[10] = "FLOAT";    kinds[11] = "DOUBLE";
    kinds[12] = "PTR";      kinds[13] = "STRUCT";
    kinds[14] = "FUNC_PTR"; kinds[15] = "FIXED_ARRAY";

    ast_add_enum(ast, strclone("AnyTypeKind"), kinds, 16, NULL_SOURCE);
}

void any_inject_ast_AnyPtrType(ast_t *ast){

    /* struct AnyPtrType(kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 5);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");
    names[4] = strclone("subtype");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 5);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));
    ast_type_make_base_ptr(&types[4], strclone("AnyType"));

    ast_add_struct(ast, strclone("AnyPtrType"), names, types, 5, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyStructType(ast_t *ast){

    /* struct AnyStructType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, members **AnyType, length usize, offsets *usize, member_names **ubyte, is_packed bool) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 9);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");
    names[4] = strclone("members");
    names[5] = strclone("length");
    names[6] = strclone("offsets");
    names[7] = strclone("member_names");
    names[8] = strclone("is_packed");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 9);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[4], strclone("AnyType"));
    ast_type_make_base(&types[5], strclone("usize"));
    ast_type_make_base_ptr(&types[6], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[7], strclone("ubyte"));
    ast_type_make_base(&types[8], strclone("bool"));

    ast_add_struct(ast, strclone("AnyStructType"), names, types, 9, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyUnionType(ast_t *ast){

    /* struct AnyUnionType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, members **AnyType, length usize, member_names **ubyte) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 7);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");
    names[4] = strclone("members");
    names[5] = strclone("length");
    names[6] = strclone("member_names");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 7);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[4], strclone("AnyType"));
    ast_type_make_base(&types[5], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[6], strclone("ubyte"));

    ast_add_struct(ast, strclone("AnyUnionType"), names, types, 7, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyFuncPtrType(ast_t *ast){
    
    /* struct AnyFuncPtrType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, args **AnyType, length usize, return_type *AnyType, is_vararg bool, is_stdcall bool) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 9);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");
    names[4] = strclone("args");
    names[5] = strclone("length");
    names[6] = strclone("return_type");
    names[7] = strclone("is_vararg");
    names[8] = strclone("is_stdcall");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 9);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[4], strclone("AnyType"));
    ast_type_make_base(&types[5], strclone("usize"));
    ast_type_make_base_ptr(&types[6], strclone("AnyType"));
    ast_type_make_base(&types[7], strclone("bool"));
    ast_type_make_base(&types[8], strclone("bool"));

    ast_add_struct(ast, strclone("AnyFuncPtrType"), names, types, 9, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyFixedArrayType(ast_t *ast){
    
    /* struct AnyFixedArrayType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType, length usize) */

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 6);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("size");
    names[4] = strclone("subtype");
    names[5] = strclone("length");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 6);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base(&types[3], strclone("usize"));
    ast_type_make_base_ptr(&types[4], strclone("AnyType"));
    ast_type_make_base(&types[5], strclone("usize"));

    ast_add_struct(ast, strclone("AnyFixedArrayType"), names, types, 6, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast___types__(ast_t *ast){

    /* __types__ **AnyType */

    ast_type_t type;
    ast_type_make_base_ptr_ptr(&type, strclone("AnyType"));

    ast_add_global(ast, "__types__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES__, NULL_SOURCE);
}

void any_inject_ast___types_length__(ast_t *ast){

    /* __types_length__ usize */

    ast_type_t type;
    ast_type_make_base(&type, strclone("usize"));

    ast_add_global(ast, "__types_length__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES_LENGTH__, NULL_SOURCE);
}

void any_inject_ast___type_kinds__(ast_t *ast){

    /* __type_kinds__ **ubyte */

    ast_type_t type;
    ast_type_make_base_ptr_ptr(&type, strclone("ubyte"));

    ast_add_global(ast, "__type_kinds__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPE_KINDS__, NULL_SOURCE);
}

void any_inject_ast___type_kinds_length__(ast_t *ast){

    /* __type_kinds_length__ usize */

    ast_type_t type;
    ast_type_make_base(&type, strclone("usize"));

    ast_add_global(ast, "__type_kinds_length__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPE_KINDS_LENGTH__, NULL_SOURCE);

}
