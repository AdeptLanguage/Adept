
#include "pragma.h"
#include "filename.h"

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
    } else if(strcmp(option, "project_name") == 0){
        if(tokens[++(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected string containing project name after 'pragma project_name'");
            return 1;
        }

        free(ctx->compiler->output_filename);
        ctx->compiler->output_filename = filename_local(ctx->object->filename, (char*) tokens[*i].data);
        return 0;
    } else if(strcmp(option, "optimization") == 0){
        if(tokens[++(*i)].id != TOKEN_WORD){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected optimization level after 'pragma optimization'");
            printf("Possible levels are: none, less, normal or aggressive\n");
            return 1;
        }

        const char *level = (char*) tokens[*i].data;

        if(strcmp(level, "none") == 0){
            ctx->compiler->optimization = OPTIMIZATION_NONE;
            return 0;
        } else if(strcmp(level, "less") == 0){
            ctx->compiler->optimization = OPTIMIZATION_LESS;
            return 0;
        } else if(strcmp(level, "normal") == 0){
            ctx->compiler->optimization = OPTIMIZATION_DEFAULT;
            return 0;
        } else if(strcmp(level, "aggressive") == 0){
            ctx->compiler->optimization = OPTIMIZATION_AGGRESSIVE;
            return 0;
        } else {
            // Invalid optimiztaion level
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Invalid optimization level after 'pragma optimization'");
            printf("Possible levels are: none, less, normal or aggressive\n");
            return 1;
        }
    } else {
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Unrecognized pragma option '%s'", option);
        return 1;
    }

    return 0;
}
