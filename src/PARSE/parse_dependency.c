
#include "PARSE/parse.h"
#include "PARSE/parse_dependency.h"
#include "UTIL/filename.h"

errorcode_t parse_import(parse_ctx_t *ctx){
    // import 'somefile.adept'
    //   ^

    maybe_null_weak_cstr_t file = parse_grab_string(ctx, "Expected filename string after 'import' keyword");
    if(file == NULL) return FAILURE;

    maybe_null_strong_cstr_t target = parse_find_import(ctx, file);
    if(target == NULL) return FAILURE;

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

    *ctx->i += 1;
    if(parse_import_object(ctx, target, absolute)) return FAILURE;

    return SUCCESS;
}

void parse_foreign_library(parse_ctx_t *ctx){
    // foreign 'libmycustomlibrary.a'
    //    ^

    // Assume the token we're currently on is 'foreign' keyword
    maybe_null_weak_cstr_t library = parse_grab_string(ctx, "INTERNAL ERROR: Assumption failed that 'foreign' keyword would be proceeded by a string, will probably crash...");
    bool is_framework = false;

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    if(tokens[*i + 1].id == TOKEN_WORD){
        const char *data = tokens[*i + 1].data;

        if(strcmp(data, "framework") == 0){
            is_framework = true;

            // Take ownership of library string
            tokens[*i].data = NULL;
        } else {
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i + 1], "Unrecognized foreign library type '%s'", data);
        }

        (*ctx->i)++;
    }

    if(!is_framework) library = filename_local(ctx->object->filename, library);
    ast_add_foreign_library(ctx->ast, library, is_framework);
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

maybe_null_strong_cstr_t parse_find_import(parse_ctx_t *ctx, weak_cstr_t filename){
    strong_cstr_t test = filename_local(ctx->object->filename, filename);
    if(access(test, F_OK) != -1) return test;

    free(test);
    test = filename_adept_import(ctx->compiler->root, filename);
    if(access(test, F_OK) != -1) return test;

    compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "The file '%s' doesn't exist", filename);
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
