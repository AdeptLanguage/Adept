
#include "AST/UTIL/string_builder_extensions.h"

#include <stdlib.h>

#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "UTIL/ground.h"

void string_builder_append_type(string_builder_t *builder, ast_type_t *ast_type){
    strong_cstr_t type_string = ast_type_str(ast_type);
    string_builder_append(builder, type_string);
    free(type_string);
}

void string_builder_append_type_list(string_builder_t *builder, ast_type_t *ast_types, length_t count, bool include_parentheses){
    if(include_parentheses){
        string_builder_append_char(builder, '(');
    }

    for(length_t i = 0; i != count; i++){
        string_builder_append_type(builder, &ast_types[i]);
    }

    if(include_parentheses){
        string_builder_append_char(builder, ')');
    }
}

void string_builder_append_expr(string_builder_t *builder, ast_expr_t *ast_expr){
    strong_cstr_t expr_string = ast_expr_str(ast_expr);
    string_builder_append(builder, expr_string);
    free(expr_string);
}

void string_builder_append_expr_list(string_builder_t *builder, ast_expr_t **ast_exprs, length_t count, bool include_parentheses){
    if(include_parentheses){
        string_builder_append_char(builder, '(');
    }

    for(length_t i = 0; i != count; i++){
        string_builder_append_expr(builder, ast_exprs[i]);
    }

    if(include_parentheses){
        string_builder_append_char(builder, ')');
    }
}
