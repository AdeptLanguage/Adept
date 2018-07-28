
#include "pragma.h"
#include "search.h"
#include "filename.h"

int parse_pragma(parse_ctx_t *ctx){
    // pragma <directive> ...
    //   ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    const char *read = NULL;

    const char * const directives[] = {
        "compiler_version", "deprecated", "help", "optimization", "options",
        "project_name", "unsupported", "windows_only"
    };

    const length_t directives_length = sizeof(directives) / sizeof(const char * const);

    if(tokens[++(*i)].id != TOKEN_WORD){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected pragma option after 'pragma' keyword");
        return 1;
    }

    char *directive_string = (char*) tokens[*i].data;
    int directive = binary_string_search(directives, directives_length, directive_string);

    switch(directive){
    case 0: // 'compiler_version' directive
        read = parse_pragma_string(tokens, i);

        if(read == NULL){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected compiler version string after 'pragma compiler_version'");
            puts("\nDid you mean: pragma compiler_version '2.0'?");
            return 1;
        }

        // Check to make sure we support the target version
        if(strcmp(read, "2.0") != 0 && strcmp(read, "2.1") != 0){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler doesn't support version '%s'", read);
            puts("\nSupported Versions: '2.0', '2.1'");
            return 1;
        }
        return 0;
    case 1: // 'deprecated' directive
        read = parse_pragma_string(tokens, i);

        if(read == NULL){
            if(tokens[*i].id != TOKEN_NEWLINE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma deprecated'");
                return 1;
            } else (*i)--;
        }

        if(read != NULL){
            compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "This file is deprecated and may be removed in the future\n \xC0 %s", read);
        } else {
            compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "This file is deprecated and may be removed in the future");
        }
        return 0;
    case 2: // 'help' directive
        show_help();
        return 1;
    case 3: // 'optimization' directive
        read = parse_pragma_string(tokens, i);

        if(read == NULL){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected optimization level after 'pragma optimization'");
            printf("Possible levels are: none, less, normal or aggressive\n");
            return 1;
        }

        // Change the optimization level accordingly
        if(strcmp(read, "none") == 0)            ctx->compiler->optimization = OPTIMIZATION_NONE;
        else if(strcmp(read, "less") == 0)       ctx->compiler->optimization = OPTIMIZATION_LESS;
        else if(strcmp(read, "normal") == 0)     ctx->compiler->optimization = OPTIMIZATION_DEFAULT;
        else if(strcmp(read, "aggressive") == 0) ctx->compiler->optimization = OPTIMIZATION_AGGRESSIVE;
        else {
            // Invalid optimiztaion level
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "Invalid optimization level after 'pragma optimization'");
            printf("Possible levels are: none, less, normal or aggressive\n");
            return 1;
        }
        return 0;
    case 4: // 'options' directive
        return parse_pragma_cloptions(ctx);
    case 5: // 'project_name' directive
        read = parse_pragma_string(tokens, i);

        if(read == NULL){
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected string containing project name after 'pragma project_name'");
            return 1;
        }

        free(ctx->compiler->output_filename);
        ctx->compiler->output_filename = filename_local(ctx->object->filename, read);
        return 0;
    case 6: // 'unsupported' directive
        read = parse_pragma_string(tokens, i);

        if(read == NULL){
            if(tokens[*i].id != TOKEN_NEWLINE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma unsupported'");
                return 1;
            } else (*i)--;
        }

        if(read != NULL){
            object_panic_plain(ctx->object, "This file is no longer supported or was never supported to begin with!");
            redprintf(" \xC0 %s\n", read);
        } else {
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "This file is no longer supported or never was unsupported");
        }
        return 1;
    case 7: // 'windows_only' directive
        #ifndef _WIN32
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Windows");
        return 1;
        #endif
    default:
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Unrecognized pragma option '%s'", directive_string);
        return 1;
    }

    return 0;
}

char* parse_pragma_string(token_t *tokens, length_t *i){
    // pragma <directive> 'a string'
    //             ^

    if(tokens[++(*i)].id != TOKEN_CSTRING && tokens[*i].id != TOKEN_STRING) return NULL;
    return (char*) tokens[*i].data;
}

int parse_pragma_cloptions(parse_ctx_t *ctx){
    // pragma options 'some arguments'
    //           ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;

    char *options = parse_pragma_string(tokens, i);

    if(options == NULL){
        compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected string containing compiler options after 'pragma options'");
        return 1;
    }

    int options_argc; char **options_argv;
    break_into_arguments(options, &options_argc, &options_argv);

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
    return 0;
}
