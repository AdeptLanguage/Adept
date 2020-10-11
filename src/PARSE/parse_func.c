
#include "UTIL/util.h"
#include "UTIL/search.h"
#include "PARSE/parse.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_func.h"
#include "PARSE/parse_stmt.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"

errorcode_t parse_func(parse_ctx_t *ctx){
    ast_t *ast = ctx->ast;
    source_t source = ctx->tokenlist->sources[*ctx->i];
    token_t *tokens = ctx->tokenlist->tokens;

    if(tokens[*ctx->i + 1].id == TOKEN_ALIAS && tokens[*ctx->i].id == TOKEN_FUNC){
        // Parse function alias instead of regular function
        return parse_func_alias(ctx);
    }

    strong_cstr_t name;
    maybe_null_strong_cstr_t export_name; 
    bool is_stdcall, is_foreign, is_verbatim, is_implicit;
    
    if(parse_func_head(ctx, &name, &is_stdcall, &is_foreign, &is_verbatim, &is_implicit, &export_name)) return FAILURE;

    if(is_foreign && ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare foreign function within struct domain");
        return FAILURE;
    }

    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    bool is_entry = strcmp(name, ctx->compiler->entry_point) == 0;
    ast_func_create_template(func, name, is_stdcall, is_foreign, is_verbatim, is_implicit, source, is_entry, export_name);

    if(ctx->next_builtin_traits != TRAIT_NONE){
        func->traits |= ctx->next_builtin_traits;
        ctx->next_builtin_traits = TRAIT_NONE;
    }
    
    if(parse_func_arguments(ctx, func)) return FAILURE;
    if(parse_ignore_newlines(ctx, "Expected '{' after function head")) return FAILURE;

    tokenid_t beginning_token_id = tokens[*ctx->i].id;

    if(!is_foreign && (beginning_token_id == TOKEN_BEGIN || beginning_token_id == TOKEN_ASSIGN)){
        ast_type_make_base(&func->return_type, strclone("void"));
    } else {
        if(parse_type(ctx, &func->return_type)){
            func->return_type.elements = NULL;
            func->return_type.elements_length = 0;
            func->return_type.source = NULL_SOURCE;
            return FAILURE;
        }
    }
    
    // enforce specific arguments for special functions & methods
    if(func->traits == AST_FUNC_DEFER && (
        !ast_type_is_void(&func->return_type)
        || func->arity != 1
        || strcmp(func->arg_names[0], "this") != 0
        || !(  ast_type_is_base_ptr(&func->arg_types[0])
            || ast_type_is_polymorph_ptr(&func->arg_types[0])
            || ast_type_is_generic_base_ptr(&func->arg_types[0])
            )
        || func->arg_type_traits[0] != TRAIT_NONE
    )){
        compiler_panic(ctx->compiler, source, "Management method __defer__ must be declared as 'func __defer__(this *T) void'");
        return FAILURE;
    }

    if(func->traits == AST_FUNC_PASS && (
           !(  ast_type_is_base(&func->return_type)
            || ast_type_is_polymorph(&func->return_type)
            || ast_type_is_generic_base(&func->return_type)
            || ast_type_is_fixed_array(&func->return_type)
            )
        || func->arity != 1
        || !ast_types_identical(&func->return_type, &func->arg_types[0])
        || func->arg_type_traits[0] != AST_FUNC_ARG_TYPE_TRAIT_POD
    )){
        compiler_panic(ctx->compiler, source, "Management function __pass__ must be declared as 'func __pass__(value POD T) T'");
        return FAILURE;
    }

    if(strcmp(func->name, "__assign__") == 0 && (
        func->traits != TRAIT_NONE
        || !ast_type_is_void(&func->return_type)
        || func->arity != 2
        || strcmp(func->arg_names[0], "this") != 0
        || !(  ast_type_is_base_ptr(&func->arg_types[0])
            || ast_type_is_polymorph_ptr(&func->arg_types[0])
            || ast_type_is_generic_base_ptr(&func->arg_types[0])
            )
        || !ast_type_is_pointer_to(&func->arg_types[0], &func->arg_types[1])
        || func->arg_type_traits[0] != TRAIT_NONE
    )){
        compiler_panic(ctx->compiler, source, "Management method __assign__ must be declared like 'func __assign__(this *T, other T) void'");
        return FAILURE;
    }

    if(strcmp(func->name, "__access__") == 0 && (
        func->traits != TRAIT_NONE
        || func->arity != 2
        || !ast_type_is_pointer(&func->arg_types[0])
        || !ast_type_is_pointer(&func->return_type)
        || strcmp(func->arg_names[0], "this") != 0
        || func->arg_type_traits[0] != TRAIT_NONE
    )){
        compiler_panic(ctx->compiler, source, "Management method __access__ must be declared like '__access__(this *T, index $Key) *$Value'");
        return FAILURE;
    }

    if(strcmp(func->name, "__variadic_array__") == 0){
        if(ctx->ast->common.ast_variadic_array != NULL){
            compiler_panic(ctx->compiler, source, "The function __variadic_array__ can only be defined once");
            compiler_panic(ctx->compiler, ctx->ast->common.ast_variadic_source, "Previous definition");
            return FAILURE;
        }

        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, source, "The function __variadic_array__ must return a value");
            return FAILURE;
        }

        if(func->traits != TRAIT_NONE
        || func->arity != 4
        || !ast_type_is_base_of(&func->arg_types[0], "ptr")
        || !ast_type_is_base_of(&func->arg_types[1], "usize")
        || !ast_type_is_base_of(&func->arg_types[2], "usize")
        || !ast_type_is_base_of(&func->arg_types[3], "ptr")
        || func->arg_type_traits[0] != TRAIT_NONE
        || func->arg_type_traits[1] != TRAIT_NONE
        || func->arg_type_traits[2] != TRAIT_NONE
        || func->arg_type_traits[3] != TRAIT_NONE
        ){
            compiler_panic(ctx->compiler, source, "Special function __variadic_array__ must be declared like:\n'__variadic_array__(pointer ptr, bytes usize, length usize, maybe_types ptr) ReturnType'");
            return FAILURE;
        }

        ctx->ast->common.ast_variadic_array = malloc(sizeof(ast_type_t));
        *ctx->ast->common.ast_variadic_array = ast_type_clone(&func->return_type);
        ctx->ast->common.ast_variadic_source = func->source;
    }

    if(strcmp(func->name, "__initializer_list__") == 0){
        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, source, "The function __initializer_list__ must return a value");
            return FAILURE;
        }

        if(func->traits != TRAIT_NONE
        || func->arity != 2
        || !ast_type_is_base_of(&func->arg_types[1], "usize")
        || func->arg_type_traits[0] != TRAIT_NONE
        || func->arg_type_traits[1] != TRAIT_NONE
        ){
            compiler_panic(ctx->compiler, source, "Special function __initializer_list__ must be declared like:\n'__initializer_list__(array *$T, length usize) <$T> ReturnType'");
            return FAILURE;
        }

        ctx->ast->common.ast_initializer_list = malloc(sizeof(ast_type_t));
        *ctx->ast->common.ast_initializer_list = ast_type_clone(&func->return_type);
        ctx->ast->common.ast_initializer_list_source = func->source;
    }
    
    static const char *math_management_funcs[] = {
        "__add__", "__divide__", "__equals__", "__greater_than__",
        "__greater_than_or_equal__", "__less_than__", "__less_than_or_equal__",
        "__modulus__", "__multiply__", "__not_equals__", "__subtract__"
    };
    static const length_t math_management_funcs_length = sizeof(math_management_funcs) / sizeof(const char*);
    maybe_index_t math_func_index = binary_string_search(math_management_funcs, math_management_funcs_length, func->name);
    
    if(math_func_index != -1){
        // Enforce math management function prototype
        // NOTE: The type that's returned is up to the user
        // NOTE: The first argument cannot be a pointer
        if(func->arity != 2){
            compiler_panicf(ctx->compiler, source, "Management method %s must take two arguments", func->name);
            return FAILURE;
        }

        if(ast_type_is_pointer(&func->arg_types[0])){
            compiler_panicf(ctx->compiler, source, "Management method %s cannot have a pointer as the first argument", func->name);
            return FAILURE;
        }
    }

    if(ast_func_is_polymorphic(func)){
        // Ensure this isn't a foreign function
        if(is_foreign){
            compiler_panic(ctx->compiler, source, "Cannot declare polymorphic foreign functions");
            return FAILURE;
        }

        // Remember the function as polymorphic
        func->traits |= AST_FUNC_POLYMORPHIC;
        expand((void**) &ast->polymorphic_funcs, sizeof(ast_polymorphic_func_t), ast->polymorphic_funcs_length, &ast->polymorphic_funcs_capacity, 1, 4);

        ast_polymorphic_func_t *poly_func = &ast->polymorphic_funcs[ast->polymorphic_funcs_length++];
        poly_func->name = func->name;
        poly_func->ast_func_id = ast_func_id;
        poly_func->is_beginning_of_group = -1; // Uncalculated

        if(func->arity != 0 && strcmp(func->arg_names[0], "this") == 0){
            expand((void**) &ast->polymorphic_methods, sizeof(ast_polymorphic_func_t), ast->polymorphic_methods_length, &ast->polymorphic_methods_capacity, 1, 4);
            ast_polymorphic_func_t *poly_method = &ast->polymorphic_methods[ast->polymorphic_methods_length++];
            poly_method->name = func->name;
            poly_method->ast_func_id = ast_func_id;
            poly_method->is_beginning_of_group = -1; // Uncalculated
        }
    }

    if(parse_func_body(ctx, func)) return FAILURE;
    return SUCCESS;
}

errorcode_t parse_func_head(parse_ctx_t *ctx, strong_cstr_t *out_name, bool *out_is_stdcall,
        bool *out_is_foreign, bool *out_is_verbatim, bool *out_is_implicit, maybe_null_strong_cstr_t *out_export_name){
    
    bool is_external;
    parse_func_prefixes(ctx, out_is_stdcall, out_is_verbatim, out_is_implicit, &is_external);

    tokenid_t id = ctx->tokenlist->tokens[(*ctx->i)++].id;
    *out_is_foreign = (id == TOKEN_FOREIGN);

    if(*out_is_foreign || id == TOKEN_FUNC){
        *out_export_name = parse_eat_string(ctx, NULL);
        
        if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
            *out_name = ctx->prename;
            ctx->prename = NULL;
        } else if(*out_is_foreign){
            *out_name = parse_take_word(ctx, "Expected function name after 'foreign' keyword");
        } else {
            *out_name = parse_take_word(ctx, "Expected function name after 'func' keyword");
        }

        if(*out_name == NULL) return FAILURE;

        // Form export name if applicable
        if(*out_export_name){
            *out_export_name = strclone(*out_export_name);
        } else if(is_external){
            *out_export_name = strclone(*out_name);
        }

        // If not in struct association, then prepend current namespace
        if(ctx->struct_association == NULL) parse_prepend_namespace(ctx, out_name);
        return SUCCESS;
    }

    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i - 1], "Expected 'func' or 'foreign' keyword after 'stdcall' keyword");
    return FAILURE;
}

errorcode_t parse_func_body(parse_ctx_t *ctx, ast_func_t *func){
    if(func->traits & AST_FUNC_FOREIGN) return SUCCESS;
    if(parse_ignore_newlines(ctx, "Expected function body")) return FAILURE;

    ast_expr_list_t stmts;
    defer_scope_t defer_scope;
    defer_scope_init(&defer_scope, NULL, NULL, TRAIT_NONE);

    if(ctx->tokenlist->tokens[*ctx->i].id == TOKEN_ASSIGN){
        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Cannot return 'void' from single line function");
            return FAILURE;
        }

        (*ctx->i)++;
        ctx->func = func;
        
        if(parse_ignore_newlines(ctx, "Expected function body")) return FAILURE;
        ast_expr_list_init(&stmts, 1);

        ast_expr_t *return_expression;
        if(parse_expr(ctx, &return_expression)){
            ast_free_statements_fully(stmts.statements, stmts.length);
            return FAILURE;
        }

        ast_expr_return_t *stmt = malloc(sizeof(ast_expr_return_t));
        stmt->id = EXPR_RETURN;
        stmt->source = return_expression->source;
        stmt->value = return_expression;
        stmt->last_minute.statements = NULL;
        stmt->last_minute.length = 0;
        stmt->last_minute.capacity = 0;
        stmts.statements[stmts.length++] = (ast_expr_t*) stmt;

        func->statements = stmts.statements;
        func->statements_length = stmts.length;
        func->statements_capacity = stmts.capacity;
        return SUCCESS;
    }

    if(parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after function prototype")) return FAILURE;

    ast_expr_list_init(&stmts, 16);
    ctx->func = func;

    if(parse_stmts(ctx, &stmts, &defer_scope, PARSE_STMTS_STANDARD)){
        ast_free_statements_fully(stmts.statements, stmts.length);
        defer_scope_free(&defer_scope);
        return FAILURE;
    }

    defer_scope_free(&defer_scope);
    
    func->statements = stmts.statements;
    func->statements_length = stmts.length;
    func->statements_capacity = stmts.capacity;
    return SUCCESS;
}

errorcode_t parse_func_arguments(parse_ctx_t *ctx, ast_func_t *func){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    bool is_solid;
    length_t backfill = 0;
    length_t capacity = 0;
    func->variadic_arg_name = NULL;

    if(parse_ignore_newlines(ctx, "Expected '(' after function name")) return FAILURE;

    if(ctx->struct_association){
        if(func->traits & AST_FUNC_FOREIGN){
            compiler_panic(ctx->compiler, func->source, "Cannot declare foreign function inside of struct domain");
            return FAILURE;
        }

        parse_func_grow_arguments(func, backfill, &capacity);

        if(ctx->struct_association_is_polymorphic){
            // Insert 'this *<$A, $B, $C, ...> AssociatedStruct' as first argument to function

            ast_elem_pointer_t *pointer = malloc(sizeof(ast_elem_pointer_t));
            pointer->id = AST_ELEM_POINTER;
            pointer->source = NULL_SOURCE;

            ast_elem_generic_base_t *generic_base = malloc(sizeof(ast_elem_generic_base_t));
            generic_base->id = AST_ELEM_GENERIC_BASE;
            generic_base->source = NULL_SOURCE;
            generic_base->name = strclone(ctx->struct_association->name);
            generic_base->generics = malloc(sizeof(ast_type_t) * ctx->struct_association->generics_length);

            for(length_t i = 0; i != ctx->struct_association->generics_length; i++){
                ast_type_make_polymorph(&generic_base->generics[i], strclone(ctx->struct_association->generics[i]), false);
            }

            generic_base->generics_length = ctx->struct_association->generics_length;
            generic_base->name_is_polymorphic = false;
    
            func->arg_types[0].elements = malloc(sizeof(ast_elem_t*) * 2);
            func->arg_types[0].elements[0] = (ast_elem_t*) pointer;
            func->arg_types[0].elements[1] = (ast_elem_t*) generic_base;
            func->arg_types[0].elements_length = 2;
            func->arg_types[0].source = NULL_SOURCE;
        } else {
            // Insert 'this *AssociatedStruct' as first argument to function
            ast_type_make_base_ptr(&func->arg_types[0], strclone(ctx->struct_association->name));
        }

        func->arg_names[0] = strclone("this");
        func->arg_sources[0] = ctx->struct_association->source;
        func->arg_flows[0] = FLOW_IN;
        func->arg_type_traits[0] = TRAIT_NONE;
        func->arity++;
    }
    
    // Allow for no argument list
    if(tokens[*i].id != TOKEN_OPEN) return SUCCESS;
    (*i)++; // Eat '('

    // Allow polymorphic prerequisites for function arguments
    ctx->allow_polymorphic_prereqs = true;

    while(tokens[*i].id != TOKEN_CLOSE){
        if(parse_ignore_newlines(ctx, "Expected function argument")){
            parse_free_unbackfilled_arguments(func, backfill);
            ctx->allow_polymorphic_prereqs = false;
            return FAILURE;
        }

        parse_func_grow_arguments(func, backfill, &capacity);

        if(parse_func_argument(ctx, func, capacity, &backfill, &is_solid)){
            ctx->allow_polymorphic_prereqs = false;
            return FAILURE;
        }
        
        if(!is_solid) continue;

        bool takes_variable_arity = func->traits & AST_FUNC_VARARG || func->traits & AST_FUNC_VARIADIC;

        if(tokens[*i].id == TOKEN_NEXT && !takes_variable_arity){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
                parse_free_unbackfilled_arguments(func, backfill);
                ctx->allow_polymorphic_prereqs = false;
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            const char *error_message = takes_variable_arity
                    ? "Expected ')' after variadic argument"
                    : "Expected ',' after argument type";
            compiler_panic(ctx->compiler, sources[*i], error_message);
            parse_free_unbackfilled_arguments(func, backfill);
            ctx->allow_polymorphic_prereqs = false;
            return FAILURE;
        }
    }

    // Stop allowing polymorphic prerequisites
    ctx->allow_polymorphic_prereqs = false;
    
    if(backfill != 0){
        compiler_panic(ctx->compiler, sources[*i], "Expected argument type before end of argument list");
        parse_free_unbackfilled_arguments(func, backfill);
        return FAILURE;
    }

    (*i)++; // skip over ')'
    return SUCCESS;
}

errorcode_t parse_func_argument(parse_ctx_t *ctx, ast_func_t *func, length_t capacity, length_t *backfill, bool *out_is_solid){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    switch(tokens[*i].id){
    case TOKEN_IN:    func->arg_flows[func->arity + *backfill] = FLOW_IN;    (*i)++; break;
    case TOKEN_OUT:   func->arg_flows[func->arity + *backfill] = FLOW_OUT;   (*i)++; break;
    case TOKEN_INOUT: func->arg_flows[func->arity + *backfill] = FLOW_INOUT; (*i)++; break;
    default:          func->arg_flows[func->arity + *backfill] = FLOW_IN;    break;
    }

    func->arg_sources[func->arity + *backfill] = sources[*i];

    if(func->arg_defaults)
        func->arg_defaults[func->arity + *backfill] = NULL;

    if(tokens[*i].id == TOKEN_ELLIPSIS){
        // Alone ellipsis, used for c-style varargs

        if(*backfill != 0){
            compiler_panic(ctx->compiler, sources[*i], "Expected type for previous arguments before ellipsis");
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        (*i)++;
        func->traits |= AST_FUNC_VARARG;
        *out_is_solid = false;
        return SUCCESS;
    }

    if(!(func->traits & AST_FUNC_FOREIGN)){
        char *name = parse_take_word(ctx, "Expected argument name before argument type");

        if(name == NULL){
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        func->arg_names[func->arity + *backfill] = name;
    }

    if(tokens[*i].id == TOKEN_ELLIPSIS){
        // Ellipsis as type, used for modern variadic argument

        if(*backfill != 0){
            compiler_panic(ctx->compiler, sources[*i], "Expected type for previous arguments before ellipsis");
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        (*i)++;
        func->traits |= AST_FUNC_VARIADIC;
        *out_is_solid = false;

        // Take variadic name from last unused argument name
        func->variadic_arg_name = func->arg_names[func->arity + *backfill];
        func->arg_names[func->arity + *backfill] = NULL;

        // Assign variadic source
        func->variadic_source = sources[*i - 2];
        return SUCCESS;
    }

    if(
        parse_ignore_newlines(ctx, "Expected type") ||
        (tokens[*i].id == TOKEN_ASSIGN && parse_func_default_arg_value(ctx, func, capacity, backfill)) ||
        parse_ignore_newlines(ctx, "Expected type")
    ){
        if(func->arg_names) free(func->arg_names[func->arity + *backfill]);
        parse_free_unbackfilled_arguments(func, *backfill);
        return FAILURE;
    }

    if(!(func->traits & AST_FUNC_FOREIGN) && tokens[*i].id == TOKEN_NEXT){
        if(tokens[++(*i)].id == TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
            if(func->arg_names) free(func->arg_names[func->arity + *backfill]);
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        *backfill += 1;
        *out_is_solid = false;
        return SUCCESS;
    }
    
    if(tokens[*i].id == TOKEN_POD){
        func->arg_type_traits[func->arity + *backfill] = AST_FUNC_ARG_TYPE_TRAIT_POD;
        (*i)++;
    } else {
        func->arg_type_traits[func->arity + *backfill] = TRAIT_NONE;
    }

    if(
        parse_ignore_newlines(ctx, "Expected type") ||
        parse_type(ctx, &func->arg_types[func->arity + *backfill]) ||
        parse_ignore_newlines(ctx, "Expected type") ||
        (tokens[*i].id == TOKEN_ASSIGN && parse_func_default_arg_value(ctx, func, capacity, backfill))
    ){
        if(func->arg_names) free(func->arg_names[func->arity + *backfill]);
        parse_free_unbackfilled_arguments(func, *backfill);
        return FAILURE;
    }

    parse_func_backfill_arguments(func, backfill);
    func->arity++;
    *out_is_solid = true;

    return SUCCESS;
}

errorcode_t parse_func_default_arg_value(parse_ctx_t *ctx, ast_func_t *func, length_t capacity, length_t *backfill){
    // my_argument float = 0.0f
    //                   ^

    if(func->arg_defaults && func->arg_defaults[func->arity + *backfill]){
        compiler_panic(ctx->compiler, func->arg_sources[func->arity + *backfill], "Function argument already has default value");
        return FAILURE;
    }

    // Skip over '=' token
    if(parse_eat(ctx, TOKEN_ASSIGN, "INTERNAL ERROR: parse_func_default_arg_value() expected '=' token")) return FAILURE;

    // Create default argument array if it doesn't already exist
    if(func->arg_defaults == NULL){
        func->arg_defaults = malloc(sizeof(ast_expr_t*) * capacity);

        // NOTE: Previous arguments must have their default argument set to nothing
        for(length_t i = 0; i != func->arity + *backfill; i++){
            func->arg_defaults[i] = NULL;
        }
    }

    if(parse_expr(ctx, &func->arg_defaults[func->arity + *backfill])) return FAILURE;
    return SUCCESS;
}

void parse_func_backfill_arguments(ast_func_t *func, length_t *backfill){
    length_t master_arg_index = func->arity + *backfill;
    ast_type_t *master_type = &func->arg_types[master_arg_index];
    trait_t master_type_trait = func->arg_type_traits[master_arg_index];
    ast_expr_t *master_default = func->arg_defaults == NULL ? NULL : func->arg_defaults[master_arg_index];

    bool backfill_default_values = true;

    for(length_t i = 0; *backfill != 0; i++){
        length_t arg_index = func->arity + *backfill - i - 1;
        func->arg_types[arg_index] = ast_type_clone(master_type);
        func->arg_type_traits[arg_index] = master_type_trait;

        // Backfill for default values only until an argument has a default value set
        if(backfill_default_values){
            if(master_default && func->arg_defaults[arg_index] == NULL){
                func->arg_defaults[arg_index] = ast_expr_clone(master_default);
            } else {
                backfill_default_values = false;
            }
        }

        func->arity++;
        *backfill -= 1;
    }
}

void parse_func_grow_arguments(ast_func_t *func, length_t backfill, length_t *capacity){
    if(*capacity == 0){
        if(!(func->traits & AST_FUNC_FOREIGN)){
            func->arg_names = malloc(sizeof(char*) * 4);
        }

        func->arg_types   = malloc(sizeof(ast_type_t) * 4);
        func->arg_sources = malloc(sizeof(source_t) * 4);
        func->arg_flows   = malloc(sizeof(char) * 4);
        func->arg_type_traits = malloc(sizeof(trait_t) * 4);
        *capacity = 4;
        return;
    }

    if(func->arity + backfill != *capacity) return;
    *capacity *= 2;

    if(!(func->traits & AST_FUNC_FOREIGN)){
        grow((void**) &func->arg_names, sizeof(char*), func->arity + backfill, *capacity);
    }

    grow((void**) &func->arg_types,       sizeof(ast_type_t), func->arity, *capacity);
    grow((void**) &func->arg_sources,     sizeof(source_t),   func->arity + backfill, *capacity);
    grow((void**) &func->arg_flows,       sizeof(char),       func->arity + backfill, *capacity);
    grow((void**) &func->arg_type_traits, sizeof(trait_t),    func->arity + backfill, *capacity);

    if(func->arg_defaults)
        grow((void**) &func->arg_defaults, sizeof(ast_expr_t*), func->arity + backfill, *capacity);
}

void parse_func_prefixes(parse_ctx_t *ctx, bool *out_is_stdcall, bool *out_is_verbatim, bool *out_is_implicit, bool *out_is_external){
    tokenid_t token_id = ctx->tokenlist->tokens[*ctx->i].id;

    *out_is_stdcall = false;
    *out_is_verbatim = false;
    *out_is_implicit = false;
    *out_is_external = false;

    // NOTE: Duplicates are allowed, maybe they shouldn't be or does it really matter?
    while(true){
        switch(token_id){
        case TOKEN_STDCALL:
            token_id = ctx->tokenlist->tokens[++(*ctx->i)].id;
            *out_is_stdcall = true;
            continue;
        case TOKEN_VERBATIM:
            token_id = ctx->tokenlist->tokens[++(*ctx->i)].id;
            *out_is_verbatim = true;
            continue;
        case TOKEN_IMPLICIT:
            token_id = ctx->tokenlist->tokens[++(*ctx->i)].id;
            *out_is_implicit = true;
            continue;
        case TOKEN_EXTERNAL:
            token_id = ctx->tokenlist->tokens[++(*ctx->i)].id;
            *out_is_external = true;
            continue;
        }

        return;
    }

    return;
}

void parse_free_unbackfilled_arguments(ast_func_t *func, length_t backfill){
    for(length_t i = 0; i != backfill; i++){
        free(func->arg_names[func->arity + backfill - i - 1]);
        
        if(func->arg_defaults)
            ast_expr_free_fully(func->arg_defaults[func->arity + backfill - i - 1]);
    }
}

errorcode_t parse_func_alias(parse_ctx_t *ctx){
    // func alias myAlias(...) => otherFunction
    //  ^

    ast_t *ast = ctx->ast;
    source_t source = ctx->tokenlist->sources[(*ctx->i)++];

    // Eat 'alias' keyword
    if(parse_eat(ctx, TOKEN_ALIAS, "Expected 'alias' keyword for function alias")) return FAILURE;

    // Get from alias name
    weak_cstr_t from;

    if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        from = ctx->prename;
        ctx->prename = NULL;
    } else {
        from = parse_eat_word(ctx, "Expected function alias name");
        if(from == NULL) return FAILURE;
    }

    ast_type_t *arg_types;
    length_t arity;
    trait_t required_traits;    
    bool match_first_of_name;
    if(parse_func_alias_args(ctx, &arg_types, &arity, &required_traits, &match_first_of_name)) return FAILURE;

    // Eat '=>'
    if(parse_eat(ctx, TOKEN_STRONG_ARROW, "Expected '=>' after argument types for function alias")){
        ast_types_free_fully(arg_types, arity);
        return FAILURE;
    }

    weak_cstr_t to = parse_eat_word(ctx, "Expected function alias destination name");
    if(to == NULL){
        ast_types_free_fully(arg_types, arity);
        return FAILURE;
    }

    expand((void**) &ast->func_aliases, sizeof(ast_func_alias_t), ast->func_aliases_length, &ast->func_aliases_capacity, 1, 8);

    ast_func_alias_t *falias = &ast->func_aliases[ast->func_aliases_length++];
    falias->from = from;
    falias->to = to;
    falias->arg_types = arg_types;
    falias->arity = arity;
    falias->required_traits = required_traits;
    falias->source = source;
    falias->match_first_of_name = match_first_of_name;
    return SUCCESS;
}

errorcode_t parse_func_alias_args(parse_ctx_t *ctx, ast_type_t **out_arg_types, length_t *out_arity, trait_t *out_required_traits, bool *out_match_first_of_name){
    // func alias myAlias(...) => otherFunction
    //                   ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    length_t args_capacity = 0;

    *out_required_traits = TRAIT_NONE;
    *out_arity = 0;
    *out_arg_types = NULL;
    *out_match_first_of_name = tokens[*i].id != TOKEN_OPEN;

    // Don't parse argument types if we're going to match the first of the same name
    if(*out_match_first_of_name) return SUCCESS;

    // Eat '('
    if(parse_eat(ctx, TOKEN_OPEN, "Expected '(' after function alias name")) return FAILURE;

    while(tokens[*i].id != TOKEN_CLOSE){
        expand((void**) out_arg_types, sizeof(ast_type_t), *out_arity, &args_capacity, 1, 4);

        if(parse_ignore_newlines(ctx, "Expected argument type for function alias")) goto failure;

        if(tokens[*i].id == TOKEN_ELLIPSIS){
            // '...'
            *out_required_traits |= AST_FUNC_VARARG;
            (*i)++;
        } else if(tokens[*i].id == TOKEN_RANGE){
            // '..'
            *out_required_traits |= AST_FUNC_VARIADIC;
            (*i)++;
        } else {
            // Type
            if(parse_type(ctx, &(*out_arg_types)[*out_arity])) goto failure;

            // Increase arity
            (*out_arity)++;
        }

        if(parse_ignore_newlines(ctx, "Expected argument type for function alias")) goto failure;

        if(tokens[*i].id == TOKEN_NEXT){
            if(*out_required_traits & AST_FUNC_VARARG || *out_required_traits & AST_FUNC_VARIADIC){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ')' after variadic argument");
                goto failure;
            }

            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected type after ',' in argument types");
                goto failure;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            bool takes_variable_arity = *out_required_traits & AST_FUNC_VARIADIC || *out_required_traits & AST_FUNC_VARARG;

            const char *error_message = takes_variable_arity
                    ? "Expected ')' after variadic argument"
                    : "Expected ',' after argument type";
            
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], error_message);
            goto failure;
        }
    }

    // Eat ')'
    if(parse_eat(ctx, TOKEN_CLOSE, "Expected ')' after function alias argument types")) goto failure;
    return SUCCESS;

failure:
    ast_types_free_fully(*out_arg_types, *out_arity);
    return FAILURE;
}
