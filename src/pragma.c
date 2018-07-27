
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
        show_help();
        return 1;
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

        // Change the target output filename
        free(ctx->compiler->output_filename);
        ctx->compiler->output_filename = filename_local(ctx->object->filename, (char*) tokens[*i].data);
        return 0;
    } else if(strcmp(option, "optimization") == 0){
        if(tokens[++(*i)].id != TOKEN_WORD){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected optimization level after 'pragma optimization'");
            printf("Possible levels are: none, less, normal or aggressive\n");
            return 1;
        }

        // Get the optimization level string
        const char *level = (char*) tokens[*i].data;

        // Change the optimization level accordingly
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
    } else if(strcmp(option, "compiler_version") == 0){
        if(tokens[++(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected compiler version string after 'pragma compiler_version'");
            puts("\nDid you mean: pragma compiler_version '2.0'?");
            return 1;
        }

        // Get the target compiler version
        const char *target_version = (char*) tokens[*i].data;

        // Check to make sure we support the target version
        if(strcmp(target_version, "2.0") != 0){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler doesn't support version '%s'", target_version);
            puts("\nSupported Versions: '2.0'");
            return 1;
        }
        return 0;
    } else if(strcmp(option, "deprecated") == 0){
        char *deprecation_message = NULL;

        if(tokens[++(*i)].id != TOKEN_NEWLINE){
            if(tokens[(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma deprecated'");
                return 1;
            }

            deprecation_message = (char*) tokens[*i].data;
        } else (*i)--;

        if(deprecation_message != NULL){
            compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "This file is deprecated and may be removed in the future\n \xC0 %s", deprecation_message);
        } else {
            compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "This file is deprecated and may be removed in the future");
        }
        return 0;
    } else if(strcmp(option, "unsupported") == 0){
        char *unsupported_message = NULL;

        if(tokens[++(*i)].id != TOKEN_NEWLINE){
            if(tokens[(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma unsupported'");
                return 1;
            }

            unsupported_message = (char*) tokens[*i].data;
        } else (*i)--;

        if(unsupported_message != NULL){
            object_panic_plain(ctx->object, "This file is no longer supported or was never supported to begin with!");
            redprintf(" \xC0 %s\n", unsupported_message);
        } else {
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "This file is no longer supported or never was unsupported");
        }
        return 1;
    } else if(strcmp(option, "windows_only") == 0){
        #ifndef _WIN32
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Windows");
        return 1;
        #endif
    } else {
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Unrecognized pragma option '%s'", option);
        return 1;
    }

    return 0;
}
