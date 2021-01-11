
#include "UTIL/util.h"
#include "UTIL/search.h"
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
    bool is_packed;
    strong_cstr_t *generics = NULL;
    length_t generics_length = 0;
    if(parse_composite_head(ctx, is_union, &name, &is_packed, &generics, &generics_length)) return FAILURE;

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

    // Look for start of struct domain and set it up if it exists
    length_t scan_i = *ctx->i + 1;
    while(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_NEWLINE)
        scan_i++;
    
    if(scan_i < ctx->tokenlist->length && ctx->tokenlist->tokens[scan_i].id == TOKEN_BEGIN){
        ctx->composite_association = (ast_polymorphic_composite_t*) domain;
        *ctx->i = scan_i;
    }

    return SUCCESS;
}

errorcode_t parse_composite_head(parse_ctx_t *ctx, bool is_union, strong_cstr_t *out_name, bool *out_is_packed, strong_cstr_t **out_generics, length_t *out_generics_length){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    if(!is_union){
        *out_is_packed = (tokens[*i].id == TOKEN_PACKED);
        if(*out_is_packed) *i += 1;
        if(parse_eat(ctx, TOKEN_STRUCT, "Expected 'struct' keyword after 'packed' keyword")) return FAILURE;
    } else {
        if(parse_eat(ctx, TOKEN_UNION, "Expected 'union' keyword for union definition")) return FAILURE;
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

    if(parse_ignore_newlines(ctx, "Expected '(' after composite name")
    || parse_eat(ctx, TOKEN_OPEN, "Expected '(' after composite name")){
        return FAILURE;
    }

    ast_field_map_init(out_field_map);
    ast_layout_skeleton_init(out_skeleton);

    length_t backfill = 0;
    ast_layout_endpoint_t next_endpoint;
    ast_layout_endpoint_init_with(&next_endpoint, (uint16_t[]){0}, 1);

    while(tokens[*i].id != TOKEN_CLOSE || backfill != 0){
        if(parse_ignore_newlines(ctx, "Expected name of field")
        || parse_composite_field(ctx, out_field_map, out_skeleton, &backfill, &next_endpoint)
        || parse_ignore_newlines(ctx, "Expected ')' or ',' after field")){
            goto failure;
        }

        if(tokens[*i].id == TOKEN_NEXT){
            if(tokens[++(*i)].id == TOKEN_CLOSE){
                compiler_panic(ctx->compiler, sources[*i], "Expected field name and type after ',' in field list");
                goto failure;
            }
        } else if(tokens[*i].id != TOKEN_CLOSE){
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

    if(tokens[*i].id == TOKEN_NEXT){
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

    ast_composite_t *inner_composite = object_composite_find(ctx->ast, ctx->object, &ctx->compiler->tmp, inner_struct_name, NULL);
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
