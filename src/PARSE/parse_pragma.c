
#include "UTIL/color.h"
#include "UTIL/search.h"
#include "UTIL/filename.h"
#include "PARSE/parse_pragma.h"
#include "PARSE/parse_ctx.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

errorcode_t parse_pragma(parse_ctx_t *ctx){
    // pragma <directive> ...
    //   ^

    length_t *i = ctx->i;
    token_t *tokens = ctx->tokenlist->tokens;
    maybe_null_weak_cstr_t read = NULL;

    if(ctx->struct_association != NULL){
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*ctx->i], "Cannot pass pragma directives within struct domain");
        return FAILURE;
    }

    const char * const directives[] = {
        "compiler_supports", "compiler_version", "deprecated", "disable_warnings", "enable_warnings", "help", "libm",
        "mac_only", "no_type_info", "no_typeinfo", "no_undef", "null_checks", "optimization", "options", "package", "project_name",
        "unsafe_meta", "unsafe_new", "unsupported", "windows_only"
    };

    const length_t directives_length = sizeof(directives) / sizeof(const char * const);

    weak_cstr_t directive_string = parse_grab_word(ctx, "Expected pragma option after 'pragma' keyword");
    if(directive_string == NULL) return FAILURE;

    maybe_index_t directive = binary_string_search(directives, directives_length, directive_string);

    switch(directive){
    case 0: // 'compiler_supports' directive
    case 1: // 'compiler_version' directive
        read = parse_grab_string(ctx, directive == 0
                ? "Expected compiler version string after 'pragma compiler_supports'"
                : "Expected compiler version string after 'pragma compiler_version'");

        if(read == NULL){
            printf("\nDid you mean: pragma %s '%s'?\n", directive == 0 ? "compiler_supports" : "compiler_version", ADEPT_VERSION_STRING);
            return FAILURE;
        }

        // Check to make sure we support the target version
        if(strcmp(read, "2.0") == 0 || strcmp(read, "2.1") == 0){
            compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler only partially supports version '%s'", read);
        } else if(strcmp(read, "2.2") != 0 && strcmp(read, "2.3") != 0){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler doesn't support version '%s'", read);
            puts("\nSupported Versions: '2.3', '2.2', '2.1', '2.0'");
            return FAILURE;
        }
        return SUCCESS;
    case 2: // 'deprecated' directive
        read = parse_grab_string(ctx, NULL);

        if(read == NULL){
            if(tokens[*i].id != TOKEN_NEWLINE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma deprecated'");
                return FAILURE;
            } else (*i)--;
        }

        if(read != NULL){
            compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "This file is deprecated and may be removed in the future\n %s %s", BOX_DRAWING_UP_RIGHT, read);
        } else {
            compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "This file is deprecated and may be removed in the future");
        }
        return SUCCESS;
    case 3: // 'disable_warnings' directive
        ctx->compiler->traits |= COMPILER_NO_WARN;
        return SUCCESS;
    case 4: // 'enable_warnings' directive
        ctx->compiler->traits &= ~COMPILER_NO_WARN;
        return SUCCESS;
    case 5: // 'help' directive
        show_help();
        return FAILURE;
    case 6: // 'libm' directive
        ctx->compiler->use_libm = true;
        return SUCCESS;
    case 7: // 'mac_only' directive
        #if !defined(__APPLE__) || !TARGET_OS_MAC
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Mac");
        return FAILURE;
        #else
        return SUCCESS;
        #endif
    case 8: // 'no_type_info' directive
        compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "WARNING: 'pragma no_type_info' is obsolete, use 'pragma no_typeinfo' instead");
    case 9: // 'no_typeinfo'  directive
        ctx->compiler->traits |= COMPILER_NO_TYPEINFO;
        return SUCCESS;
    case 10: // 'no_undef' directive
        ctx->compiler->traits |= COMPILER_NO_UNDEF;
        return SUCCESS;
    case 11: // 'null_checks' directive
        ctx->compiler->checks |= COMPILER_NULL_CHECKS;
        return SUCCESS;
    case 12: // 'optimization' directive
        read = parse_grab_word(ctx, "Expected optimization level after 'pragma optimization'");

        if(read == NULL){
            printf("Possible levels are: none, less, normal or aggressive\n");
            return FAILURE;
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
            return FAILURE;
        }
        return SUCCESS;
    case 13: // 'options' directive
        return parse_pragma_cloptions(ctx);
    case 14: // 'package' directive
        if(ctx->compiler->traits & COMPILER_INFLATE_PACKAGE) return SUCCESS;
        if(compiler_create_package(ctx->compiler, ctx->object) == 0){
            ctx->compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        }
        return FAILURE;
    case 15: // 'project_name' directive
        read = parse_grab_string(ctx, "Expected string containing project name after 'pragma project_name'");
        if(read == NULL) return FAILURE;

        free(ctx->compiler->output_filename);
        ctx->compiler->output_filename = filename_local(ctx->object->filename, read);
        return SUCCESS;
    case 16: // 'unsafe_meta' directive
        ctx->compiler->traits |= COMPILER_UNSAFE_META;
        return SUCCESS;
    case 17: // 'unsafe_new' directive
        ctx->compiler->traits |= COMPILER_UNSAFE_NEW;
        return SUCCESS;
    case 18: // 'unsupported' directive
        read = parse_grab_string(ctx, NULL);

        if(read == NULL){
            if(tokens[*i].id != TOKEN_NEWLINE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma unsupported'");
                return FAILURE;
            } else (*i)--;
        }

        if(read != NULL){
            object_panic_plain(ctx->object, "This file is no longer supported or was never supported to begin with!");
            redprintf(" %s %s\n", BOX_DRAWING_UP_RIGHT, read);
        } else {
            compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i], "This file is no longer supported or never was unsupported");
        }
        return FAILURE;
    case 19: // 'windows_only' directive
        #ifndef _WIN32
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Windows");
        return FAILURE;
        #else
        return SUCCESS;
        #endif
    default:
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Unrecognized pragma option '%s'", directive_string);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t parse_pragma_cloptions(parse_ctx_t *ctx){
    // pragma options 'some arguments'
    //           ^

    weak_cstr_t options = parse_grab_string(ctx, "Expected string containing compiler options after 'pragma options'");
    if(options == NULL || ctx->compiler->traits & COMPILER_INFLATE_PACKAGE) return FAILURE;

    int options_argc; char **options_argv;
    break_into_arguments(options, &options_argc, &options_argv);

    // Parse the compiler options
    if(parse_arguments(ctx->compiler, ctx->object, options_argc, options_argv)){
        for(length_t a = 1; a != options_argc; a++) free(options_argv[a]);
        free(options_argv);
        return FAILURE;
    }

    // Free allocated options array
    for(length_t a = 1; a != options_argc; a++) free(options_argv[a]);
    free(options_argv);

    // Perform actions needed for flags for previously missed events
    // Export as a package if the flag was set, because we missed the time to do it earlier
    if(ctx->compiler->traits & COMPILER_MAKE_PACKAGE){
        if(compiler_create_package(ctx->compiler, ctx->object) == 0) ctx->compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        return FAILURE;
    }

    return SUCCESS;
}
