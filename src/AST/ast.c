
#include "AST/ast.h"
#include "AST/ast_type.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "DRVR/compiler.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

void ast_init(ast_t *ast, unsigned int cross_compile_for){
    ast->funcs = malloc(sizeof(ast_func_t) * 8);
    ast->funcs_length = 0;
    ast->funcs_capacity = 8;
    ast->func_aliases = NULL;
    ast->func_aliases_length = 0;
    ast->func_aliases_capacity = 0;
    ast->composites = malloc(sizeof(ast_composite_t) * 4);
    ast->composites_length = 0;
    ast->composites_capacity = 4;
    ast->aliases = malloc(sizeof(ast_alias_t) * 8);
    ast->aliases_length = 0;
    ast->aliases_capacity = 8;
    ast->constants = NULL;
    ast->constants_length = 0;
    ast->constants_capacity = 0;
    ast->globals = NULL;
    ast->globals_length = 0;
    ast->globals_capacity = 0;
    ast->enums = NULL;
    ast->enums_length = 0;
    ast->enums_capacity = 0;
    ast->libraries = NULL;
    ast->library_kinds = NULL;
    ast->libraries_length = 0;
    ast->libraries_capacity = 0;
    ast_type_make_base(&ast->common.ast_int_type, strclone("int"));
    ast_type_make_base(&ast->common.ast_usize_type, strclone("usize"));
    ast->common.ast_variadic_array = NULL;

    ast->type_table = NULL;
    ast->meta_definitions = NULL;
    ast->meta_definitions_length = 0;
    ast->meta_definitions_capacity = 0;
    ast->polymorphic_funcs = NULL;
    ast->polymorphic_funcs_length = 0;
    ast->polymorphic_funcs_capacity = 0;
    ast->polymorphic_methods = NULL;
    ast->polymorphic_methods_length = 0;
    ast->polymorphic_methods_capacity = 0;
    ast->polymorphic_composites = NULL;
    ast->polymorphic_composites_length = 0;
    ast->polymorphic_composites_capacity = 0;

    // Add relevant standard meta definitions

    // __compiler__
    // NOTE: We don't have to worry about freeing what we get from 'compiler_get_string()'
    // because meta_definition_add_str takes ownership of strong_cstr_t
    meta_definition_add_str(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler__", compiler_get_string());

    // __compiler_version__, __compiler_version_name__
    meta_definition_add_float(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_version__", ADEPT_VERSION_NUMBER);
    meta_definition_add_str(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_version_name__", strclone(ADEPT_VERSION_STRING));

    // __compiler_major__, __compiler_minor__, __compiler_release__, __compiler_qualifier__
    meta_definition_add_int(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_major__", ADEPT_VERSION_MAJOR);
    meta_definition_add_int(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_minor__", ADEPT_VERSION_MINOR);
    meta_definition_add_int(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_release__", ADEPT_VERSION_RELEASE);
    meta_definition_add_str(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__compiler_qualifier__", strclone(ADEPT_VERSION_QUALIFIER));

    // __windows__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__windows__",
    #ifdef _WIN32
        cross_compile_for == CROSS_COMPILE_NONE
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({
            return process.platform == 'win32' ? 1 : 0
        }) == 1
    #else
        cross_compile_for == CROSS_COMPILE_WINDOWS
    #endif
    );

    // __macos__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__macos__",
    #if defined(__APPLE__) || defined(__MACH__)
        cross_compile_for == CROSS_COMPILE_NONE
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({
            return process.platform == 'darwin' ? 1 : 0
        }) == 1
    #else
        cross_compile_for == CROSS_COMPILE_MACOS
    #endif
    );

    // __arm64__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__arm64__",
    #if defined(__arm64__) || defined(__aarch64__)
        true
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({return process.arch == 'arm64' ? 1 : 0}) == 1
    #else
        false
    #endif
    );

    // __x86_64__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__x86_64__",
    #if defined(__x86_64__) || defined(_M_AMD64)
        true
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({return process.arch == 'x64' ? 1 : 0}) == 1
    #else
        false
    #endif
    );

    // __unix__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__unix__",
    #if defined(__unix__) || defined(__unix) || defined(unix)
        cross_compile_for == CROSS_COMPILE_NONE
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({
            return process.platform == 'darwin' || process.platform == 'linux' ? 1 : 0
        }) == 1
    #else
        false
    #endif
    );

    // __linux__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__linux__",
    #if defined(__linux__) || defined(__linux) || defined(linux)
        cross_compile_for == CROSS_COMPILE_NONE
    #elif defined(__EMSCRIPTEN__)
        EM_ASM_INT({
            return process.platform == 'linux' ? 1 : 0
        }) == 1
    #else
        false
    #endif
    );

    // __wasm__
    meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__wasm__", cross_compile_for == CROSS_COMPILE_WASM32);

    unsigned short x = 0xEEFF;
    if (*((unsigned char*) &x) == 0xFF || cross_compile_for == CROSS_COMPILE_WINDOWS){
        // Little Endian
        meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__little_endian__", true);
        meta_definition_add_str(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__endianness__", strclone("little"));
    } else {
        // Big Endian
        meta_definition_add_bool(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__big_endian__", true);
        meta_definition_add_str(&ast->meta_definitions, &ast->meta_definitions_length, &ast->meta_definitions_capacity, "__endianness__", strclone("big"));
    }
}

void ast_free(ast_t *ast){
    length_t i;

    ast_free_enums(ast->enums, ast->enums_length);
    ast_free_functions(ast->funcs, ast->funcs_length);
    ast_free_function_aliases(ast->func_aliases, ast->func_aliases_length);
    ast_free_composites(ast->composites, ast->composites_length);
    ast_free_globals(ast->globals, ast->globals_length);
    ast_free_constants(ast->constants, ast->constants_length);
    ast_free_aliases(ast->aliases, ast->aliases_length);

    for(length_t l = 0; l != ast->libraries_length; l++){
        free(ast->libraries[l]);
    }

    free(ast->enums);
    free(ast->funcs);
    free(ast->func_aliases);
    free(ast->composites);
    free(ast->constants);
    free(ast->globals);
    free(ast->aliases);
    free(ast->libraries);
    free(ast->library_kinds);

    ast_type_free(&ast->common.ast_int_type);
    ast_type_free(&ast->common.ast_usize_type);

    if(ast->common.ast_variadic_array != NULL){
        ast_type_free_fully(ast->common.ast_variadic_array);
    }
    
    type_table_free(ast->type_table);
    free(ast->type_table);

    for(i = 0; i != ast->meta_definitions_length; i++){
        meta_expr_free_fully(ast->meta_definitions[i].value);
    }
    free(ast->meta_definitions);
    free(ast->polymorphic_funcs);
    free(ast->polymorphic_methods);
    for(i = 0; i != ast->polymorphic_composites_length; i++){
        ast_polymorphic_composite_t *poly_composite = &ast->polymorphic_composites[i];

        ast_free_composites((ast_composite_t*) poly_composite, 1);
        freestrs(poly_composite->generics, poly_composite->generics_length);
    }
    free(ast->polymorphic_composites);
}

void ast_free_functions(ast_func_t *functions, length_t functions_length){
    for(length_t i = 0; i != functions_length; i++){
        ast_func_t *func = &functions[i];
        free(func->name);
 
        if(func->traits & AST_FUNC_FOREIGN){
            for(length_t a = 0; a != func->arity; a++){
                ast_type_free(&func->arg_types[a]);
            }
        } else {
            for(length_t a = 0; a != func->arity; a++){
                free(func->arg_names[a]);
                ast_type_free(&func->arg_types[a]);
            }
        }

        free(func->arg_names);
        free(func->arg_types);
        free(func->arg_sources);
        free(func->arg_flows);
        free(func->arg_type_traits);

        if(func->arg_defaults) ast_exprs_free_fully(func->arg_defaults, func->arity);
        
        free(func->variadic_arg_name);
        ast_free_statements(func->statements, func->statements_length);
        free(func->statements);
        ast_type_free(&func->return_type);
        free(func->export_as);
    }
}

void ast_free_function_aliases(ast_func_alias_t *faliases, length_t length){
    for(length_t i = 0; i != length; i++){
        ast_func_alias_t *falias = &faliases[i];
        free(falias->from);
        ast_types_free_fully(falias->arg_types, falias->arity);
    }
}

void ast_free_statements(ast_expr_t **statements, length_t length){
    for(length_t s = 0; s != length; s++){
        ast_expr_free(statements[s]); // Free statement-specific memory
        free(statements[s]); // Free statement memory
    }
}

void ast_free_statements_fully(ast_expr_t **statements, length_t length){
    ast_free_statements(statements, length);
    free(statements);
}

void ast_free_composites(ast_composite_t *composites, length_t composites_length){
    for(length_t i = 0; i != composites_length; i++){
        ast_composite_t *composite = &composites[i];

        ast_layout_free(&composite->layout);
        free(composite->name);
    }
}

void ast_free_constants(ast_constant_t *constants, length_t constants_length){
    for(length_t i = 0; i != constants_length; i++){
        ast_constant_t *constant = &constants[i];
        if(constant->expression) ast_expr_free_fully(constant->expression);
        free(constant->name);
    }
}

void ast_free_aliases(ast_alias_t *aliases, length_t aliases_length){
    for(length_t i = 0; i != aliases_length; i++){
        free(aliases[i].name);
        ast_type_free(&aliases[i].type);
    }
}

void ast_free_globals(ast_global_t *globals, length_t globals_length){
    for(length_t i = 0; i != globals_length; i++){
        ast_global_t *global = &globals[i];
        free(global->name);
        ast_type_free(&global->type);
        if(global->initial != NULL){
            ast_expr_free_fully(global->initial);
        }
    }
}

void ast_free_enums(ast_enum_t *enums, length_t enums_length){
    for(length_t i = 0; i != enums_length; i++){
        ast_enum_t *enum_definition = &enums[i];
        free(enum_definition->name);
        free(enum_definition->kinds);
    }
}

void ast_dump(ast_t *ast, const char *filename){
    FILE *file = fopen(filename, "w");
    length_t i;

    if(file == NULL){
        internalerrorprintf("Failed to open ast dump file\n");
        return;
    }

    ast_dump_enums(file, ast->enums, ast->enums_length);
    ast_dump_composites(file, ast->composites, ast->composites_length);
    ast_dump_globals(file, ast->globals, ast->globals_length);
    ast_dump_functions(file, ast->funcs, ast->funcs_length);

    for(i = 0; i != ast->aliases_length; i++){
        ast_alias_t *alias = &ast->aliases[i];

        char *s = ast_type_str(&alias->type);
        fprintf(file, "alias %s = %s\n", alias->name, s);
        free(s);
    }

    for(i = 0; i != ast->libraries_length; i++){
        fprintf(file, "foreign '%s'\n", ast->libraries[i]);
    }

    fclose(file);
}

void ast_dump_functions(FILE *file, ast_func_t *functions, length_t functions_length){
    for(length_t i = 0; i != functions_length; i++){
        char *arguments_string = malloc(256);
        length_t arguments_string_length = 0;
        length_t arguments_string_capacity = 256;
        ast_func_t *func = &functions[i];
        arguments_string[0] = '\0';

        for(length_t a = 0; a != func->arity; a++){
            char *type_string = ast_type_str(&func->arg_types[a]);
            length_t name_length = (func->arg_names == NULL) ? 0 : strlen(func->arg_names[a]);
            length_t type_length = strlen(type_string);

            while(arguments_string_length + name_length + type_length + 3 >= arguments_string_capacity){
                char *new_arguments_string = malloc(arguments_string_capacity * 2);
                memcpy(new_arguments_string, arguments_string, arguments_string_length + 1);
                free(arguments_string);
                arguments_string = new_arguments_string;
                arguments_string_capacity *= 2;
            }

            if(func->traits & AST_FUNC_FOREIGN){
                if(a + 1 == func->arity){
                    memcpy(&arguments_string[arguments_string_length], type_string, type_length + 1);
                    arguments_string_length += type_length;
                } else {
                    memcpy(&arguments_string[arguments_string_length], type_string, type_length);
                    memcpy(&arguments_string[arguments_string_length + type_length], ", ", 3);
                    arguments_string_length += type_length + 2;
                }
            } else {
                memcpy(&arguments_string[arguments_string_length], func->arg_names[a], name_length);
                memcpy(&arguments_string[arguments_string_length + name_length], " ", 1);

                if(a + 1 == func->arity){
                    memcpy(&arguments_string[arguments_string_length + name_length + 1], type_string, type_length + 1);
                    arguments_string_length += name_length + type_length + 1;
                } else {
                    memcpy(&arguments_string[arguments_string_length + name_length + 1], type_string, type_length);
                    memcpy(&arguments_string[arguments_string_length + name_length + 1 + type_length], ", ", 3);
                    arguments_string_length += name_length + type_length + 3;
                }
            }

            free(type_string);
        }

        if(func->traits & AST_FUNC_VARARG){
            if(arguments_string_length + 6 >= arguments_string_capacity){
                arguments_string_capacity += 6;
                char *new_arguments_string = malloc(arguments_string_capacity);
                memcpy(new_arguments_string, arguments_string, arguments_string_length + 1);
                free(arguments_string);
                arguments_string = new_arguments_string;
            }

            memcpy(&arguments_string[arguments_string_length], ", ...", 6);
        }

        if(func->traits & AST_FUNC_VARIADIC){
            if(arguments_string_length + 5 >= arguments_string_capacity){
                arguments_string_capacity += 5;
                char *new_arguments_string = malloc(arguments_string_capacity);
                memcpy(new_arguments_string, arguments_string, arguments_string_length + 1);
                free(arguments_string);
                arguments_string = new_arguments_string;
            }

            memcpy(&arguments_string[arguments_string_length], ", ..", 5);
        }

        char *return_type_string = ast_type_str(&func->return_type);

        if(func->traits & AST_FUNC_FOREIGN){
            fprintf(file, "foreign %s(%s) %s\n", func->name, arguments_string, return_type_string);
        } else {
            fprintf(file, "func %s(%s) %s {\n", func->name, arguments_string, return_type_string);
            if(func->statements != NULL) ast_dump_statements(file, func->statements, func->statements_length, 1);
            fprintf(file, "}\n");
        }

        free(arguments_string);
        free(return_type_string);
    }
}

void ast_dump_statements(FILE *file, ast_expr_t **statements, length_t length, length_t indentation){
    for(length_t s = 0; s != length; s++){
        // Print indentation
        indent(file, indentation);

        // Print statement
        switch(statements[s]->id){
        case EXPR_RETURN: {
                ast_expr_t *return_value = ((ast_expr_return_t*) statements[s])->value;
                ast_expr_list_t last_minute = ((ast_expr_return_t*) statements[s])->last_minute;

                if(last_minute.length != 0){
                    fprintf(file, "<deferred> {\n");
                    ast_dump_statements(file, last_minute.statements, last_minute.length, indentation + 1);

                    // Indent Remaining
                    indent(file, indentation);
                    fprintf(file, "}\n");

                    indent(file, indentation);
                }

                char *return_value_str = return_value == NULL ? "" : ast_expr_str(return_value);
                fprintf(file, "return %s\n", return_value_str);
                if(return_value != NULL) free(return_value_str);
            }
            break;
        case EXPR_CALL: {
                ast_expr_call_t *call_stmt = (ast_expr_call_t*) statements[s];

                fprintf(file, "%s%s(", call_stmt->name, call_stmt->is_tentative ? "?" : "");
                for(length_t arg_index = 0; arg_index != call_stmt->arity; arg_index++){
                    char *arg_str = ast_expr_str(call_stmt->args[arg_index]);
                    if(arg_index + 1 != call_stmt->arity) fprintf(file, "%s, ", arg_str);
                    else fprintf(file, "%s", arg_str);
                    free(arg_str);
                }
                fprintf(file, ")\n");
            }
            break;
        case EXPR_DECLARE: case EXPR_DECLAREUNDEF: {
                bool is_undef = statements[s]->id == EXPR_DECLAREUNDEF;
                ast_expr_declare_t *declare_stmt = (ast_expr_declare_t*) statements[s];

                if(declare_stmt->is_const)  fprintf(file, "const ");
                if(declare_stmt->is_static) fprintf(file, "static ");

                char *variable_type_str = ast_type_str(&declare_stmt->type);
                fprintf(file, (declare_stmt->value == NULL && !is_undef) ? "%s %s\n" : "%s %s = ", declare_stmt->name, variable_type_str);
                free(variable_type_str);

                if(is_undef){
                    fprintf(file, "undef\n");
                } else if(declare_stmt->value != NULL){
                    char *initial_value = ast_expr_str(declare_stmt->value);
                    fprintf(file, "%s\n", initial_value);
                    free(initial_value);
                }
            }
            break;
        case EXPR_ASSIGN: case EXPR_ADD_ASSIGN: case EXPR_SUBTRACT_ASSIGN: case EXPR_MULTIPLY_ASSIGN:
        case EXPR_DIVIDE_ASSIGN: case EXPR_MODULUS_ASSIGN: case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN: case EXPR_LS_ASSIGN: case EXPR_RS_ASSIGN: case EXPR_LGC_LS_ASSIGN: case EXPR_LGC_RS_ASSIGN:
            {
                const char *operator = NULL;
                switch(statements[s]->id){
                case EXPR_ASSIGN:
                    operator = "=";
                    break;
                case EXPR_ADD_ASSIGN:
                    operator = "+=";
                    break;
                case EXPR_SUBTRACT_ASSIGN:
                    operator = "-=";
                    break;
                case EXPR_MULTIPLY_ASSIGN:
                    operator = "*=";
                    break;
                case EXPR_DIVIDE_ASSIGN:
                    operator = "/=";
                    break;
                case EXPR_MODULUS_ASSIGN:
                    operator = "%=";
                    break;
                case EXPR_AND_ASSIGN:
                    operator = "&=";
                    break;
                case EXPR_OR_ASSIGN:
                    operator = "|=";
                    break;
                case EXPR_XOR_ASSIGN:
                    operator = "^=";
                    break;
                case EXPR_LS_ASSIGN:
                    operator = "<<=";
                    break;
                case EXPR_RS_ASSIGN:
                    operator = ">>=";
                    break;
                case EXPR_LGC_LS_ASSIGN:
                    operator = "<<<=";
                    break;
                case EXPR_LGC_RS_ASSIGN:
                    operator = ">>>=";
                    break;
                default:
                    operator = "¿=";
                }
                
                ast_expr_assign_t *assign_stmt = (ast_expr_assign_t*) statements[s];
                char *destination_str = ast_expr_str(assign_stmt->destination);
                char *value_str = ast_expr_str(assign_stmt->value);
                fprintf(file, "%s %s %s\n", destination_str, operator, value_str);
                free(destination_str);
                free(value_str);
            }
            break;
        case EXPR_IF: case EXPR_UNLESS: case EXPR_WHILE: case EXPR_UNTIL: {
                ast_expr_t *if_value = ((ast_expr_if_t*) statements[s])->value;
                strong_cstr_t if_value_str = ast_expr_str(if_value);
                weak_cstr_t keyword_name;

                switch(statements[s]->id){
                case EXPR_IF:     keyword_name = "if";     break;
                case EXPR_UNLESS: keyword_name = "unless"; break;
                case EXPR_WHILE:  keyword_name = "while";  break;
                case EXPR_UNTIL:  keyword_name = "until";  break;
                default: keyword_name = "<unknown conditional keyword>";
                }

                fprintf(file, "%s %s {\n", keyword_name, if_value_str);
                ast_dump_statements(file, ((ast_expr_if_t*) statements[s])->statements, ((ast_expr_if_t*) statements[s])->statements_length, indentation+1);
                indent(file, indentation);
                fprintf(file, "}\n");
                free(if_value_str);
            }
            break;
        case EXPR_IFELSE: case EXPR_UNLESSELSE: {
                ast_expr_t *if_value = ((ast_expr_ifelse_t*) statements[s])->value;
                strong_cstr_t if_value_str = ast_expr_str(if_value);
                fprintf(file, "%s %s {\n", (statements[s]->id == EXPR_IFELSE ? "if" : "unless"), if_value_str);
                ast_dump_statements(file, ((ast_expr_ifelse_t*) statements[s])->statements, ((ast_expr_ifelse_t*) statements[s])->statements_length, indentation+1);
                indent(file, indentation);
                fprintf(file, "} else {\n");
                ast_dump_statements(file, ((ast_expr_ifelse_t*) statements[s])->else_statements, ((ast_expr_ifelse_t*) statements[s])->else_statements_length, indentation+1);
                indent(file, indentation);
                fprintf(file, "}\n");
                free(if_value_str);
            }
            break;
        case EXPR_REPEAT: {
                ast_expr_repeat_t *repeat = (ast_expr_repeat_t*) statements[s];

                ast_expr_t *value = repeat->limit;
                strong_cstr_t value_str = ast_expr_str(value);
                fprintf(file, "repeat %s%s ", repeat->is_static ? "static " : "", value_str);

                if(repeat->idx_overload_name){
                    fprintf(file, "using %s ", repeat->idx_overload_name);
                }

                fprintf(file, "{\n");
                ast_dump_statements(file, repeat->statements, repeat->statements_length, indentation + 1);
                indent(file, indentation);
                fprintf(file, "}\n");
                free(value_str);
            }
            break;
        case EXPR_EACH_IN: {
                ast_expr_each_in_t *each_in = (ast_expr_each_in_t*) statements[s];

                strong_cstr_t it_type_name = ast_type_str(each_in->it_type);
                fprintf(file, "each %s%s%s in %s", each_in->it_name ? each_in->it_name : "", each_in->it_name ? " " : "", it_type_name, each_in->is_static ? "static ": "");
                free(it_type_name);
                
                if(each_in->list){
                    strong_cstr_t value_str = ast_expr_str(each_in->list);
                    fprintf(file, "%s {\n", value_str);
                    free(value_str);
                } else {
                    strong_cstr_t array_str = ast_expr_str(each_in->low_array);
                    strong_cstr_t length_str = ast_expr_str(each_in->length);
                    fprintf(file, "[%s, %s] {\n", array_str, length_str);
                    free(array_str);
                    free(length_str);
                }

                ast_dump_statements(file, each_in->statements, each_in->statements_length, indentation + 1);
                indent(file, indentation);
                fprintf(file, "}\n");
            }
            break;
        case EXPR_CALL_METHOD: {
                ast_expr_call_method_t *call_stmt = (ast_expr_call_method_t*) statements[s];
                char *value_str = ast_expr_str(call_stmt->value);
                fprintf(file, "%s.%s%s(", value_str, call_stmt->name, call_stmt->is_tentative ? "?" : "");
                free(value_str);
                for(length_t arg_index = 0; arg_index != call_stmt->arity; arg_index++){
                    char *arg_str = ast_expr_str(call_stmt->args[arg_index]);
                    if(arg_index + 1 != call_stmt->arity) fprintf(file, "%s, ", arg_str);
                    else fprintf(file, "%s", arg_str);
                    free(arg_str);
                }
                fprintf(file, ")\n");
            }
            break;
        case EXPR_DELETE: {
                ast_expr_t *delete_value = ((ast_expr_unary_t*) statements[s])->value;
                char *delete_value_str = ast_expr_str(delete_value);
                fprintf(file, "delete %s\n", delete_value_str);
                free(delete_value_str);
            }
            break;
        case EXPR_SWITCH: {
                ast_expr_switch_t *switch_stmt = (ast_expr_switch_t*) statements[s];
                strong_cstr_t value_str = ast_expr_str(switch_stmt->value);
                fprintf(file, "switch (%s) {\n", value_str);
                free(value_str);

                for(length_t c = 0; c != switch_stmt->cases_length; c++){
                    indent(file, indentation);
                    ast_case_t *expr_case = &switch_stmt->cases[c];
                    strong_cstr_t condition_str = ast_expr_str(expr_case->condition);
                    fprintf(file, "case (%s)\n", condition_str);
                    free(condition_str);
                    ast_dump_statements(file, expr_case->statements, expr_case->statements_length, indentation + 1);
                }

                if(switch_stmt->default_statements_length != 0){
                    indent(file, indentation);
                    fprintf(file, "default\n");
                    ast_dump_statements(file, switch_stmt->default_statements, switch_stmt->default_statements_length, indentation + 1);
                }

                indent(file, indentation);
                fprintf(file, "}\n");
            }
            break;
        case EXPR_BREAK:
            fprintf(file, "break\n");
            break;
        case EXPR_CONTINUE:
            fprintf(file, "continue\n");
            break;
        case EXPR_BREAK_TO: {
                ast_expr_break_to_t *break_to_stmt = (ast_expr_break_to_t*) statements[s];
                fprintf(file, "break %s\n", break_to_stmt->label);
            }
            break;
        case EXPR_CONTINUE_TO: {
                ast_expr_continue_to_t *continue_to_stmt = (ast_expr_continue_to_t*) statements[s];
                fprintf(file, "continue %s\n", continue_to_stmt->label);
            }
            break;
        case EXPR_VA_START: case EXPR_VA_END: {
                ast_expr_t *value = ((ast_expr_va_start_t*) statements[s])->value;
                char *value_str = ast_expr_str(value);
                fprintf(file, "%s %s\n", statements[s]->id == EXPR_VA_START ? "va_start" : "va_end", value_str);
                free(value_str);
            }
            break;
        case EXPR_VA_COPY: {
                ast_expr_t *dest_value = ((ast_expr_va_copy_t*) statements[s])->dest_value;
                ast_expr_t *src_value = ((ast_expr_va_copy_t*) statements[s])->src_value;
                char *dest_str = ast_expr_str(dest_value);
                char *src_str = ast_expr_str(src_value);
                fprintf(file, "va_copy(%s, %s)\n", dest_str, src_str);
                free(dest_str);
                free(src_str);
            }
            break;
        case EXPR_LLVM_ASM: {
                ast_expr_llvm_asm_t *llvm_asm_stmt = (ast_expr_llvm_asm_t*) statements[s];
                char *assembly = string_to_escaped_string(llvm_asm_stmt->assembly, strlen(llvm_asm_stmt->assembly), '\"');
                fprintf(file, "llvm_asm ... { \"%s\" } ... (...)\n", assembly);
                free(assembly);
            }
            break;
        default:
            fprintf(file, "<unknown statement>\n");
        }
    }
}

void ast_dump_composites(FILE *file, ast_composite_t *composites, length_t composites_length){
    for(length_t i = 0; i != composites_length; i++){
        ast_dump_composite(file, &composites[i], 0);
    }
}

void ast_dump_composite(FILE *file, ast_composite_t *composite, length_t additional_indentation){
    ast_layout_t *layout = &composite->layout;

    indent(file, additional_indentation);

    // Dump composite traits
    if(layout->traits & AST_LAYOUT_PACKED){
        fprintf(file, "packed ");
    }

    // Dump composite kind
    switch(layout->kind){
    case AST_LAYOUT_STRUCT:
        fprintf(file, "struct ");
        break;
    case AST_LAYOUT_UNION:
        fprintf(file, "union ");
        break;
    default:
        internalerrorprintf("ast_dump_composite() got unknown layout kind\n");
        return;
    }

    // Dump generics "<$K, $V>" if the composite is polymorphic
    if(composite->is_polymorphic){
        ast_polymorphic_composite_t *poly_composite = (ast_polymorphic_composite_t*) composite;
        bool printed_first_generic = false;

        fprintf(file, "<");

        for(length_t i = 0; i != poly_composite->generics_length; i++){
            fprintf(file, "$%s", poly_composite->generics[i]);

            if(printed_first_generic){
                fprintf(file, ", ");
            } else {
                printed_first_generic = true;
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

void ast_dump_composite_subfields(FILE *file, ast_layout_skeleton_t *skeleton, ast_field_map_t *field_map, ast_layout_endpoint_t parent_endpoint, length_t indentation){
    bool has_printed_first_field = false;

    for(length_t i = 0; i != skeleton->bones_length; i++){
        if(has_printed_first_field){
            fprintf(file, ",\n");
        } else {
            has_printed_first_field = true;
        }

        ast_layout_bone_t *bone = &skeleton->bones[i];

        ast_layout_endpoint_t endpoint = parent_endpoint;
        ast_layout_endpoint_add_index(&endpoint, i);

        indent(file, indentation);
        if(bone->traits & AST_LAYOUT_BONE_PACKED) fprintf(file, "packed ");

        switch(bone->kind){
        case AST_LAYOUT_BONE_KIND_TYPE: {
                char *field_name = ast_field_map_get_name_of_endpoint(field_map, endpoint);
                assert(field_name);

                char *s = ast_type_str(&bone->type);
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
            internalerrorprintf("ast_dump_composite() got unknown bone kind\n");
            return;
        }
    }

    fprintf(file, "\n");
}

void ast_dump_globals(FILE *file, ast_global_t *globals, length_t globals_length){
    for(length_t i = 0; i != globals_length; i++){
        ast_global_t *global = &globals[i];
        char *global_typename = ast_type_str(&global->type);

        if(global->initial == NULL){
            fprintf(file, "%s %s\n", global->name, global_typename);
        } else {
            char *value = ast_expr_str(global->initial);
            fprintf(file, "%s %s = %s\n", global->name, global_typename, value);
            free(value);
        }

        free(global_typename);
    }
}

void ast_dump_enums(FILE *file, ast_enum_t *enums, length_t enums_length){
    for(length_t i = 0; i != enums_length; i++){
        ast_enum_t *enum_definition = &enums[i];

        char *kinds_string = NULL;
        length_t kinds_string_length = 0;
        length_t kinds_string_capacity = 0;

        for(length_t i = 0; i != enum_definition->length; i++){
            const char *kind_name = enum_definition->kinds[i];
            length_t kind_name_length = strlen(kind_name);
            expand((void**) &kinds_string, sizeof(char), kinds_string_length, &kinds_string_capacity, kind_name_length + 2, 64);

            memcpy(&kinds_string[kinds_string_length], kind_name, kind_name_length);
            kinds_string_length += kind_name_length;

            if(i + 1 == enum_definition->length){
                kinds_string[kinds_string_length] = '\0';
            } else {
                memcpy(&kinds_string[kinds_string_length], ", ", 2);
                kinds_string_length += 2;
            }
        }


        fprintf(file, "%s (%s)\n", enum_definition->name, kinds_string ? kinds_string : "");
        free(kinds_string);
    }
}

void ast_func_create_template(ast_func_t *func, strong_cstr_t name, bool is_stdcall, bool is_foreign, bool is_verbatim,
        bool is_implicit, source_t source, bool is_entry, maybe_null_strong_cstr_t export_as){
    
    func->name = name;
    func->arg_names = NULL;
    func->arg_types = NULL;
    func->arg_sources = NULL;
    func->arg_flows = NULL;
    func->arg_type_traits = NULL;
    func->arg_defaults = NULL;
    func->arity = 0;
    func->return_type.elements = NULL;
    func->return_type.elements_length = 0;
    func->return_type.source = NULL_SOURCE;
    func->return_type.source.object_index = source.object_index;
    func->traits = TRAIT_NONE;
    func->variadic_arg_name = NULL;
    func->variadic_source = NULL_SOURCE;
    func->statements = NULL;
    func->statements_length = 0;
    func->statements_capacity = 0;
    func->source = source;
    func->export_as = export_as;

    if(is_entry)                       func->traits |= AST_FUNC_MAIN;
    if(strcmp(name, "__defer__") == 0) func->traits |= AST_FUNC_DEFER | (is_verbatim ? TRAIT_NONE : AST_FUNC_AUTOGEN);
    if(strcmp(name, "__pass__") == 0)  func->traits |= AST_FUNC_PASS  | (is_verbatim ? TRAIT_NONE : AST_FUNC_AUTOGEN);
    if(is_stdcall)                     func->traits |= AST_FUNC_STDCALL;
    if(is_foreign)                     func->traits |= AST_FUNC_FOREIGN;
    if(is_implicit)                    func->traits |= AST_FUNC_IMPLICIT;
}

bool ast_func_is_polymorphic(ast_func_t *func){
    for(length_t i = 0; i != func->arity; i++){
        if(ast_type_has_polymorph(&func->arg_types[i])) return true;
    }
    if(ast_type_has_polymorph(&func->return_type)) return true;
    return false;
}

void ast_composite_init(ast_composite_t *composite, strong_cstr_t name, ast_layout_t layout, source_t source){
    composite->name = name;
    composite->layout = layout;
    composite->source = source;
    composite->is_polymorphic = false;
}

void ast_polymorphic_composite_init(ast_polymorphic_composite_t *composite, strong_cstr_t name, ast_layout_t layout, source_t source, strong_cstr_t *generics, length_t generics_length){
    composite->name = name;
    composite->layout = layout;
    composite->source = source;
    composite->is_polymorphic = true;
    composite->generics = generics;
    composite->generics_length = generics_length;
}

void ast_alias_init(ast_alias_t *alias, weak_cstr_t name, ast_type_t type, trait_t traits, source_t source){
    alias->name = name;
    alias->type = type;
    alias->traits = traits;
    alias->source = source;
}

void ast_enum_init(ast_enum_t *enum_definition,  weak_cstr_t name, weak_cstr_t *kinds, length_t length, source_t source){
    enum_definition->name = name;
    enum_definition->kinds = kinds;
    enum_definition->length = length;
    enum_definition->source = source;
}

ast_composite_t *ast_composite_find_exact(ast_t *ast, const char *name){
    // TODO: Maybe sort and do a binary search or something
    for(length_t i = 0; i != ast->composites_length; i++){
        if(strcmp(ast->composites[i].name, name) == 0){
            return &ast->composites[i];
        }
    }
    return NULL;
}

successful_t ast_composite_find_exact_field(ast_composite_t *composite, const char *name, ast_layout_endpoint_t *out_endpoint, ast_layout_endpoint_path_t *out_path){
    if(!ast_field_map_find(&composite->layout.field_map, name, out_endpoint)) return false;
    if(!ast_layout_get_path(&composite->layout, *out_endpoint, out_path)) return false;
    return true;
}

ast_polymorphic_composite_t *ast_polymorphic_composite_find_exact(ast_t *ast, const char *name){
    // TODO: Maybe sort and do a binary search or something
    for(length_t i = 0; i != ast->polymorphic_composites_length; i++){
        if(strcmp(ast->polymorphic_composites[i].name, name) == 0){
            return &ast->polymorphic_composites[i];
        }
    }
    return NULL;
}

successful_t ast_enum_find_kind(ast_enum_t *ast_enum, const char *name, length_t *out_index){
    for(length_t i = 0; i != ast_enum->length; i++){
        if(strcmp(ast_enum->kinds[i], name) == 0){
            *out_index = i;
            return true;
        }
    }
    return false;
}

maybe_index_t ast_find_alias(ast_alias_t *aliases, length_t aliases_length, const char *alias){
    // If not found returns -1 else returns index inside array

    maybe_index_t first, middle, last, comparison;
    first = 0; last = aliases_length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(aliases[middle].name, alias);

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

maybe_index_t ast_find_constant(ast_constant_t *constants, length_t constants_length, const char *constant){
    // If not found returns -1 else returns index inside array

    maybe_index_t first, middle, last, comparison;
    first = 0; last = constants_length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(constants[middle].name, constant);

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

maybe_index_t ast_find_enum(ast_enum_t *enums, length_t enums_length, const char *enum_name){
    // If not found returns -1 else returns index inside array

    maybe_index_t first, middle, last, comparison;
    first = 0; last = enums_length - 1;

    while(first <= last){
        middle = (first + last) / 2;
        comparison = strcmp(enums[middle].name, enum_name);

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

maybe_index_t ast_find_global(ast_global_t *globals, length_t globals_length, weak_cstr_t name){
    // If not found returns -1 else returns index inside array

    maybe_index_t first, middle, last, comparison;
    first = 0; last = globals_length - 1;

    ast_global_t target;
    target.name = name;
    target.name_length = strlen(name);
    // (neglect other fields of 'target')

    while(first <= last){
        middle = (first + last) / 2;
        comparison = ast_globals_cmp(&globals[middle], &target);

        if(comparison == 0) return middle;
        else if(comparison > 0) last = middle - 1;
        else first = middle + 1;
    }

    return -1;
}

void ast_add_alias(ast_t *ast, strong_cstr_t name, ast_type_t strong_type, trait_t traits, source_t source){
    expand((void**) &ast->aliases, sizeof(ast_alias_t), ast->aliases_length, &ast->aliases_capacity, 1, 8);

    ast_alias_t *alias = &ast->aliases[ast->aliases_length++];
    ast_alias_init(alias, name, strong_type, traits, source);
}

void ast_add_enum(ast_t *ast, strong_cstr_t name, weak_cstr_t *kinds, length_t length, source_t source){
    expand((void**) &ast->enums, sizeof(ast_enum_t), ast->enums_length, &ast->enums_capacity, 1, 4);
    ast_enum_init(&ast->enums[ast->enums_length++], name, kinds, length, source);
}

ast_composite_t *ast_add_composite(ast_t *ast, strong_cstr_t name, ast_layout_t layout, source_t source){
    expand((void**) &ast->composites, sizeof(ast_composite_t), ast->composites_length, &ast->composites_capacity, 1, 4);

    ast_composite_t *composite = &ast->composites[ast->composites_length++];
    ast_composite_init(composite, name, layout, source);
    return composite;
}

ast_polymorphic_composite_t *ast_add_polymorphic_composite(ast_t *ast, strong_cstr_t name, ast_layout_t layout, source_t source, strong_cstr_t *generics, length_t generics_length){
    expand((void**) &ast->polymorphic_composites, sizeof(ast_polymorphic_composite_t), ast->polymorphic_composites_length, &ast->polymorphic_composites_capacity, 1, 4);

    ast_polymorphic_composite_t *poly_composite = &ast->polymorphic_composites[ast->polymorphic_composites_length++];
    ast_polymorphic_composite_init(poly_composite, name, layout, source, generics, generics_length);
    return poly_composite;
}

void ast_add_global(ast_t *ast, strong_cstr_t name, ast_type_t type, ast_expr_t *initial_value, trait_t traits, source_t source){
    expand((void**) &ast->globals, sizeof(ast_global_t), ast->globals_length, &ast->globals_capacity, 1, 8);

    ast_global_t *global = &ast->globals[ast->globals_length++];
    global->name = name;
    global->name_length = strlen(name);
    global->type = type;
    global->initial = initial_value;
    global->traits = traits;
    global->source = source;
}

void ast_add_foreign_library(ast_t *ast, strong_cstr_t library, char kind){
    coexpand((void**) &ast->libraries, sizeof(char*), (void**) &ast->library_kinds, sizeof(char), ast->libraries_length, &ast->libraries_capacity, 1, 4);
    ast->libraries[ast->libraries_length] = library;
    ast->library_kinds[ast->libraries_length++] = kind;
}

void va_args_inject_ast(compiler_t *compiler, ast_t *ast){
    ast_layout_t layout;

    if(compiler->cross_compile_for == CROSS_COMPILE_NONE && sizeof(va_list) <= 8){
        // Small va_list
        strong_cstr_t names[1];
        names[0] = strclone("_opaque");

        ast_type_t types[1];
        ast_type_make_base(&types[0], strclone("ptr"));

        ast_layout_init_with_struct_fields(&layout, names, types, 1);
        ast_add_composite(ast, strclone("va_list"), layout, NULL_SOURCE);
    } else {
        // Larger Intel x86_64 va_list

        // Don't show va_list size warning if we are cross compiling.
        // When cross compiling, we'll make the conservative guess that 'va_list' will
        // be 24 bytes or smaller
        if(compiler->cross_compile_for == CROSS_COMPILE_NONE && sizeof(va_list) != 24){
            // Neglect whether to terminate, since this is not a fixable warning
            compiler_warnf(compiler, NULL_SOURCE, "Assuming Intel x86_64 va_list\n");
        }

        strong_cstr_t names[4];
        names[0] = strclone("_opaque1");
        names[1] = strclone("_opaque2");
        names[2] = strclone("_opaque3");
        names[3] = strclone("_opaque4");

        ast_type_t types[4];
        ast_type_make_base(&types[0], strclone("int"));
        ast_type_make_base(&types[1], strclone("int"));
        ast_type_make_base(&types[2], strclone("ptr"));
        ast_type_make_base(&types[3], strclone("ptr"));
        
        ast_layout_init_with_struct_fields(&layout, names, types, 1);
        ast_add_composite(ast, strclone("va_list"), layout, NULL_SOURCE);
    }
}

int ast_aliases_cmp(const void *a, const void *b){
    return strcmp(((ast_alias_t*) a)->name, ((ast_alias_t*) b)->name);
}

int ast_constants_cmp(const void *a, const void *b){
    return strcmp(((ast_constant_t*) a)->name, ((ast_constant_t*) b)->name);
}

int ast_enums_cmp(const void *a, const void *b){
    return strcmp(((ast_enum_t*) a)->name, ((ast_enum_t*) b)->name);
}

int ast_polymorphic_funcs_cmp(const void *a, const void *b){
    int diff = strcmp(((ast_polymorphic_func_t*) a)->name, ((ast_polymorphic_func_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ast_polymorphic_func_t*) a)->ast_func_id - (int) ((ast_polymorphic_func_t*) b)->ast_func_id;
}

int ast_globals_cmp(const void *ga, const void *gb){
    #define global_a ((ast_global_t*) ga)
    #define global_b ((ast_global_t*) gb)

    if(global_a->name_length != global_b->name_length){
        // DANGEROUS: Assume that the length difference isn't of
        // magnitudes big enough to cause overflow of 'int' value
        return (int) global_a->name_length - (int) global_b->name_length;
    }

    return strncmp(global_a->name, global_b->name, global_a->name_length);
    
    #undef global_a
    #undef global_b
}
