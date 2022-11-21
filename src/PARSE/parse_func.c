
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "LEX/token.h"
#include "PARSE/parse_checks.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_expr.h"
#include "PARSE/parse_func.h"
#include "PARSE/parse_stmt.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

static errorcode_t add_dispatcher(parse_ctx_t *ctx, func_id_t virtual_origin){
    // This function is responsible for creating dispatcher functions that will
    // handle the actual virtual dispatch for virtual methods.
    // The default/overrides of these will not be accessible with normal calls,
    // and will only be accessible when we look for them during vtable generation
    // (via `ir_gen_find_dispatchee`)
    // The dispatchers will be the user-facing function that actually gets called
    // by the programmer
    // In code, they would look something like this:
    //
    // func dispatcher(this *$This extends SubjectType, arg1 ArgType1, argn ArgTypeN) ReturnType {
    //     return (*(this.__vtable__ as **ptr))[secret_vtable_entry_index](arg1, argn)
    // }
    //
    // Old-style vararg virtual methods are forbidden (for now at least).

    ast_t *ast = ctx->ast;
    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];
    ast_func_t *virtual = &ast->funcs[virtual_origin];

    if(virtual->traits & AST_FUNC_VARARG){
        compiler_panicf(ctx->compiler, virtual->source, "Virtual dispatcher cannot use old-style variadic arguments");
        return FAILURE;
    }

    // Hook up link from virtual func to virtual dispatcher
    virtual->virtual_dispatcher = ast_func_id;

    length_t arity = virtual->arity;

    ast_func_head_t func_head = (ast_func_head_t){
        .name = strclone(virtual->name),
        .source = virtual->source,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = (ast_func_prefixes_t){0},
        .export_name = NULL,
    };

    ast_func_create_template(func, &func_head);

    func->virtual_origin = virtual_origin;
    func->traits |= AST_FUNC_DISPATCHER | AST_FUNC_GENERATED | AST_FUNC_POLYMORPHIC;
    func->arity = arity;

    func->arg_names = strsclone(virtual->arg_names, arity);
    func->arg_types = ast_types_clone(virtual->arg_types, arity);
    func->arg_sources = memclone(virtual->arg_sources, sizeof(source_t) * arity);
    func->arg_flows = memclone(virtual->arg_flows, sizeof(char) * arity);
    func->arg_type_traits = memclone(virtual->arg_type_traits, sizeof(trait_t) * arity);
    func->return_type = ast_type_clone(&virtual->return_type);

    // Ensure subject type is at the very least a pointer to a base or generic base
    if(!ast_type_is_pointer_to_base_like(&func->arg_types[0])){
        strong_cstr_t typename = ast_type_str(&func->arg_types[0]);
        compiler_panicf(ctx->compiler, func->arg_types[0].source, "Cannot define virtual methods for non-class type '%s'", typename);
        free(typename);
        return FAILURE;
    }

    // Change subject type to be a polymorphic parameter that requires a match to extend the original subject class
    // e.g. from
    //   `*Shape`
    // to
    //   `*$This extends Shape`

    ast_type_dereference(&func->arg_types[0]);
    func->arg_types[0] = ast_type_pointer_to(ast_type_make_polymorph_prereq(strclone("This"), false, NULL, func->arg_types[0]));

    // Register as polymorphic function
    ast_add_poly_func(ast, func->name, ast_func_id);
    return SUCCESS;
}

errorcode_t parse_func(parse_ctx_t *ctx){
    ast_t *ast = ctx->ast;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t source = parse_ctx_peek_source(ctx);

    if(tokens[*ctx->i + 1].id == TOKEN_ALIAS && tokens[*ctx->i].id == TOKEN_FUNC){
        // Parse function alias instead of regular function
        return parse_func_alias(ctx);
    }

    if(ctx->ast->funcs_length >= MAX_FUNC_ID){
        compiler_panic(ctx->compiler, source, "Maximum number of AST functions reached\n");
        return FAILURE;
    }

    ast_func_head_t func_head;
    ast_func_head_parse_info_t func_head_parse_info;
    if(parse_func_head(ctx, &func_head, &func_head_parse_info)) return FAILURE;

    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];

    ast_func_create_template(func, &func_head);

    if(func_head.is_foreign && ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare foreign function within struct domain");
        return FAILURE;
    }

    if(ctx->next_builtin_traits != TRAIT_NONE){
        func->traits |= ctx->next_builtin_traits;
        ctx->next_builtin_traits = TRAIT_NONE;
    }
    
    if(parse_func_arguments(ctx, func)) return FAILURE;
    if(parse_ignore_newlines(ctx, "Expected '{' after function head")) return FAILURE;

    if(parse_eat(ctx, TOKEN_EXHAUSTIVE, NULL) == SUCCESS){
        func->traits |= AST_FUNC_NO_DISCARD;
    }

    tokenid_t beginning_token_id = tokens[*ctx->i].id;

    if(!func_head.is_foreign && (beginning_token_id == TOKEN_BEGIN || beginning_token_id == TOKEN_ASSIGN)){
        func->return_type = ast_type_make_base(strclone("void"));
    } else {
        if(parse_type(ctx, &func->return_type)){
            func->return_type = (ast_type_t){0};
            return FAILURE;
        }
    }

    // TODO: CLEANUP: This is a little ugly
    if(ctx->tokenlist->tokens[*ctx->i].id == TOKEN_ASSIGN
    && ctx->tokenlist->tokens[*ctx->i + 1].id == TOKEN_DELETE){
        func->traits |= AST_FUNC_DISALLOW;
        *(ctx->i) += 2;
    }

    if(func_head_parse_info.is_constructor){
        assert(ctx->composite_association);
        
        // Remember that the associated composite has a constructor
        ctx->composite_association->has_constructor = true;

        // Remember if this function is a constructor of a class
        if(ctx->composite_association->is_class){
            func->traits |= AST_FUNC_CLASS_CONSTRUCTOR;
        }
    }

    if(validate_func_requirements(ctx, func, source)){
        return FAILURE;
    }

    if(ast_func_has_polymorphic_signature(func)){
        // Ensure this isn't a foreign function
        if(func_head.is_foreign){
            compiler_panic(ctx->compiler, source, "Cannot declare foreign functions as polymorphic");
            return FAILURE;
        }

        // Remember the function as polymorphic
        func->traits |= AST_FUNC_POLYMORPHIC;
        ast_add_poly_func(ast, func->name, ast_func_id);

        if(func->arity != 0 && streq(func->arg_names[0], "this")){
            expand((void**) &ast->polymorphic_methods, sizeof(ast_poly_func_t), ast->polymorphic_methods_length, &ast->polymorphic_methods_capacity, 1, 4);
            ast_poly_func_t *poly_method = &ast->polymorphic_methods[ast->polymorphic_methods_length++];
            poly_method->name = func->name;
            poly_method->ast_func_id = ast_func_id;
            poly_method->is_beginning_of_group = -1; // Uncalculated
        }
    }

    if(func->traits & AST_FUNC_VIRTUAL){
        if(!ast_func_is_method(func)){
            compiler_panicf(ctx->compiler, func->source, "Cannot mark non-method as 'virtual'");
            return FAILURE;
        }

        if(add_dispatcher(ctx, ast_func_id)) return FAILURE;
    }

    if(parse_func_body(ctx, func)) return FAILURE;

    if(func_head_parse_info.is_constructor && !func_head_parse_info.is_in_only_constructor){
        parse_func_solidify_constructor(ast, func, source);
    }

    return SUCCESS;
}

void parse_func_solidify_constructor(ast_t *ast, ast_func_t *constructor, source_t source){
    // Automatically create subject-less constructor for subject-ful constructor
    // e.g. `func Person(name POD String, age POD int) Person { ... }`
    // for  `struct Person (...) { constructor(name String, age int){ ... } }`

    const ast_type_t this_pointee_type_view = ast_type_unwrapped_view(&constructor->arg_types[0]);
    weak_cstr_t struct_name = ast_type_struct_name(&this_pointee_type_view);

    ast_func_head_t func_head = (ast_func_head_t){
        .name = strclone(struct_name),
        .source = source,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = (ast_func_prefixes_t){
            .is_stdcall = false,
            .is_verbatim = false,
            .is_implicit = false,
            .is_external = false,
            .is_virtual = false,
            .is_override = false,
        },
        .export_name = NULL
    };

    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];

    ast_func_create_template(func, &func_head);

    if(ast_func_has_polymorphic_signature(constructor)) {
        func->traits |= AST_FUNC_POLYMORPHIC;
        ast_add_poly_func(ast, func->name, ast_func_id);
    }

    length_t arity = constructor->arity - 1;

    func->arg_names = strsclone(&constructor->arg_names[1], arity);
    func->arg_types = ast_types_clone(&constructor->arg_types[1], arity);
    func->arg_sources = memclone(&constructor->arg_sources[1], sizeof(source_t) * arity);
    func->arg_flows = memclone(&constructor->arg_flows[1], sizeof(char) * arity);
    func->arg_type_traits = malloc(sizeof(trait_t) * arity);
    
    if(constructor->arg_defaults){
        func->arg_defaults = malloc(sizeof(ast_expr_t*) * arity);

        for(length_t i = 0; i != arity; i++){
            ast_expr_t *default_value = constructor->arg_defaults[1 + i];

            func->arg_defaults[i] = default_value ? ast_expr_clone(default_value) : NULL;
        }
    } else {
        func->arg_defaults = NULL;
    }

    func->arity = arity;
    func->return_type = ast_type_clone(&this_pointee_type_view);
    func->statements = (ast_expr_list_t){0};

    // Generate statements to call into subject-ful constructor
    // and then return the constructed value

    optional_ast_expr_list_t inputs = (optional_ast_expr_list_t){
        .has = true,
        .value = ast_expr_list_create(arity),
    };

    for(length_t i = 0; i != arity; i++){
        ast_expr_list_append_unchecked(&inputs.value, ast_expr_create_variable(func->arg_names[i], NULL_SOURCE));

        func->arg_type_traits[i] = AST_FUNC_ARG_TYPE_TRAIT_POD;
    }

    ast_expr_t *declare_and_construct_stmt = ast_expr_create_declaration(EXPR_DECLARE, source, "$", ast_type_clone(&this_pointee_type_view), AST_EXPR_DECLARATION_POD, NULL, inputs);

    ast_expr_t *return_value = ast_expr_create_variable("$", NULL_SOURCE);
    ast_expr_t *return_stmt = ast_expr_create_return(NULL_SOURCE, return_value, (ast_expr_list_t){0});

    ast_expr_list_append(&func->statements, declare_and_construct_stmt);
    ast_expr_list_append(&func->statements, return_stmt);
}

errorcode_t parse_func_head(parse_ctx_t *ctx, ast_func_head_t *out_head, ast_func_head_parse_info_t *out_info){
    source_t source = parse_ctx_peek_source(ctx);

    ast_func_prefixes_t prefixes;
    parse_func_prefixes(ctx, &prefixes);

    bool is_in_only_constructor = parse_eat(ctx, TOKEN_IN, NULL) == SUCCESS;

    tokenid_t id = parse_ctx_peek(ctx);
    *ctx->i += 1;

    bool is_foreign = id == TOKEN_FOREIGN;
    bool is_constructor = id == TOKEN_CONSTRUCTOR;

    if(id != TOKEN_FUNC && !is_constructor && !is_foreign){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*ctx->i - 1], "Expected 'func' or 'foreign' or 'constructor' keyword");
        return FAILURE;
    }

    maybe_null_strong_cstr_t custom_export_name = parse_eat_string(ctx, NULL);
    strong_cstr_t name;

    if(is_constructor){
        if(ctx->prename != NULL){
            compiler_panic(ctx->compiler, source, "Constructor cannot be named");
            return FAILURE;
        }

        name = strclone("__constructor__");
    } else if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        name = ctx->prename;
        ctx->prename = NULL;
    } else {
        const char *message_on_failure = is_foreign
            ? "Expected function name after 'foreign' keyword"
            : "Expected function name after 'func' keyword";
        
        name = parse_take_word(ctx, message_on_failure);
    }

    if(name == NULL) return FAILURE;

    if(ctx->composite_association == NULL){
        if(is_constructor){
            compiler_panic(ctx->compiler, source, "Constructor must be defined inside the domain of a class/structure");
            return FAILURE;
        } else {
            parse_prepend_namespace(ctx, &name);
        }
    }

    maybe_null_strong_cstr_t export_name = custom_export_name ? strclone(custom_export_name) : (prefixes.is_external ? strclone(name) : NULL);
    bool is_entry = streq(ctx->compiler->entry_point, name);

    *out_head = (ast_func_head_t){
        .name = name,
        .source = source,
        .is_foreign = is_foreign,
        .is_entry = is_entry,
        .prefixes = prefixes,
        .export_name = export_name,
    };

    *out_info = (ast_func_head_parse_info_t){
        .is_constructor = is_constructor,
        .is_in_only_constructor = is_in_only_constructor,
    };

    return SUCCESS;
}

errorcode_t parse_func_body(parse_ctx_t *ctx, ast_func_t *func){
    if(func->traits & AST_FUNC_FOREIGN) {
        #ifdef ADEPT_INSIGHT_BUILD
        func->end_source = parse_ctx_peek_source(ctx);
        #endif

        return SUCCESS;
    }

    if(parse_ignore_newlines(ctx, "Expected function body")) return FAILURE;

    defer_scope_t defer_scope = defer_scope_create(NULL, NULL, TRAIT_NONE);

    ast_expr_list_t stmts;

    if(parse_ctx_peek(ctx) == TOKEN_ASSIGN){
        if(ast_type_is_void(&func->return_type)){
            compiler_panic(ctx->compiler, parse_ctx_peek_source(ctx), "Cannot return 'void' from single line function");
            return FAILURE;
        }

        (*ctx->i)++;
        ctx->func = func;
        
        if(parse_ignore_newlines(ctx, "Expected function body")) return FAILURE;

        ast_expr_t *return_expression;
        if(parse_expr(ctx, &return_expression)) return FAILURE;

        stmts = ast_expr_list_create(1);
        ast_expr_list_append(&stmts, ast_expr_create_return(return_expression->source, return_expression, (ast_expr_list_t){0}));
        goto success;
    }

    if(func->traits & AST_FUNC_DISALLOW && parse_ctx_peek(ctx) != TOKEN_BEGIN){
        // HACK:
        // Since we are expected to end on the last token we processed
        // we will need to go back a token
        (*ctx->i)--;
        return SUCCESS;
    }

    if(parse_eat(ctx, TOKEN_BEGIN, "Expected '{' after function prototype")) return FAILURE;

    stmts = ast_expr_list_create(16);
    ctx->func = func;

    if(parse_stmts(ctx, &stmts, &defer_scope, PARSE_STMTS_STANDARD)){
        ast_expr_list_free(&stmts);
        defer_scope_free(&defer_scope);
        return FAILURE;
    }

    defer_scope_free(&defer_scope);

success:
    #ifdef ADEPT_INSIGHT_BUILD
    func->end_source = parse_ctx_peek_source(ctx);
    #endif

    func->statements = stmts;
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

    // Add automatic `this` parameter if defined inside composite
    if(ctx->composite_association){
        if(func->traits & AST_FUNC_FOREIGN){
            compiler_panic(ctx->compiler, func->source, "Cannot declare foreign function inside of struct domain");
            return FAILURE;
        }

        parse_func_grow_arguments(func, backfill, &capacity);

        if(ctx->composite_association->is_polymorphic){
            // Insert 'this *<$A, $B, $C, ...> AssociatedStruct' as first argument to function

            ast_type_t *generics = malloc(sizeof(ast_type_t) * ctx->composite_association->generics_length);
            length_t generics_length = ctx->composite_association->generics_length;

            for(length_t i = 0; i != generics_length; i++){
                generics[i] = ast_type_make_polymorph(strclone(ctx->composite_association->generics[i]), false);
            }

            ast_elem_t *pointer = ast_elem_pointer_make(NULL_SOURCE);
            ast_elem_t *generic_base = ast_elem_generic_base_make(strclone(ctx->composite_association->name), NULL_SOURCE, generics, generics_length);

            ast_elem_t **elements = malloc(sizeof(ast_elem_t*) * 2);
            elements[0] = pointer;
            elements[1] = generic_base;

            func->arg_types[0] = (ast_type_t){
                .elements = elements,
                .elements_length = 2,
                .source = NULL_SOURCE,
            };
        } else {
            // Insert pointer type of 'this' as first argument to function
            func->arg_types[0] = ast_type_make_base_ptr(strclone(ctx->composite_association->name));
        }

        func->arg_names[0] = strclone("this");
        func->arg_sources[0] = ctx->composite_association->source;
        func->arg_flows[0] = FLOW_IN;
        func->arg_type_traits[0] = TRAIT_NONE;
        func->arity++;
    }
    
    // Allow for no argument list
    if(parse_eat(ctx, TOKEN_OPEN, NULL) != SUCCESS) return SUCCESS;

    // Allow polymorphic prerequisites for function arguments
    ctx->allow_polymorphic_prereqs = true;

    // Parse parameters
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

        if(parse_ignore_newlines(ctx, "Expected type after ',' in argument list")){
            parse_free_unbackfilled_arguments(func, backfill);
            ctx->allow_polymorphic_prereqs = false;
            return FAILURE;
        }
        
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

    parse_collapse_polycount_var_fixed_arrays(func->arg_types, func->arity);
    parse_collapse_polycount_var_fixed_arrays(&func->return_type, 1);

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
    
    if(parse_eat(ctx, TOKEN_ELLIPSIS, NULL) == SUCCESS){
        // Alone ellipsis, used for c-style varargs

        if(*backfill != 0){
            compiler_panic(ctx->compiler, sources[*i - 1], "Expected type for previous arguments before ellipsis");
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        func->traits |= AST_FUNC_VARARG;
        *out_is_solid = false;
        return SUCCESS;
    }

    // Parse argument name
    // TODO: CLEANUP: Cleanup this messy code
    if(func->traits & AST_FUNC_FOREIGN){
        // Parse argument name of foreign function declaration (argument names are optional)

        // Look ahead to see if word token is for type, or is for argument name
        length_t lookahead = *i;
        bool is_argument_name = NULL;

        if(tokens[lookahead].id == TOKEN_WORD){
            lookahead++;

            while(tokens[lookahead].id == TOKEN_NEWLINE){
                lookahead++;
            }

            if(tokens[lookahead].id != TOKEN_NEXT && tokens[lookahead].id != TOKEN_CLOSE){
                is_argument_name = true;
            }
        }

        if(is_argument_name){
            if(func->arg_names == NULL){
                if(func->arity != 0 && ast_type_is_base(&func->arg_types[func->arity - 1])){
                    strong_cstr_t name = ast_type_str(&func->arg_types[func->arity - 1]);
                    compiler_panicf(ctx->compiler, func->arg_sources[func->arity - 1], "'%s' is ambiguous, did you mean '%s Type' (as a parameter name) or '_ %s' (as a type name)?", name, name, name);
                    parse_free_unbackfilled_arguments(func, *backfill);
                    free(name);
                    return FAILURE;
                }

                func->arg_names = calloc(capacity, sizeof(strong_cstr_t));
            }

            maybe_null_strong_cstr_t arg_name = parse_take_word(ctx, "INTERNAL ERROR: Expected argument name while parsing foreign function declaration, will probably crash...");
            func->arg_names[func->arity + *backfill] = arg_name;
        } else {
            if(func->arg_names != NULL){
                func->arg_names[func->arity + *backfill] = NULL;
            }
        }
    } else {
        // Parse argument name for normal function definition (argument names are required)

        maybe_null_strong_cstr_t name = parse_take_word(ctx, "Expected argument name before argument type");

        if(name == NULL){
            parse_free_unbackfilled_arguments(func, *backfill);
            return FAILURE;
        }

        func->arg_names[func->arity + *backfill] = name;
    }

    if(tokens[*i].id == TOKEN_ELLIPSIS){
        // Ellipsis as type, used for modern variadic argument

        if(func->traits & AST_FUNC_FOREIGN){
            compiler_panic(ctx->compiler, sources[*i - 1], "Foreign functions cannot have Adept-style named variadic arguments");
            goto failure;
        }

        if(*backfill != 0){
            compiler_panic(ctx->compiler, sources[*i], "Expected type for previous arguments before ellipsis");
            goto failure;
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

    if(parse_ignore_newlines(ctx, "Expected type")
    || parse_func_default_arg_value_if_applicable(ctx, func, capacity, backfill)
    || parse_ignore_newlines(ctx, "Expected type")){
        goto failure;
    }

    if(!(func->traits & AST_FUNC_FOREIGN) && tokens[*i].id == TOKEN_NEXT){
        if(tokens[++(*i)].id == TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected type after ',' in argument list");
            goto failure;
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
    
    if(parse_ignore_newlines(ctx, "Expected type")
    || parse_type(ctx, &func->arg_types[func->arity + *backfill])
    || parse_ignore_newlines(ctx, "Expected type")
    || parse_func_default_arg_value_if_applicable(ctx, func, capacity, backfill)){
        goto failure;
    }

    parse_func_backfill_arguments(func, backfill);
    func->arity++;
    *out_is_solid = true;
    return SUCCESS;

failure:
    if(func->arg_names) free(func->arg_names[func->arity + *backfill]);
    parse_free_unbackfilled_arguments(func, *backfill);
    return FAILURE;
}

errorcode_t parse_func_default_arg_value_if_applicable(parse_ctx_t *ctx, ast_func_t *func, length_t capacity, length_t *backfill){
    // my_argument float = 0.0f
    //                   ^
    
    if(parse_ctx_peek(ctx) != TOKEN_ASSIGN) return SUCCESS;

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
        func->arg_names = (func->traits & AST_FUNC_FOREIGN) ? NULL : malloc(sizeof(char*) * 4);
        func->arg_types   = malloc(sizeof(ast_type_t) * 4);
        func->arg_sources = malloc(sizeof(source_t) * 4);
        func->arg_flows   = malloc(sizeof(char) * 4);
        func->arg_type_traits = malloc(sizeof(trait_t) * 4);
        *capacity = 4;
        return;
    }

    if(func->arity + backfill != *capacity) return;
    *capacity *= 2;

    if(!(func->traits & AST_FUNC_FOREIGN) || func->arg_names){
        grow((void**) &func->arg_names, sizeof(char*), func->arity + backfill, *capacity);
    }

    grow((void**) &func->arg_types,       sizeof(ast_type_t), func->arity, *capacity);
    grow((void**) &func->arg_sources,     sizeof(source_t),   func->arity + backfill, *capacity);
    grow((void**) &func->arg_flows,       sizeof(char),       func->arity + backfill, *capacity);
    grow((void**) &func->arg_type_traits, sizeof(trait_t),    func->arity + backfill, *capacity);

    if(func->arg_defaults)
        grow((void**) &func->arg_defaults, sizeof(ast_expr_t*), func->arity + backfill, *capacity);
}

void parse_func_prefixes(parse_ctx_t *ctx, ast_func_prefixes_t *out_prefixes){
    memset(out_prefixes, 0, sizeof(ast_func_prefixes_t));

    while(true){
        switch(parse_ctx_peek(ctx)){
        case TOKEN_STDCALL:  out_prefixes->is_stdcall  = true; break;
        case TOKEN_VERBATIM: out_prefixes->is_verbatim = true; break;
        case TOKEN_IMPLICIT: out_prefixes->is_implicit = true; break;
        case TOKEN_EXTERNAL: out_prefixes->is_external = true; break;
        case TOKEN_VIRTUAL:  out_prefixes->is_virtual = true; break;
        case TOKEN_OVERRIDE: out_prefixes->is_override = true; break;
        default: return;
        }

        *ctx->i += 1;
    }
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
        from = parse_take_word(ctx, "Expected function alias name");
        if(from == NULL) return FAILURE;
    }

    // Prepend namespace if applicable
    parse_prepend_namespace(ctx, &from);

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
    
    if(ast->func_aliases_length >= MAX_FUNC_ID){
        compiler_panic(ctx->compiler, source, "Maximum number of AST function aliases reached\n");
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

void parse_collapse_polycount_var_fixed_arrays(ast_type_t *types, length_t length){
    // Will collapse all [$#N] type elements to $#N

    // TODO: CLEANUP: Cleanup?
    for(length_t i = 0; i != length; i++){
        parse_collapse_polycount_var_fixed_arrays_for_type(&types[i]);
    }
}

void parse_collapse_polycount_var_fixed_arrays_for_type(ast_type_t *type){
    // Will collapse all [$#N] type elements to $#N

    for(length_t i = 0; i != type->elements_length; i++){
        ast_elem_t *elem = type->elements[i];

        if(elem->id == AST_ELEM_VAR_FIXED_ARRAY){
            ast_elem_var_fixed_array_t *var_fixed_array = (ast_elem_var_fixed_array_t*) elem;

            if(var_fixed_array->length->id == EXPR_POLYCOUNT){
                ast_expr_polycount_t *old_polycount_expr = (ast_expr_polycount_t*) var_fixed_array->length;
                source_t source = old_polycount_expr->source;

                // Take name
                strong_cstr_t name = old_polycount_expr->name;
                old_polycount_expr->name = NULL;

                // Delete old element
                ast_elem_free(type->elements[i]);

                // Replace with unwrapped version
                ast_elem_polycount_t *new_elem = (ast_elem_polycount_t*) malloc(sizeof(ast_elem_polycount_t));

                *new_elem = (ast_elem_polycount_t){
                    .id =  AST_ELEM_POLYCOUNT,
                    .source = source,
                    .name = name,
                };

                type->elements[i] = (ast_elem_t*) new_elem;
            }
        }
    }
}
