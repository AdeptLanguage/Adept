
#include <stdlib.h>

#include "AST/ast.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "AST/TYPE/ast_type_make.h"
#include "BRIDGE/any.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"

const char *any_type_kind_names[] = {
    "void", "bool", "byte", "ubyte", "short", "ushort", "int", "uint",
    "long", "ulong", "float", "double", "pointer", "struct", "union", "function-pointer", "fixed-array"
};

void any_inject_ast(ast_t *ast){
    any_inject_ast_Any(ast);
    any_inject_ast_AnyType(ast);
    any_inject_ast_AnyTypeKind(ast);

    any_inject_ast_AnyPtrType(ast);
    any_inject_ast_AnyUnionType(ast);
    any_inject_ast_AnyStructType(ast);
    any_inject_ast_AnyFuncPtrType(ast);
    any_inject_ast_AnyCompositeType(ast);
    any_inject_ast_AnyFixedArrayType(ast);

    any_inject_ast___types__(ast);
    any_inject_ast___types_length__(ast);
    any_inject_ast___type_kinds__(ast);
    any_inject_ast___type_kinds_length__(ast);
}

void any_inject_ast_Any(ast_t *ast){

    /* struct Any (type *AnyType, placeholder ulong) */

    strong_cstr_t names[2] = {
        strclone("type"),
        strclone("placeholder"),
    };

    ast_type_t types[2] = {
        ast_type_make_base_ptr(strclone("AnyType")),
        ast_type_make_base(strclone("ulong")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 2);
    ast_add_composite(ast, strclone("Any"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast_AnyType(ast_t *ast){

    /* struct AnyType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize) */

    strong_cstr_t names[4] = {
        strclone("kind"),
        strclone("name"),
        strclone("is_alias"),
        strclone("size"),
    };

    ast_type_t types[4] = {
        ast_type_make_base(strclone("AnyTypeKind")),
        ast_type_make_base_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("usize")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 4);
    ast_add_composite(ast, strclone("AnyType"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast_AnyTypeKind(ast_t *ast){

    /*
    enum AnyTypeKind (
        VOID, BOOL, BYTE, UBYTE, SHORT, USHORT, INT, UINT, LONG,
        ULONG, FLOAT, DOUBLE, PTR, STRUCT, UNION, FUNC_PTR, FIXED_ARRAY
    )
    */

    weak_cstr_t *kinds = malloc(sizeof(weak_cstr_t) * 17);
    kinds[0]  = "VOID";     kinds[1]  = "BOOL";
    kinds[2]  = "BYTE";     kinds[3]  = "UBYTE";
    kinds[4]  = "SHORT";    kinds[5]  = "USHORT";
    kinds[6]  = "INT";      kinds[7]  = "UINT";
    kinds[8]  = "LONG";     kinds[9]  = "ULONG";
    kinds[10] = "FLOAT";    kinds[11] = "DOUBLE";
    kinds[12] = "PTR";      kinds[13] = "STRUCT";
    kinds[14] = "UNION";    kinds[15] = "FUNC_PTR";
    kinds[16] = "FIXED_ARRAY";

    ast_add_enum(ast, strclone("AnyTypeKind"), kinds, 17, NULL_SOURCE);
}

void any_inject_ast_AnyPtrType(ast_t *ast){

    /* struct AnyPtrType(kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType) */

    strong_cstr_t names[5] = {
        strclone("kind"),
        strclone("name"),
        strclone("is_alias"),
        strclone("size"),
        strclone("subtype"),
    };

    ast_type_t types[5] = {
        ast_type_make_base(strclone("AnyTypeKind")),
        ast_type_make_base_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr(strclone("AnyType")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 5);
    ast_add_composite(ast, strclone("AnyPtrType"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast_AnyCompositeType(ast_t *ast){

    /* struct AnyCompositeType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, members **AnyType, length usize, offsets *usize, member_names **ubyte, is_packed bool) */

    strong_cstr_t names[9] = {
        strclone("kind"),
        strclone("name"),
        strclone("is_alias"),
        strclone("size"),
        strclone("members"),
        strclone("length"),
        strclone("offsets"),
        strclone("member_names"),
        strclone("is_packed"),
    };

    ast_type_t types[9] = {
        ast_type_make_base(strclone("AnyTypeKind")),
        ast_type_make_base_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr_ptr(strclone("AnyType")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr(strclone("usize")),
        ast_type_make_base_ptr_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 9);
    ast_add_composite(ast, strclone("AnyCompositeType"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast_AnyStructType(ast_t *ast){
    // For backwards-compatibility, redirect AnyStructType to be the new AnyCompositeType
    // The 'kind' will still be ANY_KIND_STRUCT to signify that it is a struct (or now maybe complex struct)

    // alias AnyStructType = AnyCompositeType

    ast_type_t strong_type = ast_type_make_base(strclone("AnyCompositeType"));

    ast_add_alias(ast, strclone("AnyStructType"), strong_type, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyUnionType(ast_t *ast){
    // For backwards-compatibility, redirect AnyUnionType to be the new AnyCompositeType
    // The 'kind' will still be ANY_KIND_UNION to signify that it is a union (or now maybe complex union)

    // alias AnyUnionType = AnyCompositeType

    ast_type_t strong_type = ast_type_make_base(strclone("AnyCompositeType"));

    ast_add_alias(ast, strclone("AnyUnionType"), strong_type, TRAIT_NONE, NULL_SOURCE);
}

void any_inject_ast_AnyFuncPtrType(ast_t *ast){
    
    /* struct AnyFuncPtrType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, args **AnyType, length usize, return_type *AnyType, is_vararg bool, is_stdcall bool) */

    strong_cstr_t names[9] = {
        strclone("kind"),
        strclone("name"),
        strclone("is_alias"),
        strclone("size"),
        strclone("args"),
        strclone("length"),
        strclone("return_type"),
        strclone("is_vararg"),
        strclone("is_stdcall"),
    };

    ast_type_t types[9] = {
        ast_type_make_base(strclone("AnyTypeKind")),
        ast_type_make_base_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr_ptr(strclone("AnyType")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr(strclone("AnyType")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("bool")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 9);
    ast_add_composite(ast, strclone("AnyFuncPtrType"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast_AnyFixedArrayType(ast_t *ast){
    
    /* struct AnyFixedArrayType (kind AnyTypeKind, name *ubyte, is_alias bool, size usize, subtype *AnyType, length usize) */

    strong_cstr_t names[6] = {
        strclone("kind"),
        strclone("name"),
        strclone("is_alias"),
        strclone("size"),
        strclone("subtype"),
        strclone("length"),
    };

    ast_type_t types[6] = {
        ast_type_make_base(strclone("AnyTypeKind")),
        ast_type_make_base_ptr(strclone("ubyte")),
        ast_type_make_base(strclone("bool")),
        ast_type_make_base(strclone("usize")),
        ast_type_make_base_ptr(strclone("AnyType")),
        ast_type_make_base(strclone("usize")),
    };

    ast_layout_t layout;
    ast_layout_init_with_struct_fields(&layout, names, types, 6);
    ast_add_composite(ast, strclone("AnyFixedArrayType"), layout, NULL_SOURCE, NULL, false);
}

void any_inject_ast___types__(ast_t *ast){

    /* __types__ **AnyType */

    ast_type_t type = ast_type_make_base_ptr_ptr(strclone("AnyType"));

    ast_add_global(ast, strclone("__types__"), type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES__, NULL_SOURCE);
}

void any_inject_ast___types_length__(ast_t *ast){

    /* __types_length__ usize */

    ast_type_t type = ast_type_make_base(strclone("usize"));

    ast_add_global(ast, strclone("__types_length__"), type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPES_LENGTH__, NULL_SOURCE);
}

void any_inject_ast___type_kinds__(ast_t *ast){

    /* __type_kinds__ **ubyte */

    ast_type_t type = ast_type_make_base_ptr_ptr(strclone("ubyte"));

    ast_add_global(ast, strclone("__type_kinds__"), type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPE_KINDS__, NULL_SOURCE);
}

void any_inject_ast___type_kinds_length__(ast_t *ast){

    /* __type_kinds_length__ usize */

    ast_type_t type = ast_type_make_base(strclone("usize"));

    ast_add_global(ast, strclone("__type_kinds_length__"), type, NULL, AST_GLOBAL_SPECIAL | AST_GLOBAL___TYPE_KINDS_LENGTH__, NULL_SOURCE);
}
