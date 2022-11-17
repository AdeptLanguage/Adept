
#include "AST/ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/UTIL/string_builder_extensions.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"

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
    ast->named_expressions = (ast_named_expression_list_t){0};
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
    ast->common.ast_int_type = ast_type_make_base(strclone("int"));
    ast->common.ast_usize_type = ast_type_make_base(strclone("usize"));
    ast->common.ast_variadic_array = NULL;
    ast->common.ast_initializer_list = NULL;

    ast->type_table = NULL;
    ast->meta_definitions = NULL;
    ast->meta_definitions_length = 0;
    ast->meta_definitions_capacity = 0;
    ast->poly_funcs = NULL;
    ast->poly_funcs_length = 0;
    ast->poly_funcs_capacity = 0;
    ast->polymorphic_methods = NULL;
    ast->polymorphic_methods_length = 0;
    ast->polymorphic_methods_capacity = 0;
    ast->poly_composites = NULL;
    ast->poly_composites_length = 0;
    ast->poly_composites_capacity = 0;

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
    #if defined(__unix__) || defined(__unix) || defined(unix) || defined(__APPLE__) || defined(__MACH__)
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
    ast_free_enums(ast->enums, ast->enums_length);
    ast_free_functions(ast->funcs, ast->funcs_length);
    ast_free_function_aliases(ast->func_aliases, ast->func_aliases_length);
    ast_free_composites(ast->composites, ast->composites_length);
    ast_free_globals(ast->globals, ast->globals_length);
    ast_named_expression_list_free(&ast->named_expressions);
    ast_free_aliases(ast->aliases, ast->aliases_length);

    for(length_t i = 0; i != ast->libraries_length; i++){
        free(ast->libraries[i]);
    }

    free(ast->enums);
    free(ast->funcs);
    free(ast->func_aliases);
    free(ast->composites);
    free(ast->globals);
    free(ast->aliases);
    free(ast->libraries);
    free(ast->library_kinds);

    ast_type_free(&ast->common.ast_int_type);
    ast_type_free(&ast->common.ast_usize_type);
    ast_type_free_fully(ast->common.ast_variadic_array);
    ast_type_free_fully(ast->common.ast_initializer_list);

    type_table_free(ast->type_table);
    free(ast->type_table);

    for(length_t i = 0; i != ast->meta_definitions_length; i++){
        meta_expr_free_fully(ast->meta_definitions[i].value);
    }

    free(ast->meta_definitions);
    free(ast->poly_funcs);
    free(ast->polymorphic_methods);

    for(length_t i = 0; i != ast->poly_composites_length; i++){
        ast_poly_composite_t *poly_composite = &ast->poly_composites[i];

        ast_free_composites((ast_composite_t*) poly_composite, 1);
        free_strings(poly_composite->generics, poly_composite->generics_length);
    }

    free(ast->poly_composites);
}

void ast_free_functions(ast_func_t *functions, length_t functions_length){
    for(length_t i = 0; i != functions_length; i++){
        ast_func_t *func = &functions[i];
        free(func->name);

        if(func->arg_names){
            free_strings(func->arg_names, func->arity);
        }

        ast_types_free(func->arg_types, func->arity);
        free(func->arg_types);
        free(func->arg_sources);
        free(func->arg_flows);
        free(func->arg_type_traits);

        if(func->arg_defaults) ast_exprs_free_fully(func->arg_defaults, func->arity);
        
        free(func->variadic_arg_name);
        ast_expr_list_free(&func->statements);
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

void ast_free_composites(ast_composite_t *composites, length_t composites_length){
    for(length_t i = 0; i != composites_length; i++){
        ast_composite_t *composite = &composites[i];

        ast_layout_free(&composite->layout);
        ast_type_free(&composite->parent);
        free(composite->name);
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
        ast_expr_free_fully(global->initial);
    }
}

void ast_free_enums(ast_enum_t *enums, length_t enums_length){
    for(length_t i = 0; i != enums_length; i++){
        ast_enum_t *enum_definition = &enums[i];
        free(enum_definition->name);
        free(enum_definition->kinds);
    }
}

strong_cstr_t ast_func_args_str(ast_func_t *func){
    string_builder_t builder;
    string_builder_init(&builder);

    for(length_t i = 0; i != func->arity; i++){
        if(func->arg_names && func->arg_names[i]){
            string_builder_append(&builder, func->arg_names[i]);
            string_builder_append_char(&builder, ' ');
        }

        if(func->arg_type_traits && func->arg_type_traits[i] & AST_FUNC_ARG_TYPE_TRAIT_POD){
            string_builder_append(&builder, "POD ");
        }

        string_builder_append_type(&builder, &func->arg_types[i]);

        if(func->arg_defaults && func->arg_defaults[i]){
            string_builder_append(&builder, " = ");
            string_builder_append_expr(&builder, func->arg_defaults[i]);
        }

        if(i + 1 != func->arity){
            string_builder_append(&builder, ", ");
        }
    }

    if(func->traits & AST_FUNC_VARIADIC){
        if(func->arity != 0){
            string_builder_append(&builder, ", ");
        }

        string_builder_append(&builder, func->variadic_arg_name);
        string_builder_append(&builder, " ...");
    } else if(func->traits & AST_FUNC_VARARG){
        string_builder_append(&builder, ", ...");
    }

    return string_builder_finalize(&builder);
}

strong_cstr_t ast_func_head_str(ast_func_t *func){
    strong_cstr_t args_string = ast_func_args_str(func);
    strong_cstr_t return_type_string = ast_type_str(&func->return_type);
    weak_cstr_t no_discard = func->traits & AST_FUNC_NO_DISCARD ? " exhaustive" : "";
    weak_cstr_t disallow = func->traits & AST_FUNC_DISALLOW ? " = delete" : "";

    strong_cstr_t result;

    if(func->traits & AST_FUNC_FOREIGN){
        result = mallocandsprintf("foreign %s(%s)%s %s%s", func->name, args_string ? args_string : "", no_discard, return_type_string, disallow);
    } else {
        weak_cstr_t maybe_dispatcher = func->traits & AST_FUNC_DISPATCHER ? "[[dispatcher]] " : "";
        weak_cstr_t maybe_virtual = func->traits & AST_FUNC_VIRTUAL ? "virtual " : "";
        weak_cstr_t maybe_override = func->traits & AST_FUNC_OVERRIDE ? "override " : "";
        result = mallocandsprintf("%s%s%sfunc %s(%s)%s %s%s", maybe_dispatcher, maybe_virtual, maybe_override, func->name, args_string ? args_string : "", no_discard, return_type_string, disallow);
    }

    free(args_string);
    free(return_type_string);
    return result;
}

bool ast_func_is_method(ast_func_t *func){
    return func->arity > 0 && func->arg_names && streq(func->arg_names[0], "this") && !(func->traits & AST_FUNC_FOREIGN);
}

maybe_null_weak_cstr_t ast_method_get_subject_typename(ast_func_t *method){
    // Assumes that 'ast_func_is_method(method)' is true

    ast_type_t subject_view = ast_type_dereferenced_view(&method->arg_types[0]);
    ast_elem_t *elem = subject_view.elements[0];

    if(ast_type_is_base(&subject_view)){
        return ((ast_elem_base_t*) elem)->base;
    } else if(ast_type_is_generic_base(&subject_view)){
        return ((ast_elem_generic_base_t*) elem)->name;
    } else {
        return NULL;
    }
}

func_id_t ast_new_func(ast_t *ast){
    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);
    return ast->funcs_length++;
}

void ast_func_create_template(ast_func_t *func, const ast_func_head_t *options){
    func->name = options->name;
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
    func->return_type.source.object_index = options->source.object_index;
    func->traits = TRAIT_NONE;
    func->variadic_arg_name = NULL;
    func->variadic_source = NULL_SOURCE;
    func->statements = (ast_expr_list_t){0};
    func->source = options->source;
    func->export_as = options->export_name;
    func->virtual_origin = INVALID_FUNC_ID;
    func->virtual_dispatcher = INVALID_FUNC_ID;

    #if ADEPT_INSIGHT_BUILD
    func->end_source = options->source;
    #endif

    if(options->is_entry)                 func->traits |= AST_FUNC_MAIN;
    if(streq(options->name, "__defer__")) func->traits |= AST_FUNC_DEFER | (options->prefixes.is_verbatim ? TRAIT_NONE : AST_FUNC_AUTOGEN);
    if(streq(options->name, "__pass__"))  func->traits |= AST_FUNC_PASS  | (options->prefixes.is_verbatim ? TRAIT_NONE : AST_FUNC_AUTOGEN);
    if(options->prefixes.is_stdcall)      func->traits |= AST_FUNC_STDCALL;
    if(options->prefixes.is_implicit)     func->traits |= AST_FUNC_IMPLICIT;
    if(options->prefixes.is_virtual)      func->traits |= AST_FUNC_VIRTUAL;
    if(options->prefixes.is_override)     func->traits |= AST_FUNC_OVERRIDE;
    if(options->is_foreign)               func->traits |= AST_FUNC_FOREIGN;

    // Handle WinMain
    if(streq(options->name, "WinMain") && options->export_name && streq(options->export_name, "WinMain")){
        func->traits |= AST_FUNC_WINMAIN;
    }
}

bool ast_func_has_polymorphic_signature(ast_func_t *func){
    return ast_type_list_has_polymorph(func->arg_types, func->arity)
        || ast_type_has_polymorph(&func->return_type);
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
    // TODO: CLEANUP: SPEED: Maybe sort and do a binary search or something
    for(length_t i = 0; i != ast->composites_length; i++){
        if(streq(ast->composites[i].name, name)){
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

ast_poly_composite_t *ast_poly_composite_find_exact(ast_t *ast, const char *name){
    // TODO: Maybe sort and do a binary search or something
    for(length_t i = 0; i != ast->poly_composites_length; i++){
        if(streq(ast->poly_composites[i].name, name)){
            return &ast->poly_composites[i];
        }
    }
    return NULL;
}

ast_composite_t *ast_find_composite(ast_t *ast, const ast_type_t *type){
    if(type->elements_length != 1) return NULL;

    switch(type->elements[0]->id){
    case AST_ELEM_BASE: {
            const char *target_name = ((ast_elem_base_t*) type->elements[0])->base;

            for(length_t i = 0; i != ast->composites_length; i++){
                ast_composite_t *composite = &ast->composites[i];

                if(streq(composite->name, target_name)){
                    return composite;
                }
            }
        }
        break;
    case AST_ELEM_GENERIC_BASE: {
            ast_elem_generic_base_t *generic_base_elem = (ast_elem_generic_base_t*) type->elements[0];

            const char *target_name = generic_base_elem->name;
            length_t generics_count = generic_base_elem->generics_length;

            for(length_t i = 0; i != ast->poly_composites_length; i++){
                ast_poly_composite_t *poly_composite = &ast->poly_composites[i];

                if(streq(poly_composite->name, target_name) && poly_composite->generics_length == generics_count){
                    return (ast_composite_t*) poly_composite;
                }
            }
        }
        break;
    }

    return NULL;
}

successful_t ast_enum_find_kind(ast_enum_t *ast_enum, const char *name, length_t *out_index){
    for(length_t i = 0; i != ast_enum->length; i++){
        if(streq(ast_enum->kinds[i], name)){
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

bool ast_enum_contains(ast_enum_t *ast_enum, const char *kind_name){
    for(size_t i = 0; i < ast_enum->length; i++){
        if(streq(ast_enum->kinds[i], kind_name)){
            return true;
        }
    }
    return false;
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

bool ast_func_end_is_reachable_inner(ast_expr_list_t *stmts, unsigned int max_depth, unsigned int depth){
    if (depth >= max_depth) return true;

    for(length_t i = 0; i < stmts->length; i++){
        ast_expr_t *stmt = stmts->statements[i];
        switch(stmt->id){
        case EXPR_IFELSE:
        case EXPR_UNLESSELSE: {
            ast_expr_conditional_else_t *conditional = (ast_expr_conditional_else_t*) stmt;

            if(!ast_func_end_is_reachable_inner(&conditional->statements, max_depth, depth + 1)
            && !ast_func_end_is_reachable_inner(&conditional->else_statements, max_depth, depth + 1)){
                return false;
            }
            break;
        }
        case EXPR_RETURN:
            return false;
        }
    }
    
    return true;
}

bool ast_func_end_is_reachable(ast_t *ast, func_id_t ast_func_id){
    // Ensure that the reference we're working with isn't one that was previously invalidated
    return ast_func_end_is_reachable_inner(&ast->funcs[ast_func_id].statements, 20, 0);
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

void ast_add_global_named_expression(ast_t *ast, ast_named_expression_t named_expression){
    ast_named_expression_list_append(&ast->named_expressions, named_expression);
}

void ast_add_poly_func(ast_t *ast, weak_cstr_t func_name_persistent, func_id_t ast_func_id){
    expand((void**) &ast->poly_funcs, sizeof(ast_poly_func_t), ast->poly_funcs_length, &ast->poly_funcs_capacity, 1, 4);

    ast_poly_func_t *poly_func = &ast->poly_funcs[ast->poly_funcs_length++];
    poly_func->name = func_name_persistent;
    poly_func->ast_func_id = ast_func_id;
    poly_func->is_beginning_of_group = -1; // Uncalculated
}

ast_composite_t *ast_add_composite(
    ast_t *ast,
    strong_cstr_t name,
    ast_layout_t layout,
    source_t source,
    ast_type_t maybe_parent,
    bool is_class
){
    expand((void**) &ast->composites, sizeof(ast_composite_t), ast->composites_length, &ast->composites_capacity, 1, 4);

    ast_composite_t *composite = &ast->composites[ast->composites_length++];

    *composite = (ast_composite_t){
        .name = name,
        .layout = layout,
        .source = source,
        .parent = maybe_parent,
        .is_polymorphic = false,
        .is_class = is_class,
        .has_constructor = false,
    };

    return composite;
}

ast_poly_composite_t *ast_add_poly_composite(
    ast_t *ast,
    strong_cstr_t name,
    ast_layout_t layout,
    source_t source,
    ast_type_t maybe_parent,
    bool is_class,
    strong_cstr_t *generics,
    length_t generics_length
){
    expand((void**) &ast->poly_composites, sizeof(ast_poly_composite_t), ast->poly_composites_length, &ast->poly_composites_capacity, 1, 4);

    ast_poly_composite_t *poly_composite = &ast->poly_composites[ast->poly_composites_length++];

    *poly_composite = (ast_poly_composite_t){
        .name = name,
        .layout = layout,
        .source = source,
        .parent = maybe_parent,
        .is_polymorphic = true,
        .is_class = is_class,
        .has_constructor = false,
        .generics = generics,
        .generics_length = generics_length,
    };

    return poly_composite;
}

void ast_add_global(ast_t *ast, strong_cstr_t name, ast_type_t type, ast_expr_t *initial_value, trait_t traits, source_t source){
    expand((void**) &ast->globals, sizeof(ast_global_t), ast->globals_length, &ast->globals_capacity, 1, 8);

    ast_global_t *global = &ast->globals[ast->globals_length++];

    *global = (ast_global_t){
        .name = name,
        .name_length = strlen(name),
        .type = type,
        .initial = initial_value,
        .traits = traits,
        .source = source,
    };
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
        strong_cstr_t names[1] = {
            strclone("_opaque"),
        };

        ast_type_t types[1] = {
            ast_type_make_base(strclone("ptr")),
        };

        ast_layout_init_with_struct_fields(&layout, names, types, NUM_ITEMS(names));
        ast_add_composite(ast, strclone("va_list"), layout, NULL_SOURCE, AST_TYPE_NONE, false);
    } else {
        // Larger Intel x86_64 va_list

        // Don't show va_list size warning if we are cross compiling.
        // When cross compiling, we'll make the conservative guess that 'va_list' will
        // be 24 bytes or smaller
        if(compiler->cross_compile_for == CROSS_COMPILE_NONE && sizeof(va_list) != 24){
            // Neglect whether to terminate, since this is not a fixable warning
            compiler_warnf(compiler, NULL_SOURCE, "Assuming Intel x86_64 va_list\n");
        }

        strong_cstr_t names[4] = {
            strclone("_opaque1"),
            strclone("_opaque2"),
            strclone("_opaque3"),
            strclone("_opaque4"),
        };

        ast_type_t types[4] = {
            ast_type_make_base(strclone("int")),
            ast_type_make_base(strclone("int")),
            ast_type_make_base(strclone("ptr")),
            ast_type_make_base(strclone("ptr")),
        };
        
        ast_layout_init_with_struct_fields(&layout, names, types, NUM_ITEMS(names));
        ast_add_composite(ast, strclone("va_list"), layout, NULL_SOURCE, AST_TYPE_NONE, false);
    }
}

int ast_aliases_cmp(const void *a, const void *b){
    return strcmp(((ast_alias_t*) a)->name, ((ast_alias_t*) b)->name);
}

int ast_enums_cmp(const void *a, const void *b){
    return strcmp(((ast_enum_t*) a)->name, ((ast_enum_t*) b)->name);
}

int ast_poly_funcs_cmp(const void *a, const void *b){
    int diff = strcmp(((ast_poly_func_t*) a)->name, ((ast_poly_func_t*) b)->name);
    if(diff != 0) return diff;
    return (int) ((ast_poly_func_t*) a)->ast_func_id - (int) ((ast_poly_func_t*) b)->ast_func_id;
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
