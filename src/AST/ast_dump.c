
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast_dump.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_type.h"
#include "UTIL/color.h"
#include "UTIL/string.h"
#include "UTIL/util.h"

void ast_dump(ast_t *ast, const char *filename){
    FILE *file = fopen(filename, "w");

    if(file == NULL){
        die("ast_dump() - Failed to open ast dump file\n");
    }

    ast_dump_enums(file, ast->enums, ast->enums_length);
    ast_dump_composites(file, ast->composites, ast->composites_length);
    ast_dump_globals(file, ast->globals, ast->globals_length);
    ast_dump_funcs(file, ast->funcs, ast->funcs_length);
    ast_dump_aliases(file, ast->aliases, ast->aliases_length);
    ast_dump_libraries(file, ast->libraries, ast->libraries_length);
    fclose(file);
}

static void ast_dump_stmt_return(FILE *file, ast_expr_return_t *stmt, length_t indentation){
    ast_expr_t *return_value = stmt->value;
    ast_expr_list_t last_minute = stmt->last_minute;

    if(last_minute.length != 0){
        fprintf(file, "<deferred> {\n");
        ast_dump_stmts(file, last_minute.statements, last_minute.length, indentation + 1);
        indent(file, indentation);
        fprintf(file, "}\n");
        indent(file, indentation);
    }

    if(return_value != NULL){
        strong_cstr_t s = ast_expr_str(return_value);
        fprintf(file, "return %s\n", s);
        free(s);
    } else {
        fprintf(file, "return\n");
    }
}

static void ast_dump_stmt_declare(FILE *file, ast_expr_declare_t *stmt){
    bool is_undef = stmt->id == EXPR_DECLAREUNDEF;

    if(stmt->traits & AST_EXPR_DECLARATION_CONST)  fprintf(file, "const ");
    if(stmt->traits & AST_EXPR_DECLARATION_STATIC) fprintf(file, "static ");

    fprintf(file, "%s ", stmt->name);

    if(stmt->traits & AST_EXPR_DECLARATION_POD) fprintf(file, "POD ");

    strong_cstr_t type_str = ast_type_str(&stmt->type);
    fprintf(file, "%s", type_str);
    free(type_str);

    if(is_undef){
        fprintf(file, " = undef");
    } else if(stmt->value){
        fprintf(file, " = ");

        if(stmt->traits & AST_EXPR_DECLARATION_ASSIGN_POD) fprintf(file, "POD ");

        strong_cstr_t s = ast_expr_str(stmt->value);
        fprintf(file, "%s", s);
        free(s);
    }

    fprintf(file, "\n");
}

static void ast_dump_stmt_assign_like(FILE *file, ast_expr_assign_t *stmt, const char *operator){
    strong_cstr_t destination_str = ast_expr_str(stmt->destination);
    strong_cstr_t value_str = ast_expr_str(stmt->value);
    fprintf(file, "%s %s %s\n", destination_str, operator, value_str);
    free(destination_str);
    free(value_str);
}

static void ast_dump_stmt_conditionless_block(FILE *file, ast_expr_conditionless_block_t *stmt, length_t indentation){
    fprintf(file, "{\n");
    ast_dump_stmts_list(file, &stmt->statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_simple_conditional(FILE *file, ast_expr_conditional_t *stmt, const char *keyword, length_t indentation){
    strong_cstr_t s = ast_expr_str(stmt->value);
    fprintf(file, "%s %s {\n", keyword, s);
    free(s);

    ast_dump_stmts_list(file, &stmt->statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_compound_conditional(FILE *file, ast_expr_conditional_else_t *stmt, const char *keyword, length_t indentation){
    strong_cstr_t s = ast_expr_str(stmt->value);
    fprintf(file, "%s %s {\n", keyword, s);
    free(s);

    ast_dump_stmts_list(file, &stmt->statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "} else {\n");
    ast_dump_stmts_list(file, &stmt->else_statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_repeat(FILE *file, ast_expr_repeat_t *stmt, length_t indentation){
    fprintf(file, "repeat ");

    if(stmt->is_static){
        fprintf(file, "static ");
    }

    strong_cstr_t s = ast_expr_str(stmt->limit);
    fprintf(file, "%s ", s);
    free(s);

    if(stmt->idx_overload_name){
        fprintf(file, "using %s ", stmt->idx_overload_name);
    }

    fprintf(file, "{\n");
    ast_dump_stmts_list(file, &stmt->statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_unary(FILE *file, ast_expr_unary_t *stmt, const char *keyword){
    strong_cstr_t value = ast_expr_str(stmt->value);
    fprintf(file, "%s %s\n", keyword, value);
    free(value);
}

static void ast_dump_stmt_each_in(FILE *file, ast_expr_each_in_t *stmt, length_t indentation){
    fprintf(file, "each ");

    if(stmt->it_name){
        fprintf(file, "%s ", stmt->it_name);
    }

    strong_cstr_t it_type_name = ast_type_str(stmt->it_type);
    fprintf(file, "%s", it_type_name);
    free(it_type_name);

    fprintf(file, " in ");

    if(stmt->is_static){
        fprintf(file, "static ");
    }
    
    if(stmt->list){
        strong_cstr_t value = ast_expr_str(stmt->list);
        fprintf(file, "%s {\n", value);
        free(value);
    } else {
        strong_cstr_t array = ast_expr_str(stmt->low_array);
        strong_cstr_t length = ast_expr_str(stmt->length);
        fprintf(file, "[%s, %s] {\n", array, length);
        free(array);
        free(length);
    }

    ast_dump_stmts_list(file, &stmt->statements, indentation + 1);
    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_switch(FILE *file, ast_expr_switch_t *stmt, length_t indentation){
    strong_cstr_t value = ast_expr_str(stmt->value);
    fprintf(file, "switch (%s) {\n", value);
    free(value);

    for(length_t i = 0; i != stmt->cases.length; i++){
        indent(file, indentation);

        ast_case_t *single_case = &stmt->cases.cases[i];

        strong_cstr_t value = ast_expr_str(single_case->condition);
        fprintf(file, "case (%s)\n", value);
        free(value);

        ast_dump_stmts_list(file, &single_case->statements, indentation + 1);
    }

    if(stmt->or_default.length != 0){
        indent(file, indentation);
        fprintf(file, "default\n");
        ast_dump_stmts_list(file, &stmt->or_default, indentation + 1);
    }

    indent(file, indentation);
    fprintf(file, "}\n");
}

static void ast_dump_stmt_va_copy(FILE *file, ast_expr_va_copy_t *stmt){
    strong_cstr_t dest = ast_expr_str(stmt->dest_value);
    strong_cstr_t src = ast_expr_str(stmt->src_value);
    fprintf(file, "va_copy(%s, %s)\n", dest, src);
    free(dest);
    free(src);
}

static void ast_dump_stmt_llvm_asm(FILE *file, ast_expr_llvm_asm_t *stmt){
    strong_cstr_t assembly = string_to_escaped_string(stmt->assembly, strlen(stmt->assembly), '\"', true);
    fprintf(file, "llvm_asm ... { \"%s\" } ... (...)\n", assembly);
    free(assembly);
}

void ast_dump_stmts(FILE *file, ast_expr_t **statements, length_t length, length_t indentation){
    // TODO: CLEANUP: Cleanup this messy code

    for(length_t s = 0; s != length; s++){
        ast_expr_t *stmt = statements[s];

        indent(file, indentation);

        // Print statement
        switch(stmt->id){
        case EXPR_RETURN:
            ast_dump_stmt_return(file, (ast_expr_return_t*) stmt, indentation);
            break;
        case EXPR_DECLARE:
        case EXPR_DECLAREUNDEF:
            ast_dump_stmt_declare(file, (ast_expr_declare_t*) stmt);
            break;
        case EXPR_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "=");
            break;
        case EXPR_ADD_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "+=");
            break;
        case EXPR_SUBTRACT_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "-=");
            break;
        case EXPR_MULTIPLY_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "*=");
            break;
        case EXPR_DIVIDE_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "/=");
            break;
        case EXPR_MODULUS_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "%=");
            break;
        case EXPR_AND_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "&=");
            break;
        case EXPR_OR_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "|=");
            break;
        case EXPR_XOR_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "^=");
            break;
        case EXPR_LSHIFT_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "<<=");
            break;
        case EXPR_RSHIFT_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, ">>=");
            break;
        case EXPR_LGC_LSHIFT_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, "<<<=");
            break;
        case EXPR_LGC_RSHIFT_ASSIGN:
            ast_dump_stmt_assign_like(file, (ast_expr_assign_t*) stmt, ">>>=");
            break;
        case EXPR_CONDITIONLESS_BLOCK:
            ast_dump_stmt_conditionless_block(file, (ast_expr_conditionless_block_t*) stmt, indentation);
            break;
        case EXPR_IF:
            ast_dump_stmt_simple_conditional(file, (ast_expr_conditional_t*) stmt, "if", indentation);
            break;
        case EXPR_UNLESS:
            ast_dump_stmt_simple_conditional(file, (ast_expr_conditional_t*) stmt, "unless", indentation);
            break;
        case EXPR_WHILE:
            ast_dump_stmt_simple_conditional(file, (ast_expr_conditional_t*) stmt, "while", indentation);
        case EXPR_UNTIL:
            ast_dump_stmt_simple_conditional(file, (ast_expr_conditional_t*) stmt, "until", indentation);
            break;
        case EXPR_IFELSE:
            ast_dump_stmt_compound_conditional(file, (ast_expr_conditional_else_t*) stmt, "if", indentation);
            break;
        case EXPR_UNLESSELSE:
            ast_dump_stmt_compound_conditional(file, (ast_expr_conditional_else_t*) stmt, "unless", indentation);
            break;
        case EXPR_REPEAT:
            ast_dump_stmt_repeat(file, (ast_expr_repeat_t*) stmt, indentation);
            break;
        case EXPR_EACH_IN:
            ast_dump_stmt_each_in(file, (ast_expr_each_in_t*) stmt, indentation);
            break;
        case EXPR_SWITCH:
            ast_dump_stmt_switch(file, (ast_expr_switch_t*) stmt, indentation);
            break;
        case EXPR_BREAK:
            fprintf(file, "break\n");
            break;
        case EXPR_CONTINUE:
            fprintf(file, "continue\n");
            break;
        case EXPR_BREAK_TO:
            fprintf(file, "break %s\n", ((ast_expr_break_to_t*) stmt)->label);
            break;
        case EXPR_CONTINUE_TO:
            fprintf(file, "continue %s\n", ((ast_expr_continue_to_t*) stmt)->label);
            break;
        case EXPR_DELETE:
            ast_dump_stmt_unary(file, (ast_expr_unary_t*) stmt, "delete");
            break;
        case EXPR_VA_START:
            ast_dump_stmt_unary(file, (ast_expr_unary_t*) stmt, "va_start");
            break;
        case EXPR_VA_END:
            ast_dump_stmt_unary(file, (ast_expr_unary_t*) stmt, "va_end");
            break;
        case EXPR_VA_COPY:
            ast_dump_stmt_va_copy(file, (ast_expr_va_copy_t*) stmt);
            break;
        case EXPR_LLVM_ASM:
            ast_dump_stmt_llvm_asm(file, (ast_expr_llvm_asm_t*) stmt);
            break;
        case EXPR_CALL:
        case EXPR_CALL_METHOD:
        case EXPR_PREINCREMENT:
        case EXPR_POSTINCREMENT:
        case EXPR_PREDECREMENT:
        case EXPR_POSTDECREMENT: {
                strong_cstr_t expr_str = ast_expr_str(statements[s]);
                fprintf(file, "%s\n", expr_str);
                free(expr_str);
            }
            break;
        default:
            fprintf(file, "<unknown statement>\n");
        }
    }
}

void ast_dump_stmts_list(FILE *file, ast_expr_list_t *statements, length_t indentation){
    ast_dump_stmts(file, statements->statements, statements->length, indentation);
}

static void ast_dump_composite_subfields(FILE *file, ast_layout_skeleton_t *skeleton, ast_field_map_t *field_map, ast_layout_endpoint_t parent_endpoint, length_t indentation){
    for(length_t i = 0; i != skeleton->bones_length; i++){
        if(i != 0){
            fprintf(file, ",\n");
        }

        ast_layout_bone_t *bone = &skeleton->bones[i];

        ast_layout_endpoint_t endpoint = parent_endpoint;
        ast_layout_endpoint_add_index(&endpoint, i);

        indent(file, indentation);
        if(bone->traits & AST_LAYOUT_BONE_PACKED) fprintf(file, "packed ");

        switch(bone->kind){
        case AST_LAYOUT_BONE_KIND_TYPE: {
                maybe_null_weak_cstr_t field_name = ast_field_map_get_name_of_endpoint(field_map, endpoint);
                assert(field_name != NULL);

                strong_cstr_t s = ast_type_str(&bone->type);
                fprintf(file, "%s %s", field_name, s);
                free(s);
            }
            break;
        case AST_LAYOUT_BONE_KIND_UNION:
            fprintf(file, "union (\n");
            ast_dump_composite_subfields(file, &bone->children, field_map, endpoint, indentation + 1);
            indent(file, indentation);
            fprintf(file, ")\n");
            break;
        case AST_LAYOUT_BONE_KIND_STRUCT:
            fprintf(file, "struct (\n");
            ast_dump_composite_subfields(file, &bone->children, field_map, endpoint, indentation + 1);
            indent(file, indentation);
            fprintf(file, ")\n");
            break;
        default:
            die("ast_dump_composite() - Unrecognized bone kind %d\n", (int) bone->kind);
        }
    }

    fprintf(file, "\n");
}

void ast_dump_composites(FILE *file, ast_composite_t *composites, length_t composites_length){
    for(length_t i = 0; i != composites_length; i++){
        ast_dump_composite(file, &composites[i], 0);
    }
}

void ast_dump_composite(FILE *file, ast_composite_t *composite, length_t additional_indentation){
    ast_layout_t *layout = &composite->layout;

    indent(file, additional_indentation);

    if(layout->traits & AST_LAYOUT_PACKED){
        fprintf(file, "packed ");
    }

    switch(layout->kind){
    case AST_LAYOUT_STRUCT:
        fprintf(file, "struct ");
        break;
    case AST_LAYOUT_UNION:
        fprintf(file, "union ");
        break;
    default:
        die("ast_dump_composite() - Unrecognized layout kind %d\n", (int) layout->kind);
    }

    // Dump generics "<$K, $V>" if the composite is polymorphic
    if(composite->is_polymorphic){
        ast_poly_composite_t *poly_composite = (ast_poly_composite_t*) composite;

        fprintf(file, "<");

        for(length_t i = 0; i != poly_composite->generics_length; i++){
            fprintf(file, "$%s", poly_composite->generics[i]);

            if(i + 1 != poly_composite->generics_length){
                fprintf(file, ", ");
            }
        }

        fprintf(file, "> ");
    }

    fprintf(file, "%s (\n", composite->name);

    ast_field_map_t *field_map = &layout->field_map;
    ast_layout_skeleton_t *skeleton = &layout->skeleton;

    ast_layout_endpoint_t child_endpoint;
    ast_layout_endpoint_init(&child_endpoint);
    
    ast_dump_composite_subfields(file, skeleton, field_map, child_endpoint, additional_indentation + 1);
    indent(file, additional_indentation);
    fprintf(file, ")\n");
}

void ast_dump_globals(FILE *file, ast_global_t *globals, length_t globals_length){
    for(length_t i = 0; i != globals_length; i++){
        ast_global_t *global = &globals[i];
        strong_cstr_t type = ast_type_str(&global->type);

        if(global->initial == NULL){
            fprintf(file, "%s %s\n", global->name, type);
        } else {
            strong_cstr_t value = ast_expr_str(global->initial);
            fprintf(file, "%s %s = %s\n", global->name, type, value);
            free(value);
        }

        free(type);
    }
}

void ast_dump_enums(FILE *file, ast_enum_t *enums, length_t enums_length){
    for(length_t i = 0; i != enums_length; i++){
        ast_enum_t *enumeration = &enums[i];

        fprintf(file, "enum %s (", enumeration->name);

        for(length_t i = 0; i != enumeration->length; i++){
            fprintf(file, "%s", enumeration->kinds[i]);

            if(i + 1 != enumeration->length){
                fprintf(file, ", ");
            }
        }

        fprintf(file, ")\n");
    }
}

void ast_dump_funcs(FILE *file, ast_func_t *functions, length_t functions_length){
    for(length_t i = 0; i != functions_length; i++){
        ast_func_t *func = &functions[i];
        
        strong_cstr_t args = ast_func_args_str(func);
        strong_cstr_t return_type = ast_type_str(&func->return_type);

        if(func->traits & AST_FUNC_DISPATCHER) fprintf(file, "dispatcher ");
        if(func->traits & AST_FUNC_VIRTUAL)    fprintf(file, "virtual ");
        if(func->traits & AST_FUNC_OVERRIDE)   fprintf(file, "override ");

        if(func->traits & AST_FUNC_FOREIGN){
            fprintf(file, "foreign %s(%s) %s\n", func->name, args, return_type);
        } else {
            fprintf(file, "func %s(%s) %s {\n", func->name, args, return_type);
            ast_dump_stmts_list(file, &func->statements, 1);
            fprintf(file, "}\n");
        }

        free(args);
        free(return_type);
    }
}

void ast_dump_aliases(FILE *file, ast_alias_t *aliases, length_t length){
    for(length_t i = 0; i != length; i++){
        ast_alias_t *alias = &aliases[i];

        strong_cstr_t type = ast_type_str(&alias->type);
        fprintf(file, "alias %s = %s\n", alias->name, type);
        free(type);
    }
}

void ast_dump_libraries(FILE *file, strong_cstr_t *libraries, length_t length){
    for(length_t i = 0; i != length; i++){
        fprintf(file, "foreign '%s'\n", libraries[i]);
    }
}
