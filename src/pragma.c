
#include "pragma.h"

int parse_pragma(parse_ctx_t *ctx){
    // pragma <option> ...
    //   ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    if(tokens[++(*i)].id != TOKEN_WORD){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected pragma option after 'pragma' keyword");
        return 1;
    }

    char *option = (char*) tokens[*i].data;

    if(strcmp(option, "help") == 0){
        show_help(); return 1;
    } else if(strcmp(option, "options") == 0){
        if(tokens[++(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected string containing compiler options after 'pragma options'");
            return 1;
        }

        int options_argc; char **options_argv;
        break_into_arguments((char*) tokens[*i].data, &options_argc, &options_argv);

        // Parse the compiler options
        if(parse_arguments(ctx->compiler, ctx->object, options_argc, options_argv)){
            for(length_t a = 1; a != options_argc; a++) free(options_argv[a]);
            free(options_argv);
            return 1;
        }

        // Free allocated options array
        for(length_t a = 1; a != options_argc; a++) free(options_argv[a]);
        free(options_argv);

        // Perform actions needed for flags for previously missed events
        // Export as a package if the flag was set, because we missed the time to do it earlier
        if(ctx->compiler->traits & COMPILER_MAKE_PACKAGE){
            if(compiler_create_package(ctx->compiler, ctx->object) == 0) ctx->compiler->result_flags |= COMPILER_RESULT_SUCCESS;
            return 1;
        }
    } else {
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Unrecognized pragma option '%s'", option);
        return 1;
    }
    return 0;
}
