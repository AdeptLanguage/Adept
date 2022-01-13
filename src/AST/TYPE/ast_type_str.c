
#include <stdbool.h>
#include <stdlib.h>

#include "AST/UTIL/string_builder_extensions.h"
#include "AST/ast.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/ground.h"
#include "UTIL/string_builder.h"

static void ast_elem_base_str(string_builder_t *builder, ast_elem_base_t *elem){
    string_builder_append(builder, elem->base);
}

static void ast_elem_pointer_str(string_builder_t *builder){
    string_builder_append_char(builder, '*');
}

static void ast_elem_fixed_array_str(string_builder_t *builder, ast_elem_fixed_array_t *elem){
    string_builder_append_int(builder, (int) elem->length);
    string_builder_append_char(builder, ' ');
}

static void ast_elem_var_fixed_array_str(string_builder_t *builder, ast_elem_var_fixed_array_t *elem){
    string_builder_append_char(builder, '[');
    string_builder_append_expr(builder, elem->length);
    string_builder_append(builder, "] ");
}

static void ast_elem_generic_int_str(string_builder_t *builder){
    string_builder_append(builder, "long");
}

static void ast_elem_generic_float_str(string_builder_t *builder){
    string_builder_append(builder, "double");
}

static void ast_elem_func_str(string_builder_t *builder, ast_elem_func_t *elem){
    if(elem->traits & AST_FUNC_STDCALL){
        string_builder_append(builder, "stdcall ");
    }

    string_builder_append(builder, "func(");
    string_builder_append_type_list(builder, elem->arg_types, elem->arity, false);

    if(elem->traits & AST_FUNC_VARARG){
        string_builder_append(builder, ", ...");
    } else if(elem->traits & AST_FUNC_VARIADIC){
        string_builder_append(builder, ", ..");
    }

    string_builder_append(builder, ") ");
    string_builder_append_type(builder, elem->return_type);
}

static void ast_elem_polymorph_str(string_builder_t *builder, ast_elem_polymorph_t *elem){
    string_builder_append_char(builder, '$');

    if(elem->allow_auto_conversion){
        string_builder_append_char(builder, '~');
    }

    string_builder_append(builder, elem->name);
}

static void ast_elem_polymorph_prereq_str(string_builder_t *builder, ast_elem_polymorph_prereq_t *elem){
    string_builder_append_char(builder, '$');

    if(elem->allow_auto_conversion){
        string_builder_append_char(builder, '~');
    }

    string_builder_append(builder, elem->name);
    string_builder_append_char(builder, '~');
    string_builder_append(builder, elem->similarity_prerequisite);
}

static void ast_elem_polycount_str(string_builder_t *builder, ast_elem_polycount_t *elem){
    string_builder_append(builder, "$#");
    string_builder_append(builder, elem->name);
    string_builder_append_char(builder, ' ');
}

static void ast_elem_generic_base_str(string_builder_t *builder, ast_elem_generic_base_t *elem){
    string_builder_append_char(builder, '<');
    string_builder_append_type_list(builder, elem->generics, elem->generics_length, false);
    string_builder_append(builder, "> ");

    if(elem->name_is_polymorphic){
        string_builder_append_char(builder, '$');
    }

    string_builder_append(builder, elem->name);
}

static void ast_elem_layout_str(string_builder_t *builder, ast_elem_layout_t *elem){
    strong_cstr_t layout_str = ast_layout_str(&elem->layout, &elem->layout.field_map);
    string_builder_append(builder, layout_str);
    free(layout_str);
}

static void ast_elem_str(string_builder_t *builder, ast_elem_t *elem){
    switch(elem->id){
    case AST_ELEM_BASE:
        ast_elem_base_str(builder, (ast_elem_base_t*) elem);
        break;
    case AST_ELEM_POINTER:
        ast_elem_pointer_str(builder);
        break;
    case AST_ELEM_FIXED_ARRAY:
        ast_elem_fixed_array_str(builder, (ast_elem_fixed_array_t*) elem);
        break;
    case AST_ELEM_VAR_FIXED_ARRAY:
        ast_elem_var_fixed_array_str(builder, (ast_elem_var_fixed_array_t*) elem);
        break;
    case AST_ELEM_GENERIC_INT:
        ast_elem_generic_int_str(builder);
        break;
    case AST_ELEM_GENERIC_FLOAT:
        ast_elem_generic_float_str(builder);
        break;
    case AST_ELEM_FUNC:
        ast_elem_func_str(builder, (ast_elem_func_t*) elem);
        break;
    case AST_ELEM_POLYMORPH:
        ast_elem_polymorph_str(builder, (ast_elem_polymorph_t*) elem);
        break;
    case AST_ELEM_POLYMORPH_PREREQ:
        ast_elem_polymorph_prereq_str(builder, (ast_elem_polymorph_prereq_t*) elem);
        break;
    case AST_ELEM_POLYCOUNT:
        ast_elem_polycount_str(builder, (ast_elem_polycount_t*) elem);
        break;
    case AST_ELEM_GENERIC_BASE:
        ast_elem_generic_base_str(builder, (ast_elem_generic_base_t*) elem);
        break;
    case AST_ELEM_LAYOUT:
        ast_elem_layout_str(builder, (ast_elem_layout_t*) elem);
        break;
    default:
        die("ast_type_str() - Unrecognized type element 0x%08X\n", elem->id);
    }
}

strong_cstr_t ast_type_str(const ast_type_t *type){
    string_builder_t builder;
    string_builder_init(&builder);

    for(length_t i = 0; i != type->elements_length; i++){
        ast_elem_str(&builder, type->elements[i]);
    }

    return string_builder_finalize(&builder);
}
