
#include "UTIL/util.h"
#include "BRIDGE/any.h"

void any_inject_ast(ast_t *ast){
    any_inject_ast_Any(ast);
    any_inject_ast_AnyType(ast);
    any_inject_ast_AnyTypeKind(ast);

    any_inject_ast_AnyPtrType(ast);
    any_inject_ast_AnyStructType(ast);
    any_inject_ast_AnyFuncPtrType(ast);
    any_inject_ast_AnyFixedArrayType(ast);

    any_inject_ast___types__(ast);
    any_inject_ast___types_length__(ast);
}

void any_inject_ast_Any(ast_t *ast){

    /* struct Any (type *AnyType, placeholder ulong) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 2);
    names[0] = strclone("type");
    names[1] = strclone("placeholder");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 2);
    ast_type_make_base_ptr(&types[0], strclone("AnyType"));
    ast_type_make_base(&types[1], strclone("ulong"));

    ast_add_struct(ast, strclone("Any"), names, types, 2, TRAIT_NONE, source);
}

void any_inject_ast_AnyType(ast_t *ast){

    /* struct AnyType (kind AnyTypeKind, name *ubyte, is_alias bool) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 3);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 3);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));

    ast_add_struct(ast, strclone("AnyType"), names, types, 3, TRAIT_NONE, source);
}

void any_inject_ast_AnyTypeKind(ast_t *ast){

    /*
    enum AnyTypeKind (
        VOID, BOOL, BYTE, UBYTE, SHORT, USHORT, INT, UINT, LONG,
        ULONG, FLOAT, DOUBLE, PTR, STRUCT, FUNC_PTR, FIXED_ARRAY
    )
    */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    weak_cstr_t *kinds = malloc(sizeof(weak_cstr_t) * 16);
    kinds[0]  = "VOID";     kinds[1]  = "BOOL";
    kinds[2]  = "BYTE";     kinds[3]  = "UBYTE";
    kinds[4]  = "SHORT";    kinds[5]  = "USHORT";
    kinds[6]  = "INT";      kinds[7]  = "UINT";
    kinds[8]  = "LONG";     kinds[9]  = "ULONG";
    kinds[10] = "FLOAT";    kinds[11] = "DOUBLE";
    kinds[12] = "PTR";      kinds[13] = "STRUCT";
    kinds[14] = "FUNC_PTR"; kinds[15] = "FIXED_ARRAY";

    ast_add_enum(ast, "AnyTypeKind", kinds, 16, source);
}

void any_inject_ast_AnyPtrType(ast_t *ast){

    /* struct AnyPtrType(kind AnyTypeKind, name *ubyte, is_alias bool, subtype *AnyType) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 4);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("subtype");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 4);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base_ptr(&types[3], strclone("AnyType"));

    ast_add_struct(ast, strclone("AnyPtrType"), names, types, 4, TRAIT_NONE, source);
}

void any_inject_ast_AnyStructType(ast_t *ast){

    /* struct AnyStructType (kind AnyTypeKind, name *ubyte, is_alias bool, members **AnyType, length usize, offsets *usize, member_names **ubyte, is_packed bool) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 8);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("members");
    names[4] = strclone("length");
    names[5] = strclone("offsets");
    names[6] = strclone("member_names");
    names[7] = strclone("is_packed");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 8);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base_ptr_ptr(&types[3], strclone("AnyType"));
    ast_type_make_base(&types[4], strclone("usize"));
    ast_type_make_base_ptr(&types[5], strclone("usize"));
    ast_type_make_base_ptr_ptr(&types[6], strclone("ubyte"));
    ast_type_make_base(&types[7], strclone("bool"));

    ast_add_struct(ast, strclone("AnyStructType"), names, types, 8, TRAIT_NONE, source);
}

void any_inject_ast_AnyFuncPtrType(ast_t *ast){
    
    /* struct AnyFuncPtrType (kind AnyTypeKind, name *ubyte, is_alias bool, args **AnyType, length usize, return_type *AnyType, is_vararg bool, is_stdcall bool) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 8);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("args");
    names[4] = strclone("length");
    names[5] = strclone("return_type");
    names[6] = strclone("is_vararg");
    names[7] = strclone("is_stdcall");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 8);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base_ptr_ptr(&types[3], strclone("AnyType"));
    ast_type_make_base(&types[4], strclone("usize"));
    ast_type_make_base_ptr(&types[5], strclone("AnyType"));
    ast_type_make_base(&types[6], strclone("bool"));
    ast_type_make_base(&types[7], strclone("bool"));

    ast_add_struct(ast, strclone("AnyFuncPtrType"), names, types, 8, TRAIT_NONE, source);
}

void any_inject_ast_AnyFixedArrayType(ast_t *ast){
    
    /* struct AnyFixedArrayType (kind AnyTypeKind, is_alias bool, name *ubyte, subtype *AnyType, length usize) */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    strong_cstr_t *names = malloc(sizeof(strong_cstr_t) * 5);
    names[0] = strclone("kind");
    names[1] = strclone("name");
    names[2] = strclone("is_alias");
    names[3] = strclone("subtype");
    names[4] = strclone("length");

    ast_type_t *types = malloc(sizeof(ast_type_t) * 5);
    ast_type_make_base(&types[0], strclone("AnyTypeKind"));
    ast_type_make_base_ptr(&types[1], strclone("ubyte"));
    ast_type_make_base(&types[2], strclone("bool"));
    ast_type_make_base_ptr(&types[3], strclone("AnyType"));
    ast_type_make_base(&types[4], strclone("usize"));

    ast_add_struct(ast, strclone("AnyFixedArrayType"), names, types, 5, TRAIT_NONE, source);
}

void any_inject_ast___types__(ast_t *ast){

    /* __types__ **AnyType */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    ast_type_t type;
    ast_type_make_base_ptr_ptr(&type, strclone("AnyType"));

    ast_add_global(ast, "__types__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES__, source);
}

void any_inject_ast___types_length__(ast_t *ast){

    /* __types_length__ usize */

    source_t source;
    source.index = 0;
    source.object_index = 0;

    ast_type_t type;
    ast_type_make_base(&type, strclone("usize"));

    ast_add_global(ast, "__types_length__", type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES_LENGTH__, source);
}
