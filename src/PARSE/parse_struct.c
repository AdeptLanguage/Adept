
#include "UTIL/util.h"
#include "UTIL/search.h"
#include "UTIL/builtin_type.h"
#include "PARSE/parse.h"
#include "PARSE/parse_type.h"
#include "PARSE/parse_util.h"
#include "PARSE/parse_struct.h"

errorcode_t parse_composite(parse_ctx_t *ctx, bool is_union){
    ast_t *ast = ctx->ast;
    source_t source = ctx->tokenlist->sources[*ctx->i];

    if(ctx->composite_association != NULL){
        compiler_panicf(ctx->compiler, source, "Cannot declare %s within another struct's domain", is_union ? "union" : "struct");
        return FAILURE;
    }

    strong_cstr_t name;
    bool is_packed, is_record;
    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    if(parse_composite_head(ctx, is_union, &name, &is_packed, &is_record, &generics, &generics_length)) return FAILURE;

    const char *invalid_names[] = {
        "Any", "AnyFixedArrayType", "AnyFuncPtrType", "AnyPtrType", "AnyStructType",
        "AnyType", "AnyTypeKind", "StringOwnership", "bool", "byte", "double", "float", "int", "long", "ptr",
        "short", "successful", "ubyte", "uint", "ulong", "ushort", "usize", "void"

        // NOTE: 'String' is allowed
    };
    length_t invalid_names_length = sizeof(invalid_names) / sizeof(const char*);

    if(binary_string_search(invalid_names, invalid_names_length, name) != -1){
        compiler_panicf(ctx->compiler, source, "Reserved type name '%s' can't be used to create a %s", name, is_union ? "union" : "struct");
        free(name);
        return FAILURE;
    }

    ast_field_map_t field_map;
    ast_layout_skeleton_t skeleton;

    if(parse_composite_body(ctx, &field_map, &skeleton)){
        free(name);
        return FAILURE;
    }

    ast_composite_t *domain = NULL;
    trait_t traits = is_packed ? AST_LAYOUT_PACKED : TRAIT_NONE;
    ast_layout_kind_t layout_kind = is_union ? AST_LAYOUT_UNION : AST_LAYOUT_STRUCT;

    ast_layout_t layout;
    ast_layout_init(&layout, layout_kind, field_map, skeleton, traits);
    
    if(generics){
        domain = (ast_composite_t*) ast_add_polymorphic_composite(ast, name, layout, source, generics, generics_length);
    } else {
        domain = ast_add_composite(ast, name, layout, source);
    }

    if(is_record){
        if(parse_create_record_constructor(ctx, name, &layout, source)) return FAILURE;
    }

    // Look for start of struct domain and set it up if it exists
    if(parse_struct_is_function_like_beginning(ctx->tokenlist->tokens[*ctx->i].id)){
        ctx->composite_association = (ast_polymorphic_composite_t *)domain;
        *ctx->i -= 1;
    } else {
        length_t scan_i = *ctx->i + 1;
        while(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_NEWLINE)
            scan_i++;

        if(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_BEGIN){
            ctx->composite_association = (ast_polymorphic_composite_t*) domain;
            *ctx->i = scan_i;
        }
    }

    return SUCCESS;
}

bool parse_struct_is_function_like_beginning(tokenid_t token){
    return token == TOKEN_FUNC || token == TOKEN_VERBATIM;
}

errorcode_t parse_composite_head(parse_ctx_t *ctx, bool is_union, strong_cstr_t *out_name, bool *out_is_packed, bool *out_is_record, strong_cstr_t **out_generics, length_t *out_generics_length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    *out_is_packed = false;
    *out_is_record = false;

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
        } else {
            if(parse_eat(ctx, TOKEN_STRUCT, "Expected 'struct' keyword after 'packed' keyword")) return FAILURE;
        } 
    }

    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    length_t generics_capacity = 0;

    if(tokens[*i].id == TOKEN_LESSTHAN){
        (*i)++;

        while(tokens[*i].id != TOKEN_GREATERTHAN){
            expand((void**) &generics, sizeof(strong_cstr_t), generics_length, &generics_capacity, 1, 4);

            if(parse_ignore_newlines(ctx, "Expected polymorphic generic type")){
                freestrs(generics, generics_length);
                return FAILURE;
            }

            if(tokens[*i].id != TOKEN_POLYMORPH){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type");
                freestrs(generics, generics_length);
                return FAILURE;
            }

            generics[generics_length++] = tokens[*i].data;
            tokens[(*i)++].data = NULL; // Take ownership

            if(parse_ignore_newlines(ctx, "Expected '>' or ',' after polymorphic generic type")){
                freestrs(generics, generics_length);
                return FAILURE;
            }

            if(tokens[*i].id == TOKEN_NEXT){
                if(tokens[++(*i)].id == TOKEN_GREATERTHAN){
                    compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected polymorphic generic type after ',' in generics list");
                    freestrs(generics, generics_length);
                    return FAILURE;
                }
            } else if(tokens[*i].id != TOKEN_GREATERTHAN){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Expected ',' after polymorphic generic type");
                freestrs(generics, generics_length);
                return FAILURE;
            }
        }

        (*i)++;
    }

    if(ctx->compiler->traits & COMPILER_COLON_COLON && ctx->prename){
        *out_name = ctx->prename;
        ctx->prename = NULL;
    } else {
        *out_name = parse_take_word(ctx, "Expected structure name after 'struct' keyword");

        if(*out_name == NULL){
            freestrs(generics, generics_length);
            return FAILURE;
        }
    }

    parse_prepend_namespace(ctx, out_name);

    *out_generics = generics;
    *out_generics_length = generics_length;
    return SUCCESS;
}

errorcode_t parse_composite_body(parse_ctx_t *ctx, ast_field_map_t *out_field_map, ast_layout_skeleton_t *out_skeleton){
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

    while((tokens[*i].id != ctx->struct_closer && !parse_struct_is_function_like_beginning(tokens[*i].id)) || backfill != 0){
        // Be lenient with unnecessary preceeding commas
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

errorcode_t parse_composite_field(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, length_t *inout_backfill, ast_layout_endpoint_t *inout_next_endpoint){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    source_t *sources = ctx->tokenlist->sources;

    const tokenid_t leading_token = tokens[*i].id;

    if(leading_token == TOKEN_STRUCT && tokens[*i + 1].id == TOKEN_WORD){
        // Struct Integration Field

        if(*inout_backfill != 0){
            compiler_panicf(ctx->compiler, sources[*i], "Expected field type for previous fields before integrated struct");
            return FAILURE;
        }

        return parse_struct_integration_field(ctx, inout_field_map, inout_skeleton, inout_next_endpoint);
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

    ast_layout_skeleton_add_type(inout_skeleton, ast_type_clone(&field_type));
    return SUCCESS;
}

errorcode_t parse_struct_integration_field(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, ast_layout_endpoint_t *inout_next_endpoint){
    // (Inside of composite definition)
    // struct SomeStructure
    //   ^

    length_t *i = ctx->i;
    source_t *sources = ctx->tokenlist->sources;

    maybe_null_weak_cstr_t inner_struct_name = parse_grab_word(ctx, "Expected struct name for integration");
    if(inner_struct_name == NULL) return FAILURE;

    ast_composite_t *inner_composite = ast_composite_find_exact(ctx->ast, inner_struct_name);
    if(inner_composite == NULL){
        compiler_panicf(ctx->compiler, sources[*i], "Struct '%s' must already be declared", inner_struct_name);
        return FAILURE;
    }

    // Don't support complex composites for now
    if(!ast_layout_is_simple_struct(&inner_composite->layout)){
        compiler_panicf(ctx->compiler, sources[*i], "Cannot integrate complex composite '%s', only simple structs are allowed", inner_struct_name);
        return FAILURE;
    }

    length_t field_count = ast_simple_field_map_get_count(&inner_composite->layout.field_map);

    for(length_t f = 0; f != field_count; f++){
        weak_cstr_t field_name = ast_simple_field_map_get_name_at_index(&inner_composite->layout.field_map, f);
        ast_type_t *field_type = ast_layout_skeleton_get_type_at_index(&inner_composite->layout.skeleton, f);

        ast_field_map_add(inout_field_map, strclone(field_name), *inout_next_endpoint);
        ast_layout_skeleton_add_type(inout_skeleton, ast_type_clone(field_type));
        ast_layout_endpoint_increment(inout_next_endpoint);
    }

    (*i)++;
    return SUCCESS;
}

errorcode_t parse_anonymous_composite(parse_ctx_t *ctx, ast_field_map_t *inout_field_map, ast_layout_skeleton_t *inout_skeleton, ast_layout_endpoint_t *inout_next_endpoint){
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

errorcode_t parse_create_record_constructor(parse_ctx_t *ctx, weak_cstr_t name, ast_layout_t *layout, source_t source){
    if(!ast_layout_is_simple_struct(layout)) {
        compiler_panicf(ctx->compiler, source, "Record type '%s' cannot be defined to have a complicated structure", name);
        return FAILURE;
    }

    if(name[0] == '_' && name[1] == '_'){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' cannot start with double underscores", name);
        return FAILURE;
    }

    bool would_collide_with_entry = strcmp(name, ctx->compiler->entry_point) == 0;
    if(would_collide_with_entry){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' conflicts with name of entry point", name);
        return FAILURE;
    }

    ast_t *ast = ctx->ast;
    ast_layout_skeleton_t *skeleton = &layout->skeleton;
    ast_field_map_t *field_map = &layout->field_map;
    bool is_polymorphic = ast_layout_skeleton_has_polymorph(skeleton);

    // Variable name of value being made in the constructor,
    // this name should not be able to be used in normal contexts
    weak_cstr_t master_variable_name = "$";

    // Add function
    expand((void**) &ast->funcs, sizeof(ast_func_t), ast->funcs_length, &ast->funcs_capacity, 1, 4);

    length_t ast_func_id = ast->funcs_length;
    ast_func_t *func = &ast->funcs[ast->funcs_length++];
    ast_func_create_template(func, strclone(name), false, false, false, false, source, false, strclone(name));

    if(func->traits != TRAIT_NONE){
        compiler_panicf(ctx->compiler, source, "Name of record type '%s' conflicts with special symbol", name);
        return FAILURE;
    }

    // Set return type
    ast_type_make_base(&func->return_type, strclone(name));

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

    // Set statements
    func->statements_capacity = func->arity + 2;
    func->statements_length = func->statements_capacity;
    func->statements = malloc(sizeof(ast_expr_t*) * func->statements_capacity);

    trait_t traits = AST_EXPR_DECLARATION_POD | AST_EXPR_DECLARATION_ASSIGN_POD;
    ast_expr_create_declaration(&func->statements[0], all_primitive ? EXPR_DECLAREUNDEF : EXPR_DECLARE, source, master_variable_name, ast_type_clone(&func->return_type), traits, NULL);

    for(length_t i = 0; i != func->arity; i++){
        weak_cstr_t field_name = field_map->arrows[i].name;

        ast_expr_t *master;
        ast_expr_create_variable(&master, master_variable_name, source);

        // Member value
        ast_expr_t *mutable_expression;
        ast_expr_create_member(&mutable_expression, master, strclone(field_name), source);

        // Argument variable
        ast_expr_t *variable;
        ast_expr_create_variable(&variable, field_name, source);

        ast_expr_create_assignment(&func->statements[i + 1], EXPR_ASSIGN, source, mutable_expression, variable, false);
    }

    ast_expr_t *variable;
    ast_expr_create_variable(&variable, master_variable_name, source);

    ast_expr_list_t last_minute;
    memset(&last_minute, 0, sizeof(ast_expr_list_t));

    ast_expr_create_return(&func->statements[func->arity + 1], source, variable, last_minute);

    // Add function to polymorphic function registry if it's polymorphic
    if(is_polymorphic){
        expand((void**) &ast->polymorphic_funcs, sizeof(ast_polymorphic_func_t), ast->polymorphic_funcs_length, &ast->polymorphic_funcs_capacity, 1, 4);

        ast_polymorphic_func_t *poly_func = &ast->polymorphic_funcs[ast->polymorphic_funcs_length++];
        poly_func->name = func->name;
        poly_func->ast_func_id = ast_func_id;
        poly_func->is_beginning_of_group = -1; // Uncalculated
    }

    return SUCCESS;
}
