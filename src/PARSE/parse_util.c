
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "PARSE/parse_util.h"

errorcode_t parse_ignore_newlines(parse_ctx_t *ctx, const char *error_message){
    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    length_t length = ctx->tokenlist->length;

    while(tokens[*i].id == TOKEN_NEWLINE) if(length == ++(*i)){
        if(error_message) compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], error_message);
        return FAILURE;
    }

    return SUCCESS;
}

void parse_panic_token(parse_ctx_t *ctx, source_t source, unsigned int token_id, const char *message){
    // Expects from 'ctx': compiler, object
    // Expects 'message' to have a '%s' to display the token name

    int line, column;

    if(ctx->object->traits & OBJECT_PACKAGE){
        printf("%s: ", filename_name_const(ctx->object->filename));
    } else {
        lex_get_location(ctx->object->buffer, source.index, &line, &column);
        printf("%s:%d:%d: ", filename_name_const(ctx->object->filename), line, column);
    }

    redprintf("error: ");
    printf(message, global_token_name_table[token_id]);
    printf("\n");

    if(!(ctx->object->traits & OBJECT_PACKAGE)){
        compiler_print_source(ctx->compiler, line, source);
    }

    if(ctx->compiler->error == NULL){
        strong_cstr_t buffer = calloc(256, 1);
        snprintf(buffer, 256, message, global_token_name_table[token_id]);
        ctx->compiler->error = adept_error_create(buffer, source);
    }
}
