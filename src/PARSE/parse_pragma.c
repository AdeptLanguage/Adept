
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

    // NOTE: Must be presorted alphabetically and match with indicies below
    const char * const directives[] = {
        "__builtin_warn_bad_printf_format", "compiler_supports", "compiler_version", "default_stdlib", "deprecated", "disable_warnings", "enable_warnings", "help",
        "ignore_all", "ignore_deprecation", "ignore_early_return", "ignore_obsolete", "ignore_partial_support", "ignore_unrecognized_directives", "ignore_unused", "libm",
        "linux_only", "mac_only", "no_type_info", "no_typeinfo", "no_undef", "null_checks", "optimization", "options", "package", "project_name",
        "short_warnings", "unsafe_meta", "unsafe_new", "unsupported", "warn_as_error", "warn_short", "windows_only"
    };

    const length_t directives_length = sizeof(directives) / sizeof(const char * const);

    weak_cstr_t directive_string = parse_grab_word(ctx, "Expected pragma option after 'pragma' keyword");
    if(directive_string == NULL) return FAILURE;

    #define PRAGMA___BULITIN_WARN_BAD_PRINTF_FORMAT 0x00000000
    #define PRAGMA_COMPILER_SUPPORTS                0x00000001
    #define PRAGMA_COMPILER_VERSION                 0x00000002
    #define PRAGMA_DEFAULT_STDLIB                   0x00000003
    #define PRAGMA_DEPRECATED                       0x00000004
    #define PRAGMA_DISABLE_WARNINGS                 0x00000005
    #define PRAGMA_ENABLE_WARNINGS                  0x00000006
    #define PRAGMA_HELP                             0x00000007
    #define PRAGMA_IGNORE_ALL                       0x00000008
    #define PRAGMA_IGNORE_DEPRECATION               0x00000009
    #define PRAGMA_IGNORE_EARLY_RETURN              0x0000000A
    #define PRAGMA_IGNORE_OBSOLETE                  0x0000000B
    #define PRAGMA_IGNORE_PARTIAL_SUPPORT           0x0000000C
    #define PRAGMA_IGNORE_UNRECOGNIZED_DIRECTIVES   0x0000000D
    #define PRAGMA_IGNORE_UNUSED                    0x0000000E
    #define PRAGMA_LIBM                             0x0000000F
    #define PRAGMA_LINUX_ONLY                       0x00000010
    #define PRAGMA_MAC_ONLY                         0x00000011
    #define PRAGMA_NO_TYPE_INFO                     0x00000012
    #define PRAGMA_NO_TYPEINFO                      0x00000013
    #define PRAGMA_NO_UNDEF                         0x00000014
    #define PRAGMA_NULL_CHECKS                      0x00000015
    #define PRAGMA_OPTIMIZATION                     0x00000016
    #define PRAGMA_OPTIONS                          0x00000017
    #define PRAGMA_PACKAGE                          0x00000018
    #define PRAGMA_PROJECT_NAME                     0x00000019
    #define PRAGMA_SHORT_WARNINGS                   0x0000001A
    #define PRAGMA_UNSAFE_META                      0x0000001B
    #define PRAGMA_UNSAFE_NEW                       0x0000001C
    #define PRAGMA_UNSUPPORTED                      0x0000001D
    #define PRAGMA_WARN_AS_ERROR                    0x0000001E
    #define PRAGMA_WARN_SHORT                       0x0000001F
    #define PRAGMA_WINDOWS_ONLY                     0x00000020

    maybe_index_t directive = binary_string_search(directives, directives_length, directive_string);

    switch(directive){
    case PRAGMA___BULITIN_WARN_BAD_PRINTF_FORMAT: // '__builtin_warn_bad_printf_format' directive
        ctx->next_builtin_traits |= AST_FUNC_WARN_BAD_PRINTF_FORMAT;
        return SUCCESS;
    case PRAGMA_COMPILER_SUPPORTS: // 'compiler_supports' directive
    case PRAGMA_COMPILER_VERSION: // 'compiler_version' directive
        read = parse_grab_string(ctx, directive == 0
                ? "Expected compiler version string after 'pragma compiler_supports'"
                : "Expected compiler version string after 'pragma compiler_version'");

        if(read == NULL){
            printf("\nDid you mean: pragma %s '%s'?\n", directive == 0 ? "compiler_supports" : "compiler_version", ADEPT_VERSION_STRING);
            return FAILURE;
        }

        // Check to make sure we support the target version
        if(strcmp(read, "2.0") == 0 || strcmp(read, "2.1") == 0){
            if(!(ctx->compiler->ignore & COMPILER_IGNORE_PARTIAL_SUPPORT)){
                if(compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler only partially supports version '%s'", read))
                    return FAILURE;
            }
        } else if(strcmp(read, "2.2") != 0 && strcmp(read, "2.3") != 0 && strcmp(read, "2.4") != 0){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This compiler doesn't support version '%s'", read);
            puts("\nSupported Versions: '2.4', '2.3', '2.2', '2.1', '2.0'");
            return FAILURE;
        }

        // Guess default standard library version from 'pragma compiler_version' if none specified
        if(!(ctx->compiler->traits & COMPILER_FORCE_STDLIB) && ctx->compiler->default_stblib == NULL && strcmp(directive_string, "compiler_version") == 0){
            ctx->compiler->default_stblib = read;
        }

        return SUCCESS;
    case PRAGMA_DEFAULT_STDLIB: // 'default_stdlib' directive
        read = parse_grab_string(ctx, NULL);

        if(read == NULL){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Expected version string after 'pragma default_stdlib', such as '%s'", ADEPT_VERSION_STRING);
            return FAILURE;
        }
        
        if(!(ctx->compiler->traits & COMPILER_FORCE_STDLIB)){
            // Store default_stdlib per compilation object
            ctx->object->default_stblib = read;

            // If the primary file, store default_stdlib as global default as well
            if(ctx->object->index == 0){
                ctx->compiler->default_stblib = read;
            }
        }
        break;
    case PRAGMA_DEPRECATED: // 'deprecated' directive
        read = parse_grab_string(ctx, NULL);

        if(read == NULL){
            if(tokens[*i].id != TOKEN_NEWLINE){
                compiler_panic(ctx->compiler, ctx->tokenlist->sources[*i - 1], "Expected message after 'pragma deprecated'");
                return FAILURE;
            } else (*i)--;
        }

        if(!(ctx->compiler->ignore & COMPILER_IGNORE_DEPRECATION)){
            bool should_exit = false;

            if(read != NULL){
                should_exit = compiler_warnf(ctx->compiler, ctx->tokenlist->sources[*i - 1], "This file is deprecated and may be removed in the future\n %s %s", BOX_DRAWING_UP_RIGHT, read);
            } else {
                should_exit = compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "This file is deprecated and may be removed in the future");
            }

            if(should_exit) return FAILURE;
        }
        return SUCCESS;
    case PRAGMA_DISABLE_WARNINGS: // 'disable_warnings' directive
        ctx->compiler->traits |= COMPILER_NO_WARN;
        return SUCCESS;
    case PRAGMA_ENABLE_WARNINGS: // 'enable_warnings' directive
        ctx->compiler->traits &= ~COMPILER_NO_WARN;
        return SUCCESS;
    case PRAGMA_HELP: // 'help' directive
        show_help(true);
        return FAILURE;
    case PRAGMA_IGNORE_ALL: // 'ignore_all' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_ALL;
        return SUCCESS;
    case PRAGMA_IGNORE_DEPRECATION: // 'ignore_deprecation' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_DEPRECATION;
        return SUCCESS;
    case PRAGMA_IGNORE_EARLY_RETURN: // 'ignore_early_return' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_EARLY_RETURN;
        return SUCCESS;
    case PRAGMA_IGNORE_OBSOLETE: // 'ignore_obsolete' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_OBSOLETE;
        return SUCCESS;
    case PRAGMA_IGNORE_PARTIAL_SUPPORT: // 'ignore_partial_support' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_PARTIAL_SUPPORT;
        return SUCCESS;
    case PRAGMA_IGNORE_UNRECOGNIZED_DIRECTIVES: // 'ignore_unrecognized_directives' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_UNRECOGNIZED_DIRECTIVES;
        return SUCCESS;
    case PRAGMA_IGNORE_UNUSED: // 'ignore_unrecognized_directives' directive
        ctx->compiler->ignore |= COMPILER_IGNORE_UNUSED;
        return SUCCESS;
    case PRAGMA_LIBM: // 'libm' directive
        ctx->compiler->use_libm = true;
        return SUCCESS;
    case PRAGMA_MAC_ONLY: // 'mac_only' directive
        #if defined(__APPLE__) && TARGET_OS_MAC
        if(ctx->compiler->cross_compile_for != CROSS_COMPILE_NONE){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Mac");
            return FAILURE;
        }
        #else
        if(ctx->compiler->cross_compile_for != CROSS_COMPILE_MACOS){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Mac");
            return FAILURE;
        }
        #endif
        return SUCCESS;
    case PRAGMA_LINUX_ONLY: // 'linux_only' directive
        #ifdef __linux__
        if(ctx->compiler->cross_compile_for != CROSS_COMPILE_NONE){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Linux");
            return FAILURE;
        }
        #else
        compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Linux");
        return FAILURE;
        #endif
        return SUCCESS;
    case PRAGMA_NO_TYPE_INFO: // 'no_type_info' directive
        if(!(ctx->compiler->ignore & COMPILER_IGNORE_OBSOLETE)){
            if(compiler_warn(ctx->compiler, ctx->tokenlist->sources[*i], "WARNING: 'pragma no_type_info' is obsolete, use 'pragma no_typeinfo' instead"))
                return FAILURE;
        }
    case PRAGMA_NO_TYPEINFO: // 'no_typeinfo'  directive
        ctx->compiler->traits |= COMPILER_NO_TYPEINFO;
        return SUCCESS;
    case PRAGMA_NO_UNDEF: // 'no_undef' directive
        ctx->compiler->traits |= COMPILER_NO_UNDEF;
        return SUCCESS;
    case PRAGMA_NULL_CHECKS: // 'null_checks' directive
        ctx->compiler->checks |= COMPILER_NULL_CHECKS;
        return SUCCESS;
    case PRAGMA_OPTIMIZATION: // 'optimization' directive
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
    case PRAGMA_OPTIONS: // 'options' directive
        return parse_pragma_cloptions(ctx);
    case PRAGMA_PACKAGE: // 'package' directive
        if(ctx->compiler->traits & COMPILER_INFLATE_PACKAGE) return SUCCESS;
        if(compiler_create_package(ctx->compiler, ctx->object) == 0){
            ctx->compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        }
        return FAILURE;
    case PRAGMA_PROJECT_NAME: // 'project_name' directive
        read = parse_grab_string(ctx, "Expected string containing project name after 'pragma project_name'");
        if(read == NULL) return FAILURE;

        free(ctx->compiler->output_filename);
        ctx->compiler->output_filename = filename_local(ctx->object->filename, read);
        return SUCCESS;
    case PRAGMA_UNSAFE_META: // 'unsafe_meta' directive
        ctx->compiler->traits |= COMPILER_UNSAFE_META;
        return SUCCESS;
    case PRAGMA_UNSAFE_NEW: // 'unsafe_new' directive
        ctx->compiler->traits |= COMPILER_UNSAFE_NEW;
        return SUCCESS;
    case PRAGMA_UNSUPPORTED: // 'unsupported' directive
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
    case PRAGMA_WARN_AS_ERROR: // 'warn_as_error' directive
        ctx->compiler->traits |= COMPILER_WARN_AS_ERROR;
        return SUCCESS;
    case PRAGMA_WARN_SHORT: // 'warn_short' directive
        // fallthrough
    case PRAGMA_SHORT_WARNINGS: // 'short_warnings' directive
        ctx->compiler->traits |= COMPILER_SHORT_WARNINGS;
        return SUCCESS;
    case PRAGMA_WINDOWS_ONLY: // 'windows_only' directive
        #ifdef _WIN32
        if(ctx->compiler->cross_compile_for != CROSS_COMPILE_NONE){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Windows");
            return FAILURE;
        }
        #else
        if(ctx->compiler->cross_compile_for != CROSS_COMPILE_WINDOWS){
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "This file only works on Windows");
            return FAILURE;
        }
        #endif
        return SUCCESS;
    default:
        if(ctx->compiler->ignore & COMPILER_IGNORE_UNRECOGNIZED_DIRECTIVES){
            // Skip over the rest of the line
            while(tokens[*i].id != TOKEN_NEWLINE) (*i)++;
            (*i)--;
        } else {
            compiler_panicf(ctx->compiler, ctx->tokenlist->sources[*i], "Unrecognized pragma option '%s'", directive_string);
            return FAILURE;
        }
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
