
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "AST/POLY/ast_resolve.h"
#include "AST/TYPE/ast_type_make.h"
#include "AST/ast.h"
#include "AST/ast_expr.h"
#include "AST/ast_layout.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "LEX/token.h"
#include "PARSE/parse_ctx.h"
#include "PARSE/parse_struct.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "TOKEN/token_data.h"
#include "UTIL/builtin_type.h"
#include "UTIL/ground.h"
#include "UTIL/search.h"
#include "UTIL/string.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

errorcode_t parse_composite(parse_ctx_t *ctx, bool is_union){
    ast_t *ast = ctx->ast;
    source_t source = parse_ctx_peek_source(ctx);

    if(ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare %s within another struct's domain", is_union ? "union" : "struct");
        return FAILURE;
    }

    strong_cstr_t name;
    bool is_packed, is_record, is_class;
    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    ast_type_t maybe_parent_class = AST_TYPE_NONE;
    if(parse_composite_head(ctx, is_union, &name, &is_packed, &is_record, &is_class, &maybe_parent_class, &generics, &generics_length)) return FAILURE;

    const char *invalid_names[] = {
        "Any", "AnyFixedArrayType", "AnyFuncPtrType", "AnyPtrType", "AnyStructType",
        "AnyType", "AnyTypeKind", "StringOwnership", "bool", "byte", "double", "float", "int", "long", "ptr",
        "short", "successful", "ubyte", "uint", "ulong", "ushort", "usize", "void"

        // NOTE: 'String' is allowed
    };
    length_t invalid_names_length = sizeof(invalid_names) / sizeof(const char*);

    if(binary_string_search_const(invalid_names, invalid_names_length, name) != -1){
        compiler_panicf(ctx->compiler, source, "Reserved type name '%s' can't be used to create a %s", name, is_union ? "union" : "struct");
        goto body_failure;
    }

    ast_field_map_t field_map;
    ast_layout_skeleton_t skeleton;

    if(parse_composite_body(ctx, &field_map, &skeleton, is_class, maybe_parent_class)){
        goto body_failure;
    }

    ast_composite_t *domain = NULL;
    trait_t traits = is_packed ? AST_LAYOUT_PACKED : TRAIT_NONE;
    ast_layout_kind_t layout_kind = is_union ? AST_LAYOUT_UNION : AST_LAYOUT_STRUCT;

    // AST type of composite being declared (if it's a record)
    ast_layout_t layout;
    ast_layout_init(&layout, layout_kind, field_map, skeleton, traits);
    
    if(generics){
        domain = (ast_composite_t*) ast_add_poly_composite(ast, name, layout, source, maybe_parent_class, is_class, generics, generics_length);
    } else {
        domain = ast_add_composite(ast, name, layout, source, maybe_parent_class, is_class);
    }
    
    if(is_record){
        // Create constructor function if composite is a record type
        // NOTE: Ownership of 'return_type' is given away
        if(parse_create_record_constructor(ctx, name, generics, generics_length, &layout, source)) return FAILURE;
    }

    if(parse_composite_domain(ctx, domain)) return FAILURE;
    return SUCCESS;

body_failure:
    free(name);
    ast_type_free(&maybe_parent_class);
    return FAILURE;
}

errorcode_t parse_composite_domain(parse_ctx_t *ctx, ast_composite_t *composite){
    length_t anchor = *ctx->i;

    if(parse_struct_is_function_like_beginning(parse_ctx_peek(ctx))){
        ctx->composite_association = (ast_poly_composite_t*) composite;
        *ctx->i -= 1;
        return SUCCESS;
    }

    if(parse_eat(ctx, TOKEN_CLOSE, NULL)) goto nothing_found;
    if(parse_ignore_newlines(ctx, NULL)) goto nothing_found;

    if(parse_ctx_peek(ctx) == TOKEN_BEGIN){
        ctx->composite_association = (ast_poly_composite_t*) composite;
        return SUCCESS;
    }

nothing_found:
    if(composite->is_class){
        compiler_panicf(ctx->compiler, composite->source, "Class is missing constructor");
        return FAILURE;
    }

    *ctx->i = anchor;
    return SUCCESS;
}

bool parse_struct_is_function_like_beginning(tokenid_t token){
    switch(token){
    case TOKEN_CONSTRUCTOR:
    case TOKEN_FUNC:
    case TOKEN_IMPLICIT:
    case TOKEN_IN:
    case TOKEN_VERBATIM:
    case TOKEN_VIRTUAL:
    case TOKEN_OVERRIDE:
        return true;
    default:
        return false;
    }
}

errorcode_t parse_composite_head(
    parse_ctx_t *ctx,
    bool is_union,
    strong_cstr_t *out_name,
    bool *out_is_packed,
    bool *out_is_record,
    bool *out_is_class,
    ast_type_t *out_parent_class,
    strong_cstr_t **out_generics,
    length_t *out_generics_length
){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    *out_is_packed = false;
    *out_is_record = false;
    *out_is_class = false;
    *out_parent_class = (ast_type_t){0};

    if(is_union){
        if(parse_eat(ctx, TOKEN_UNION, "Expected 'union' keyword for union definition")) return FAILURE;
    } else {
        if(tokens[*i].id == TOKEN_PACKED){
            *out_is_packed = true;
            *i += 1;
        }
        
        if(tokens[*i].id == TOKEN_RECORD){
            *out_is_record = true;
            *i += 1;
        } else if(tokens[*i].id == TOKEN_CLASS){
            *out_is_class = true;
            *i += 1;
        } else if(parse_eat(ctx, TOKEN_STRUCT, "Expected 'struct' keyword after 'packed' keyword")){
            return FAILURE;
        } 
    }

    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    length_t generics_capacity = 0;

    if(parse_eat(ctx, TOKEN_LESSTHAN, NULL) == SUCCESS){
        while(parse_ctx_peek(ctx) != TOKEN_GREATERTHAN){
            expand((void**) &generics, sizeof(strong_cstr_t), generics_length, &generics_capacity, 1, 4);

            if(parse_ignore_newlines(ctx, "Expected polymorphic generic type")){
                goto failure;
            }

            if(parse_ctx_peek(ctx) != TOKEN_POLYMORPH){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type");
                goto failure;
            }

            generics[generics_length++] = parse_ctx_peek_data_take(ctx);
            *i += 1;

            if(parse_ignore_newlines(ctx, "Expected '>' or ',' after polymorphic generic type")){
                goto failure;
            }

            if(parse_eat(ctx, TOKEN_NEXT, NULL) == SUCCESS){
                if(parse_ctx_peek(ctx) == TOKEN_GREATERTHAN){
                    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type after ',' in generics list");
                    goto failure;
                }
            } else if(parse_ctx_peek(ctx) != TOKEN_GREATERTHAN){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' after polymorphic generic type");
                goto failure;
            }
        }

        (*i)++;
    }

    if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        *out_name = ctx->prename;
        ctx->prename = NULL;
    } else {
        *out_name = parse_take_word(ctx, "Expected structure name after 'struct' keyword");
        if(*out_name == NULL) goto failure;
    }

    if(parse_eat(ctx, TOKEN_EXTENDS, NULL) == SUCCESS){
        if(parse_type(ctx, out_parent_class)) goto failure;
    }

    parse_prepend_namespace(ctx, out_name);

    *out_generics = generics;
    *out_generics_length = generics_length;
    return SUCCESS;

failure:
    free_strings(generics, generics_length);
    return FAILURE;
}

errorcode_t parse_composite_body(
    parse_ctx_t *ctx,
    ast_field_map_t *out_field_map,
    ast_layout_skeleton_t *out_skeleton,
    bool is_class,
    ast_type_t maybe_parent_class
){
    // Parses root-level composite fields

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    if(parse_ignore_newlines(ctx, "Expected '(' or '{' after composite name")) return FAILURE;

    if(tokens[*i].id != TOKEN_OPEN && tokens[*i].id != TOKEN_BEGIN){
        compiler_panic(ctx->compiler, sources[*i], "Expected '(' or '{' after composite name");
        return FAILURE;
    }

    ctx->struct_closer = tokens[(*i)++].id == TOKEN_OPEN ? TOKEN_CLOSE : TOKEN_END;
    ctx->struct_closer_char = ctx->struct_closer == TOKEN_CLOSE ? ')' : '}';

    ast_field_map_init(out_field_map);
    ast_layout_skeleton_init(out_skeleton);

    length_t backfill = 0;
    ast_layout_endpoint_t next_endpoint;
    ast_layout_endpoint_init_with(&next_endpoint, (uint16_t[]){0}, 1);

    if(is_class){
        if(!AST_TYPE_IS_NONE(maybe_parent_class)){
            if(parse_composite_integrate_another(ctx, out_field_map, out_skeleton, &next_endpoint, &maybe_parent_class, true)) goto failure;
        } else {
            ast_type_t vtable_ast_type = ast_type_make_base(strclone("ptr"));

            ast_field_map_add(out_field_map, strclone("__vtable__"), next_endpoint);
            ast_layout_endpoint_increment(&next_endpoint);
            ast_layout_skeleton_add_type(out_skeleton, vtable_ast_type);
        }
    }

    if(parse_ignore_newlines(ctx, "Expected name of field")) return FAILURE;

    while((tokens[*i].id != ctx->struct_closer && !parse_struct_is_function_like_beginning(tokens[*i].id)) || backfill != 0){
        // Be lenient with unnecessary preceding commas
        if(tokens[*i].id == TOKEN_NEXT){
            (*i)++;
        }

        // Parse field
        if(parse_ignore_newlines(ctx, "Expected name of field")
        || parse_composite_field(ctx, out_field_map, out_skeleton, &backfill, &next_endpoint)){
            goto failure;
        }

        // Handle whitespace
        bool auto_comma = tokens[*i].id == TOKEN_NEWLINE;
        if(parse_ignore_newlines(ctx, ctx->struct_closer_char == ')' ? "Expected ')' or ',' after field" : "Expected '}' or ',' after field")){
            goto failure;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            // Handle ',' token 
            (*i)++;

            if(parse_ignore_newlines(ctx, ctx->struct_closer_char == ')' ? "Expected ')' before end-of-file" : "Expected '}' before end-of-file")){
                goto failure;
            }

            // Allow for unnecessary tailing comma when closing
            tokenid_t ending = tokens[*i].id;
            if(ending == ctx->struct_closer || parse_struct_is_function_like_beginning(ending)){
                break;
            }
        } else if(tokens[*i].id != ctx->struct_closer && !parse_struct_is_function_like_beginning(tokens[*i].id) && !auto_comma){
            // Expect closing ')' unless auto comma activated
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' after field name and type");
            goto failure;
        }
    }

    return SUCCESS;

failure:
    ast_field_map_free(out_field_map);
    ast_layout_skeleton_free(out_skeleton);
    return FAILURE;
}

errorcode_t parse_composite_field(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    length_t *inout_backfill,
    ast_layout_endpoint_t *inout_next_endpoint
){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    const tokenid_t leading_token = tokens[*i].id;

    // TODO: Cleanup condition
    if(leading_token == TOKEN_STRUCT && tokens[*i + 1].id != TOKEN_OPEN && tokens[*i + 1].id != TOKEN_BRACKET_OPEN){
        // Struct Integration Field

        if(*inout_backfill != 0){
            compiler_panicf(ctx->compiler, sources[*i], "Expected field type for previous fields before integrated struct");
            return FAILURE;
        }

        // Ignore 'struct' keyword
        *i += 1;

        ast_type_t inner_composite_type;
        if(parse_type(ctx, &inner_composite_type)) return FAILURE;

        errorcode_t errorcode = parse_composite_integrate_another(ctx, inout_field_map, inout_skeleton, inout_next_endpoint, &inner_composite_type, false);
        ast_type_free(&inner_composite_type);
        return errorcode;
    }

    if(leading_token == TOKEN_PACKED || leading_token == TOKEN_STRUCT || leading_token == TOKEN_UNION){
        // Anonymous struct/union

        if(*inout_backfill != 0){
            const char *kind_name = leading_token == TOKEN_UNION ? "union" : "struct";
            compiler_panicf(ctx->compiler, sources[*i], "Expected field type for previous fields before anonymous %s", kind_name);
            return FAILURE;
        }

        return parse_anonymous_composite(ctx, inout_field_map, inout_skeleton, inout_next_endpoint);
    }

    // Otherwise it's just a regular field

    strong_cstr_t field_name = parse_take_word(ctx, "Expected name of field");
    if(field_name == NULL) return FAILURE;

    ast_field_map_add(inout_field_map, field_name, *inout_next_endpoint);
    ast_layout_endpoint_increment(inout_next_endpoint);

    if(tokens[*i].id == TOKEN_NEXT || tokens[*i].id == TOKEN_NEWLINE){
        // This field is part of a field list where all the fields have the same type,
        // and the type is specified at the end
        (*inout_backfill)++;
        return SUCCESS;
    }

    ast_type_t field_type;
    if(parse_type(ctx, &field_type)) return FAILURE;

    while(*inout_backfill != 0){
        ast_layout_skeleton_add_type(inout_skeleton, ast_type_clone(&field_type));
        *inout_backfill -= 1;
    }

    ast_layout_skeleton_add_type(inout_skeleton, field_type);
    return SUCCESS;
}

static errorcode_t resolve_polymorphs_in_integration_for_bone(parse_ctx_t *ctx, source_t source_on_error, ast_poly_catalog_t *catalog, ast_layout_bone_t *bone, int depth_left){
    if(depth_left <= 0){
        compiler_panicf(ctx->compiler, source_on_error, "Refusing to resolve AST polymorphism in composite layout that nests absurdly deep");
        return FAILURE;
    }

    switch(bone->kind){
    case AST_LAYOUT_BONE_KIND_TYPE:
        if(ast_resolve_type_polymorphs(ctx->compiler, NULL, catalog, &bone->type, NULL)){
            return FAILURE;
        }
        break;
    case AST_LAYOUT_BONE_KIND_STRUCT:
    case AST_LAYOUT_BONE_KIND_UNION: {
            for(length_t i = 0; i != bone->children.bones_length; i++){
                if(resolve_polymorphs_in_integration_for_bone(ctx, source_on_error, catalog, &bone->children.bones[i], depth_left - 1)){
                    return FAILURE;
                }
            }
        }
        break;
    default:
        compiler_panicf(ctx->compiler, source_on_error, "resolve_polymorphs_in_integration_for_bone() got unknown AST layout bone kind '%d'", (int) bone->kind);
        return FAILURE;
    }

    return SUCCESS;
}

static errorcode_t resolve_polymorphs_in_integration(
    parse_ctx_t *ctx,
    ast_layout_t *poly_layout,
    const ast_type_t *usage,
    ast_layout_t *out_layout,
    weak_cstr_t *generics,
    length_t generics_length
){
    assert(ast_type_is_generic_base(usage));

    ast_elem_generic_base_t *generic_base = (ast_elem_generic_base_t*) usage->elements[0];

    if(generic_base->generics_length != generics_length){
        compiler_panicf(ctx->compiler, usage->source, "Incorrect number of type parameters specified for type '%s'", generic_base->name);
        return FAILURE;
    }

    ast_layout_t layout = ast_layout_clone(poly_layout);

    ast_poly_catalog_t catalog;
    ast_poly_catalog_init(&catalog);
    ast_poly_catalog_add_types(&catalog, generics, generic_base->generics, generics_length);

    ast_layout_bone_t root = ast_layout_as_bone(&layout);

    if(resolve_polymorphs_in_integration_for_bone(ctx, usage->source, &catalog, &root, 64)){
        goto failure;
    }

    ast_poly_catalog_free(&catalog);
    *out_layout = layout;
    return SUCCESS;

failure:
    ast_poly_catalog_free(&catalog);
    ast_layout_free(&layout);
    return FAILURE;
}

errorcode_t parse_composite_integrate_another(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    ast_layout_endpoint_t *inout_next_endpoint,
    const ast_type_t *other_type,
    bool require_class
){
    ast_composite_t *composite = ast_find_composite(ctx->ast, other_type);

    if(composite == NULL){
        const char *message = require_class ? "Cannot extend non-existent class '%s'" : "Struct '%s' must already be declared";
        strong_cstr_t typename = ast_type_str(other_type);
        compiler_panicf(ctx->compiler, other_type->source, message, typename);
        free(typename);

        if(require_class){
            printf("    Please note that parent classes must be defined before their children\n");
        }
        return FAILURE;
    }

    ast_layout_t layout_storage;
    ast_layout_t *layout;

    if(composite->is_polymorphic){
        ast_poly_composite_t *poly_composite = (ast_poly_composite_t*) composite;

        if(resolve_polymorphs_in_integration(ctx, &composite->layout, other_type, &layout_storage, poly_composite->generics, poly_composite->generics_length)){
            return FAILURE;
        }

        layout = &layout_storage;
    } else {
        layout = &composite->layout;
    }

    // Don't support complex composites for now
    if(!ast_layout_is_simple_struct(layout)){
        const char *message =
            require_class
                ? "Cannot extend class '%s' which has a complex layout"
                : "Cannot integrate composite '%s' which has a complex layout";
        
        strong_cstr_t typename = ast_type_str(other_type);
        compiler_panicf(ctx->compiler, other_type->source, message, typename);
        free(typename);
        if(layout == &layout_storage) ast_layout_free(&layout_storage);
        return FAILURE;
    }

    length_t field_count = ast_simple_field_map_get_count(&layout->field_map);

    for(length_t i = 0; i != field_count; i++){
        weak_cstr_t field_name = ast_simple_field_map_get_name_at_index(&layout->field_map, i);
        ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&layout->skeleton, i);

        ast_field_map_add(inout_field_map, strclone(field_name), *inout_next_endpoint);
        ast_layout_skeleton_add_type(inout_skeleton, ast_type_clone(field_type));
        ast_layout_endpoint_increment(inout_next_endpoint);
    }

    if(layout == &layout_storage) ast_layout_free(&layout_storage);
    return SUCCESS;
}

errorcode_t parse_anonymous_composite(
    parse_ctx_t *ctx,
    ast_field_map_t *inout_field_map,
    ast_layout_skeleton_t *inout_skeleton,
    ast_layout_endpoint_t *inout_next_endpoint
){
    // (Inside of composite definition)
    // struct (x, y, z float)
    //   ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    bool is_packed = tokens[*i].id == TOKEN_PACKED;
    if(is_packed) (*i)++;

    // Assumes either TOKEN_STRUCT or TOKEN_UNION
    ast_layout_bone_kind_t bone_kind = tokens[*i].id == TOKEN_STRUCT ? AST_LAYOUT_BONE_KIND_STRUCT : AST_LAYOUT_BONE_KIND_UNION;
    (*i)++;

    trait_t bone_traits = is_packed ? AST_LAYOUT_BONE_PACKED : TRAIT_NONE;
    ast_layout_skeleton_t *child_skeleton = ast_layout_skeleton_add_child_skeleton(inout_skeleton, bone_kind, bone_traits);
    ast_layout_endpoint_t child_next_endpoint = *inout_next_endpoint;

    if(!ast_layout_endpoint_add_index(&child_next_endpoint, 0)){
        compiler_panicf(ctx->compiler, sources[*i], "Maximum depth of anonymous composites exceeded - No more than %d are allowed", AST_LAYOUT_MAX_DEPTH);
        return FAILURE;
    }

    length_t backfill = 0;

    if(parse_ignore_newlines(ctx, "Expected '(' for anonymous composite")
    || parse_eat(ctx, TOKEN_OPEN, "Expected '(' for anonymous composite")){
        return FAILURE;
    }

    while(tokens[*i].id != TOKEN_CLOSE || backfill != 0){
        if(parse_ignore_newlines(ctx, "Expected name of field")
        || parse_composite_field(ctx, inout_field_map, child_skeleton, &backfill, &child_next_endpoint)
        || parse_ignore_newlines(ctx, "Expected ')' or ',' after field")){
            return FAILURE;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected field name and type after ',' in field list");
                return FAILURE;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
            compiler_panic(ctx->compiler, sources[*i], "Expected ',' after field name and type");
            return FAILURE;
        }
    }

    ast_layout_endpoint_increment(inout_next_endpoint);
    (*i)++;
    return SUCCESS;
}

errorcode_t parse_create_record_constructor(parse_ctx_t *ctx, weak_cstr_t name, strong_cstr_t *generics, length_t generics_length, ast_layout_t *layout, source_t source){
    if(!ast_layout_is_simple_struct(layout)) {
        compiler_panicf(ctx->compiler, source, "Record type '%s' cannot be defined to have a complicated structure", name);
        return FAILURE;
    }

    if(name[0] == '_' && name[1] == '_'){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' cannot start with double underscores", name);
        return FAILURE;
    }

    bool would_collide_with_entry = streq(name, ctx->compiler->entry_point);
    if(would_collide_with_entry){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' conflicts with name of entry point", name);
        return FAILURE;
    }

    ast_t *ast = ctx->ast;
    ast_layout_skeleton_t *skeleton = &layout->skeleton;
    ast_field_map_t *field_map = &layout->field_map;
    bool is_polymorphic = ast_layout_skeleton_has_polymorph(skeleton) || generics;

    // Variable name of value being made in the constructor,
    // this name should not be able to be used in normal contexts
    weak_cstr_t master_variable_name = "$";

    // Add function
    func_id_t ast_func_id = ast_new_func(ast);
    ast_func_t *func = &ast->funcs[ast_func_id];

    ast_func_create_template(func, &(ast_func_head_t){
        .name = strclone(name),
        .source = NULL_SOURCE,
        .is_foreign = false,
        .is_entry = false,
        .prefixes = {0},
        .export_name = NULL,
    });

    if(func->traits != TRAIT_NONE){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' conflicts with special symbol", name);
        return FAILURE;
    }

    // Figure out AST type to return for record
    if(generics){
        func->return_type = ast_type_make_base_with_polymorphs(strclone(name), generics, generics_length);
    } else {
        func->return_type = ast_type_make_base(strclone(name));
    }

    // Track whether or not all fields are primitive builtin types,
    // if so, we can skip out on zero initializing the '$' value
    bool all_primitive = true;

    // Set arguments
    func->arity = field_map->arrows_length;
    func->arg_names = malloc(sizeof(strong_cstr_t) * func->arity);
    func->arg_types = malloc(sizeof(ast_type_t) * func->arity);
    func->arg_flows = malloc(sizeof(char) * func->arity);
    func->arg_defaults = NULL;
    func->arg_sources = malloc(sizeof(source_t) * func->arity);
    func->arg_type_traits = malloc(sizeof(trait_t) * func->arity);

    for(length_t i = 0; i != field_map->arrows_length; i++){
        ast_field_arrow_t *arrow = &field_map->arrows[i];
        func->arg_names[i] = strclone(arrow->name);
        func->arg_types[i] = ast_type_clone(ast_layout_skeleton_get_type(skeleton, arrow->endpoint));
        func->arg_flows[i] = FLOW_IN;
        func->arg_sources[i] = NULL_SOURCE;
        func->arg_type_traits[i] = AST_FUNC_ARG_TYPE_TRAIT_POD;

        if(all_primitive) {
            if(ast_type_is_base(&func->arg_types[i])){
                weak_cstr_t typename = ((ast_elem_base_t*) func->arg_types[i].elements[0])->base;
                all_primitive = typename_builtin_type(typename) != -1;
            } else {
                all_primitive = false;
            }
        }
    }

    func->statements = ast_expr_list_create(func->arity + 2);

    ast_expr_list_append_unchecked(&func->statements, ast_expr_create_declaration(
        all_primitive ? EXPR_DECLAREUNDEF : EXPR_DECLARE,
        source,
        master_variable_name,
        ast_type_clone(&func->return_type),
        AST_EXPR_DECLARATION_POD | AST_EXPR_DECLARATION_ASSIGN_POD,
        NULL,
        NO_AST_EXPR_LIST
    ));

    for(length_t i = 0; i != func->arity; i++){
        weak_cstr_t field_name = field_map->arrows[i].name;

        ast_expr_t *master = ast_expr_create_variable(master_variable_name, source);

        // Member value
        ast_expr_t *mutable_expression = ast_expr_create_member(master, strclone(field_name), source);

        // Argument variable
        ast_expr_t *variable = ast_expr_create_variable(field_name, source);

        ast_expr_list_append_unchecked(&func->statements, ast_expr_create_assignment(EXPR_ASSIGN, source, mutable_expression, variable, false));
    }

    ast_expr_list_append_unchecked(
        &func->statements,
        ast_expr_create_return(source, ast_expr_create_variable(master_variable_name, source), (ast_expr_list_t){0})
    );

    // Add function to polymorphic function registry if it's polymorphic
    if(is_polymorphic){
        func->traits |= AST_FUNC_POLYMORPHIC;
        ast_add_poly_func(ast, func->name, ast_func_id);
    }

    return SUCCESS;
}
