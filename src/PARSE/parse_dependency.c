
#include "PARSE/parse.h"
#include "PARSE/parse_dependency.h"
#include "UTIL/filename.h"
#include "UTIL/util.h"

errorcode_t parse_import(parse_ctx_t *ctx){
    // import 'somefile.adept'
    //   ^

    // Don't allow importing while inside struct domains
    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Cannot import dependencies within struct domain");
        return FAILURE;
    }

    // Figure out the filename
    source_t source = NULL_SOURCE;
    maybe_null_strong_cstr_t file = NULL;
    bool is_standard_library_component = ctx->tokenlist->tokens[*ctx->i + 1].id == TOKEN_WORD;

    if(is_standard_library_component){
        // import standard_library_module
        //   ^

        // Get stdlib location
        strong_cstr_t standard_library_folder = compiler_get_stdlib(ctx->compiler, ctx->object);

        strong_cstr_t full_component = parse_standard_library_component(ctx, &source);
        if(full_component == NULL) return FAILURE;

        // Combine standard library and component name to create the filename
        file = mallocandsprintf("%s%s.adept", standard_library_folder, full_component);
        free(full_component);
        free(standard_library_folder);
    } else {
        // Grab filename string of what file to import
        file = parse_grab_string(ctx, "Expected filename string or standard library component after 'import' keyword");

        // Make it a 'strong_cstr_t'
        file = file ? strclone(file) : NULL;

        // Set code source
        source = ctx->tokenlist->sources[(*ctx->i)++];
    }
    
    if(file == NULL) return FAILURE;

    if(parse_do_import(ctx, file, source, !is_standard_library_component)){
        free(file);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_do_import(parse_ctx_t *ctx, weak_cstr_t file, source_t source, bool allow_local){
    maybe_null_strong_cstr_t target = parse_find_import(ctx, file, source, allow_local);

    if(target == NULL){
        if(!allow_local){
            printf("\nPerhaps you are using the wrong standard library version?\n\n");
        }
        
        return FAILURE;
    }

    maybe_null_strong_cstr_t absolute = parse_resolve_import(ctx, target);
    if(absolute == NULL){
        free(target);
        return FAILURE;
    }

    if(already_imported(ctx, absolute)){
        free(target);
        free(absolute);
        return SUCCESS;
    }

    if(parse_import_object(ctx, target, absolute)) return FAILURE;
    return SUCCESS;
}

errorcode_t parse_foreign_library(parse_ctx_t *ctx){
    // foreign 'libmycustomlibrary.a'
    //    ^

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Cannot declare foreign dependencies within struct domain");
        return FAILURE;
    }

    // Assume the token we're currently on is 'foreign' keyword
    maybe_null_weak_cstr_t library = parse_grab_string(ctx, "INTERNAL ERROR: Assumption failed that 'foreign' keyword would be proceeded by a string, will probably crash...");
    char kind = LIBRARY_KIND_NONE;

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    if(tokens[*i + 1].id == TOKEN_WORD){
        const char *data = tokens[*i + 1].data;

        if(strcmp(data, "framework") == 0){
            kind = LIBRARY_KIND_FRAMEWORK;

            // Take ownership of library string
            tokens[*i].data = NULL;
        } else if(strcmp(data, "library") == 0){
            kind = LIBRARY_KIND_LIBRARY;

            // Take ownership of library string
            tokens[*i].data = NULL;
        } else {
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i + 1], "Unrecognized foreign library type '%s'", data);
        }

        (*ctx->i)++;
    }

    // NOTE: 'library' become as strong_cstr_t
    if(kind == LIBRARY_KIND_NONE) library = filename_local(ctx->object->filename, library);
    ast_add_foreign_library(ctx->ast, library, kind);
    return SUCCESS;
}

errorcode_t parse_import_object(parse_ctx_t *ctx, strong_cstr_t relative_filename, strong_cstr_t absolute_filename) {
    object_t *new_object = compiler_new_object(ctx->compiler);
    new_object->filename = relative_filename;
    new_object->full_filename = absolute_filename;

    if(compiler_read_file(ctx->compiler, new_object)) return FAILURE;

    parse_ctx_t ctx_fork;
    parse_ctx_fork(ctx, new_object, &ctx_fork);

    if(parse_tokens(&ctx_fork)) return FAILURE;

    return SUCCESS;
}

maybe_null_strong_cstr_t parse_standard_library_component(parse_ctx_t *ctx, source_t *out_source){
    // Read component name
    maybe_null_strong_cstr_t full_component = NULL;
    length_t full_component_length = 0;
    length_t full_component_capacity = 0;

    maybe_null_weak_cstr_t first_part_of_component = parse_grab_word(ctx, "Expected standard library component name");
    if(first_part_of_component == NULL) return NULL;

    // Set base source
    *out_source = ctx->tokenlist->sources[*ctx->i];

    while(ctx->tokenlist->tokens[*ctx->i + 1].id == TOKEN_DIVIDE){
        // Skip over previous component name
        (*ctx->i)++;

        maybe_null_weak_cstr_t additional_part = parse_grab_word(ctx, "Expected component name after '/' in 'import' statement");
        if(additional_part == NULL) return NULL;

        length_t additional_part_length = strlen(additional_part);

        if(full_component == NULL){
            length_t first_part_of_component_length = strlen(first_part_of_component);

            expand((void**) &full_component, sizeof(char), full_component_length, &full_component_capacity, first_part_of_component_length, 256);
            memcpy(full_component, first_part_of_component, first_part_of_component_length);
            full_component_length = first_part_of_component_length;
        }
        
        expand((void**) &full_component, sizeof(char), full_component_length, &full_component_capacity, additional_part_length + 2, 256);
        full_component[full_component_length] = '/';
        memcpy(&full_component[full_component_length + 1], additional_part, additional_part_length);
        full_component[full_component_length + 1 + additional_part_length] = '\0';

        full_component_length += 1 + additional_part_length;
    }

    // Advance past the last component part
    (*ctx->i)++;
    
    // Update source stride if using 'thing1/...'
    // HACK: For 'import thing1/thing2/thing3', assume that there are no spaces in between the slashes
    if(full_component) out_source->stride = full_component_length;

    return full_component ? full_component : strclone(first_part_of_component);
}

maybe_null_strong_cstr_t parse_find_import(parse_ctx_t *ctx, weak_cstr_t filename, source_t source, bool allow_local_import){
    strong_cstr_t test;

    if(allow_local_import){
        test = filename_local(ctx->object->filename, filename);
        if(access(test, F_OK) != -1) return test;
        free(test);
    }

    test = filename_adept_import(ctx->compiler->root, filename);
    if(access(test, F_OK) != -1) return test;
    
    compiler_panicf(ctx->compiler, source, "The file '%s' doesn't exist", filename);
    free(test);
    return NULL;
}

maybe_null_strong_cstr_t parse_resolve_import(parse_ctx_t *ctx, weak_cstr_t filename){
    char *absolute = filename_absolute(filename);
    if(absolute) return absolute;

    compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "INTERNAL ERROR: Failed to get absolute path of filename '%s'", filename);
    return NULL;
}

bool already_imported(parse_ctx_t *ctx, weak_cstr_t filename){
    object_t **objects = ctx->compiler->objects;
    length_t objects_length = ctx->compiler->objects_length;

    for(length_t i = 0; i != objects_length; i++){
        if(strcmp(objects[i]->full_filename, filename) == 0) return true;
    }

    return false;
}
