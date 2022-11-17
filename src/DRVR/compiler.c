
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // IWYU pragma: keep
#elif defined(__APPLE__)
#include <mach-o/dyld.h> // IWYU pragma: keep
#include <unistd.h> // IWYU pragma: keep
#endif

#ifdef __linux__
#include <unistd.h> // IWYU pragma: keep
#include <linux/limits.h> // IWYU pragma: keep
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/TYPE/ast_type_identical.h"
#include "AST/UTIL/string_builder_extensions.h"
#include "AST/ast.h"
#include "AST/ast_dump.h"
#include "AST/ast_expr.h"
#include "AST/ast_poly_catalog.h"
#include "AST/ast_type.h"
#include "DRVR/compiler.h"
#include "DRVR/config.h"
#include "DRVR/object.h"
#include "LEX/lex.h"
#include "LEX/pkg.h"
#include "LEX/token.h"
#include "PARSE/parse.h"
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/string_list.h"
#include "UTIL/trait.h"
#include "UTIL/util.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "BKEND/backend.h"
#include "DBG/debug.h"
#include "INFER/infer.h"
#include "IR/ir_module.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_polymorphable.h"
#endif

#ifdef ADEPT_ENABLE_PACKAGE_MANAGER
#include "NET/stash.h"
#endif

errorcode_t compiler_run(compiler_t *compiler, int argc, char **argv){
    // A wrapper function around 'compiler_invoke'
    compiler_invoke(compiler, argc, argv);
    return compiler->result_flags & COMPILER_RESULT_SUCCESS ? SUCCESS : FAILURE;
}

void compiler_invoke(compiler_t *compiler, int argc, char **argv){
    object_t *object = compiler_new_object(compiler);
    compiler->result_flags = TRAIT_NONE;

    #ifdef _WIN32
	char *module_location = malloc(1024);
    GetModuleFileNameA(NULL, module_location, 1024);
	
	compiler->location = filename_absolute(module_location);
	free(module_location);
    #elif defined(__APPLE__)
    {
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0){
            compiler->location = filename_absolute(path);
        } else {
            redprintf("Executable path is too long!\n");
            return;
        }
    }
    #else

    compiler->location = NULL;

    #ifdef __linux__

    // Avoid using calloc so as not to interfere with TRACK_MEMORY_USAGE
    compiler->location = malloc(PATH_MAX + 1);
    memset(compiler->location, 0, PATH_MAX + 1);

    if(readlink("/proc/self/exe", compiler->location, PATH_MAX) < 0){
        // We failed to locate ourself using /proc/self/exe
        free(compiler->location);
        compiler->location = NULL;
    }
    #endif

    if(compiler->location == NULL){
        if(argv == NULL || argv[0] == NULL || streq(argv[0], "")){
            redprintf("external-error: ");
    		printf("Compiler was invoked with NULL or empty argv[0]\n");
    	} else {
            compiler->location = filename_absolute(argv[0]);
        }
    }

    if(compiler->location == NULL){
        internalerrorprintf("Compiler failed to locate itself\n");
        exit(1);
    }
    #endif

    compiler->root = filename_path(compiler->location);

    #ifdef _WIN32
    free(compiler->config.cainfo_file);
    compiler->config.cainfo_file = mallocandsprintf("%scurl-ca-bundle.crt", compiler->root);
    #endif

    #ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    {
        // Read persistent config file
        compiler->config_filename = mallocandsprintf("%sadept.config", compiler->root);
        weak_cstr_t config_warning = NULL;

        if(!config_read(&compiler->config, compiler->config_filename, &config_warning) && config_warning){
            yellowprintf("%s\n", config_warning);
        }
    }
    #endif

    if(handle_package_management(compiler, argc, argv)) return;
    if(parse_arguments(compiler, object, argc, argv)) return;

    #ifndef ADEPT_INSIGHT_BUILD
    debug_signal(compiler, DEBUG_SIGNAL_AT_STAGE_ARGS_AND_LEX, NULL);
    #endif

    // Compile / Package the code
    if(compiler_read_file(compiler, object)) return;

    if(compiler->traits & COMPILER_INFLATE_PACKAGE){
        // Inflate the package and exit
        #ifndef ADEPT_INSIGHT_BUILD
        debug_signal(compiler, DEBUG_SIGNAL_AT_STAGE_PARSE, NULL);
        #endif

        if(parse(compiler, object)) return;
        char *inflated_filename = filename_ext(object->filename, "idep");
        ast_dump(&object->ast, inflated_filename);
        free(inflated_filename);
        compiler->result_flags |= COMPILER_RESULT_SUCCESS;
        return;
    }

    #ifndef ADEPT_INSIGHT_BUILD
    debug_signal(compiler, DEBUG_SIGNAL_AT_STAGE_PARSE, NULL);
    #endif

    if(parse(compiler, object)) return;

    #ifndef ADEPT_INSIGHT_BUILD
    debug_signal(compiler, DEBUG_SIGNAL_AT_AST_DUMP, &object->ast);
    debug_signal(compiler, DEBUG_SIGNAL_AT_INFERENCE, NULL);

    if(infer(compiler, object)) return;

    debug_signal(compiler, DEBUG_SIGNAL_AT_INFER_DUMP, &object->ast);
    debug_signal(compiler, DEBUG_SIGNAL_AT_ASSEMBLY, NULL);

    if(ir_gen(compiler, object)) return;

    debug_signal(compiler, DEBUG_SIGNAL_AT_IR_MODULE_DUMP, &object->ir_module);
    debug_signal(compiler, DEBUG_SIGNAL_AT_EXPORT, NULL);
    
    if(ir_export(compiler, object, BACKEND_LLVM)) return;
    #endif

    compiler->result_flags |= COMPILER_RESULT_SUCCESS;
}

bool handle_package_management(compiler_t *compiler, int argc, char **argv){
    if(argc > 1 && streq(argv[1], "install")){
        #ifdef ADEPT_ENABLE_PACKAGE_MANAGER
            if(argc < 3){
                redprintf("Usage: ");
                printf("adept install <package name>\n");
                return true;
            }

            weak_cstr_t package_name = argv[2];
            adept_install(&compiler->config, "./", package_name);
        #else
            // Ignore unused
            (void) compiler;
            (void) argc;
            (void) argv;

            redprintf("Error:  Cannot perform package management\n");
            redprintf("Reason: Package manager was disabled when this compiler was built\n");
        #endif
        return true;
    }

    return false;
}

void compiler_init(compiler_t *compiler){
    compiler->location = NULL;
    compiler->root = NULL;
    compiler->objects = malloc(sizeof(object_t*) * 4);
    compiler->objects_length = 0;
    compiler->objects_capacity = 4;
    config_prepare(&compiler->config, NULL);
    compiler->config_filename = NULL;
    compiler->traits = TRAIT_NONE;
    compiler->ignore = TRAIT_NONE;
    compiler->output_filename = NULL;
    compiler->optimization = OPTIMIZATION_LESS;
    compiler->checks = TRAIT_NONE;

    #if __linux__
    compiler->use_pic = TROOLEAN_TRUE;
    #else
    compiler->use_pic = TROOLEAN_FALSE;
    #endif

    compiler->use_libm = TROOLEAN_FALSE;
    compiler->extract_import_order = false;

    #ifdef ENABLE_DEBUG_FEATURES
    compiler->debug_traits = TRAIT_NONE;
    #endif // ENABLE_DEBUG_FEATURES
    
    compiler->default_stdlib = NULL;
    compiler->error = NULL;
    compiler->warnings = NULL;
    compiler->warnings_length = 0;
    compiler->warnings_capacity = 0;
    compiler->show_unused_variables_how_to_disable = false;
    compiler->cross_compile_for = CROSS_COMPILE_NONE;
    compiler->entry_point = "main";
    string_builder_init(&compiler->user_linker_options);
    compiler->user_search_paths = (strong_cstr_list_t){0};
    compiler->windows_resources = (strong_cstr_list_t){0};

    // Allow '::' and ': Type' by default
    compiler->traits |= COMPILER_COLON_COLON | COMPILER_TYPE_COLON;
}

void compiler_free(compiler_t *compiler){
    #ifdef TRACK_MEMORY_USAGE
    memory_sort();
    #endif // TRACK_MEMORY_USAGE

    free(compiler->location);
    free(compiler->root);
    free(compiler->output_filename);
    string_builder_abandon(&compiler->user_linker_options);
    strong_cstr_list_free(&compiler->user_search_paths);
    strong_cstr_list_free(&compiler->windows_resources);

    compiler_free_objects(compiler);
    compiler_free_error(compiler);
    compiler_free_warnings(compiler);
    config_free(&compiler->config);
    free(compiler->config_filename);
}

void compiler_free_objects(compiler_t *compiler){
    for(length_t i = 0; i != compiler->objects_length; i++){
        object_t *object = compiler->objects[i];

        switch(object->compilation_stage){
        case COMPILATION_STAGE_IR_MODULE:
            #ifndef ADEPT_INSIGHT_BUILD
            ir_module_free(&object->ir_module);
            #endif
            // fallthrough
        case COMPILATION_STAGE_AST:
            ast_free(&object->ast);
            free(object->current_namespace);
            // fallthrough
        case COMPILATION_STAGE_TOKENLIST:
            free(object->buffer);
            tokenlist_free(&object->tokenlist);
            // fallthrough
        case COMPILATION_STAGE_FILENAME:
            free(object->filename);
            free(object->full_filename);
            // fallthrough
        case COMPILATION_STAGE_NONE:
            // Nothing to free up
            break;
        default:
            internalerrorprintf("Failed to delete object that has an invalid compilation stage\n");
        }

        free(object); // Free memory that the object is stored in
    }

    free(compiler->objects);
    
    compiler->objects = NULL;
    compiler->objects_length = 0;
    compiler->objects_capacity = 0;
}

void compiler_free_error(compiler_t *compiler){
    if(compiler->error){
        adept_error_free_fully(compiler->error);
        compiler->error = NULL;
    }
}

void compiler_free_warnings(compiler_t *compiler){
    adept_warnings_free_fully(compiler->warnings, compiler->warnings_length);

    compiler->warnings = NULL;
    compiler->warnings_length = 0;
    compiler->warnings_capacity = 0;
}

object_t *compiler_new_object(compiler_t *compiler){
    // NOTE: Returns pointer to object that the compiler itself will free when destroyed

    expand((void**) &compiler->objects, sizeof(object_t*), compiler->objects_length, &compiler->objects_capacity, 1, 4);
    length_t next_object_index = compiler->objects_length;

    object_t *object = malloc_init(object_t, {
        .filename = NULL,
        .full_filename = NULL,
        .compilation_stage = COMPILATION_STAGE_NONE,
        .index = next_object_index,
        .traits = OBJECT_NONE,
        .default_stdlib = NULL,
        .current_namespace = NULL,
        .current_namespace_length = 0,
    });

    compiler->objects[compiler->objects_length++] = object;
    return object;
}

errorcode_t parse_arguments(compiler_t *compiler, object_t *object, int argc, char **argv){
    int arg_index = 1;

    while(arg_index != argc){
        weak_cstr_t arg = argv[arg_index];

        if(arg[0] == '-'){
            if(streq(arg, "-h") || streq(arg, "--help")){
                show_help(false);
                compiler->result_flags |= COMPILER_RESULT_SUCCESS;
                return ALT_FAILURE;
            } else if(streq(arg, "--how-to-ignore-unused") || streq(arg, "--how-to-ignore-unused-variables")){
                show_how_to_ignore_unused_variables();
                compiler->result_flags |= COMPILER_RESULT_SUCCESS;
                return ALT_FAILURE;
            } else if(streq(arg, "-H") || streq(arg, "--help-advanced")){
                show_help(true);
                compiler->result_flags |= COMPILER_RESULT_SUCCESS;
                return ALT_FAILURE;
            } else if(streq(arg, "--update")){
                #ifndef ADEPT_INSIGHT_BUILD
                try_update_installation(&compiler->config, compiler->config_filename, NULL, NULL);
                #endif
                compiler->result_flags |= COMPILER_RESULT_SUCCESS;
                return ALT_FAILURE;
            } else if(streq(arg, "-p") || streq(arg, "--package")){
                compiler->traits |= COMPILER_MAKE_PACKAGE;
            } else if(streq(arg, "-o")){
                if(arg_index + 1 == argc){
                    redprintf("Expected output filename after '-o' flag\n");
                    return FAILURE;
                }
                free(compiler->output_filename);
                compiler->output_filename = strclone(argv[++arg_index]);
            } else if(streq(arg, "-n")){
                if(arg_index + 1 == argc){
                    redprintf("Expected output name after '-n' flag\n");
                    return FAILURE;
                }

                free(compiler->output_filename);
                compiler->output_filename = filename_local(object->filename, argv[++arg_index]);
            } else if(streq(arg, "-i") || streq(arg, "--inflate")){
                compiler->traits |= COMPILER_INFLATE_PACKAGE;
            } else if(streq(arg, "-d")){
                compiler->traits |= COMPILER_DEBUG_SYMBOLS;
            } else if(streq(arg, "-e")){
                compiler->traits |= COMPILER_EXECUTE_RESULT;
            } else if(streq(arg, "-w")){
                compiler->traits |= COMPILER_NO_WARN;
            } else if(streq(arg, "-Werror")){
                compiler->traits |= COMPILER_WARN_AS_ERROR;
            } else if(streq(arg, "-Wshort") || streq(arg, "--short-warnings")){
                compiler->traits |= COMPILER_SHORT_WARNINGS;
            } else if(streq(arg, "-j")){
                compiler->traits |= COMPILER_NO_REMOVE_OBJECT;
            } else if(streq(arg, "-c")){
                compiler->traits |= COMPILER_NO_REMOVE_OBJECT | COMPILER_EMIT_OBJECT;
            } else if(streq(arg, "-Onothing")){
                compiler->optimization = OPTIMIZATION_ABSOLUTELY_NOTHING;
            } else if(streq(arg, "-O0")){
                compiler->optimization = OPTIMIZATION_NONE;
            } else if(streq(arg, "-O1")){
                compiler->optimization = OPTIMIZATION_LESS;
            } else if(streq(arg, "-O2")){
                compiler->optimization = OPTIMIZATION_DEFAULT;
            } else if(streq(arg, "-O3")){
                compiler->optimization = OPTIMIZATION_AGGRESSIVE;
            } else if(streq(arg, "--fussy")){
                compiler->traits |= COMPILER_FUSSY;
            } else if(streq(arg, "-v") || streq(arg, "--version")){
                show_version(compiler);
                return FAILURE;
            } else if (streq(arg, "--root")){
                show_root(compiler);
                return FAILURE;
            } else if(streq(arg, "--no-undef")){
                compiler->traits |= COMPILER_NO_UNDEF;
            } else if(streq(arg, "--no-type-info") || streq(arg, "--no-typeinfo")){
                compiler->traits |= COMPILER_NO_TYPEINFO;
            } else if(streq(arg, "--unsafe-meta")){
                compiler->traits |= COMPILER_UNSAFE_META;
            } else if(streq(arg, "--unsafe-new")){
                compiler->traits |= COMPILER_UNSAFE_NEW;
            } else if(streq(arg, "--null-checks")){
                compiler->checks |= COMPILER_NULL_CHECKS;
            } else if(streq(arg, "--ignore-all")){
                compiler->ignore |= COMPILER_IGNORE_ALL;
            } else if(streq(arg, "--ignore-deprecation")){
                compiler->ignore |= COMPILER_IGNORE_DEPRECATION;
            } else if(streq(arg, "--ignore-early-return")){
                compiler->ignore |= COMPILER_IGNORE_EARLY_RETURN;
            } else if(streq(arg, "--ignore-obsolete")){
                compiler->ignore |= COMPILER_IGNORE_OBSOLETE;
            } else if(streq(arg, "--ignore-partial-support")){
                compiler->ignore |= COMPILER_IGNORE_PARTIAL_SUPPORT;
            } else if(streq(arg, "--ignore-unrecognized-directives")){
                compiler->ignore |= COMPILER_IGNORE_UNRECOGNIZED_DIRECTIVES;
            } else if(streq(arg, "--ignore-unused")){
                compiler->ignore |= COMPILER_IGNORE_UNUSED;
            } else if(streq(arg, "--pic")
                   || streq(arg, "-fPIC")
                   || streq(arg, "-fpic")){
                // Accessibility versions of --PIC
                warningprintf("Flag '%s' is not valid, assuming you meant to use --PIC\n", arg);
                compiler->use_pic = TROOLEAN_TRUE;
            } else if(streq(arg, "--PIC")){
                compiler->use_pic = TROOLEAN_TRUE;
            } else if(streq(arg, "--noPIC")
                   || streq(arg, "--no-pic")
                   || streq(arg, "--nopic")
                   || streq(arg, "-fno-pic")
                   || streq(arg, "-fno-PIC")){
                // Accessibility versions of --no-PIC
                warningprintf("Flag '%s' is not valid, assuming you meant to use --no-PIC\n", arg);
                compiler->use_pic = TROOLEAN_FALSE;
            } else if(streq(arg, "--no-PIC")){
                compiler->use_pic = TROOLEAN_FALSE;
            } else if(streq(arg, "-lm")){
                // Accessibility versions of --libm
                warningprintf("Flag '%s' is not valid, assuming you meant to use --libm\n", arg);
                compiler->use_libm = true;
            } else if(streq(arg, "--libm")){
                compiler->use_libm = true;
            } else if(streq(arg, "--extract-import-order")){
                compiler->extract_import_order = true; 
            } else if(strncmp(arg, "-std=", 5) == 0){
                compiler->default_stdlib = &arg[5];
                compiler->traits |= COMPILER_FORCE_STDLIB;
            } else if(strncmp(arg, "--std=", 6) == 0){
                compiler->default_stdlib = &arg[6];
                compiler->traits |= COMPILER_FORCE_STDLIB;
            } else if(streq(arg, "--windowed") || streq(arg, "-mwindows")){
                compiler->traits |= COMPILER_WINDOWED;
            } else if(streq(arg, "--entry")){
                if(arg_index + 1 == argc){
                    redprintf("Expected entry point after '--entry' flag\n");
                    return FAILURE;
                }
                compiler->entry_point = argv[++arg_index];
            } else if(streq(arg, "--windows")){
                #ifndef _WIN32
                printf("[-] Cross compiling for Windows x86_64\n");
                compiler->cross_compile_for = CROSS_COMPILE_WINDOWS;
                #endif
            } else if(streq(arg, "--macos")){
                #ifndef __APPLE__
                printf("[-] Cross compiling for MacOS x86_64\n");
                compiler->cross_compile_for = CROSS_COMPILE_MACOS;
                #endif
            } else if(streq(arg, "--wasm32")){
                printf("[-] Cross compiling for WebAssembly\n");
                printf("    (Adept is intended for true 64-bit architectures, some things may break!)\n");
                compiler->cross_compile_for = CROSS_COMPILE_WASM32;
            } else if(arg[0] == '-' && (arg[1] == 'L' || arg[1] == 'l')){
                // Forward argument to linker
                compiler_add_user_linker_option(compiler, arg);
            } else if(arg[0] == '-' && arg[1] == 'I'){
                if(arg[2] == '\0'){
                    if(arg_index + 1 == argc){
                        redprintf("Expected search path after '-I' flag\n");
                        return FAILURE;
                    }
                    compiler_add_user_search_path(compiler, argv[++arg_index], NULL);
                } else {
                    compiler_add_user_search_path(compiler, &arg[2], NULL);
                }
            }
            
            #ifdef ENABLE_DEBUG_FEATURES //////////////////////////////////
            else if(streq(arg, "--stages")){
                compiler->debug_traits |= COMPILER_DEBUG_STAGES;
            } else if(streq(arg, "--dump")){
                compiler->debug_traits |= COMPILER_DEBUG_DUMP;
            } else if(streq(arg, "--llvmir")){
                compiler->debug_traits |= COMPILER_DEBUG_LLVMIR;
            } else if(streq(arg, "--no-verification")){
                compiler->debug_traits |= COMPILER_DEBUG_NO_VERIFICATION;
            } else if(streq(arg, "--no-result")){
                compiler->debug_traits |= COMPILER_DEBUG_NO_RESULT;
            }

            #endif // ENABLE_DEBUG_FEATURES ///////////////////////////////

            else {
                redprintf("Invalid argument: %s\n", arg);
                return FAILURE;
            }
        } else if(object->filename == NULL){
            object->compilation_stage = COMPILATION_STAGE_FILENAME;
            object->filename = strclone(arg);

            if(strchr(object->filename, ' ') != NULL){
                redprintf("Filename cannot contain spaces! :\\\n");
                return FAILURE;
            }

            if(access(object->filename, F_OK) == -1){
                redprintf("Can't find file '%s'\n", object->filename);
                return FAILURE;
            }

            // Calculate the absolute path of the filename
            object->full_filename = filename_absolute(object->filename);

            if(object->full_filename == NULL){
                internalerrorprintf("Failed to get absolute path of filename '%s'\n", object->filename);
                return FAILURE;
            }
        } else {
            redprintf("Multiple filenames given\n");
            return FAILURE;
        }

        arg_index++;
    }

    if(object->filename == NULL){
        if(access("main.adept", F_OK) != -1) {
            // If no file was specified and the file 'main.adept' exists,
            // then assume we want to compile 'main.adept'
            object->compilation_stage = COMPILATION_STAGE_FILENAME;
            object->filename = strclone("main.adept");

            // Calculate the absolute path of the filename
            object->full_filename = filename_absolute(object->filename);

            if(object->full_filename == NULL){
                internalerrorprintf("Failed to get absolute path of filename '%s'\n", object->filename);
                return FAILURE;
            }
        } else {
            show_help(false);
            compiler->result_flags |= COMPILER_RESULT_SUCCESS;
            return ALT_FAILURE;
        }
    }

    return SUCCESS;
}

void break_into_arguments(const char *s, int *out_argc, char ***out_argv){
    // Breaks a string into arguments (quotes and backslashes allowed)
    // TODO: Clean up this function because it's all over the place

    bool in_quote = false;
    bool in_backslash = false;
    length_t argv_capacity = 2;

    (*out_argv) = malloc(sizeof(char*) * 2);
    (*out_argv)[0] = ""; // No program name argument
    (*out_argv)[1] = NULL; // No allocated memory for argument yet
    *out_argc = 1; // We use this to write to the newest argument string

    // Length and capacity of current argv string we're appending
    length_t argv_str_length = 0;
    length_t argv_str_capacity = 0;

    #define BREAK_INTO_NEW_ARGV_MACRO() { \
        /* Break into new argv */ \
        expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64); \
        (*out_argv)[*out_argc][argv_str_length++] = '\0'; \
        /* New argv array to work with */ \
        expand((void**) out_argv, sizeof(char*), ++(*out_argc), &argv_capacity, 1, 2); \
        (*out_argv)[*out_argc] = NULL; \
        argv_str_length = 0; \
        argv_str_capacity = 0; \
    }

    for(length_t i = 0; true; i++){
        switch(s[i]){
        case ' ':
            if(in_quote){
                expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64);
                (*out_argv)[*out_argc][argv_str_length++] = s[i];
            } else if((*out_argv)[*out_argc] != NULL){
                BREAK_INTO_NEW_ARGV_MACRO();
            }
            break;
        case '"':
            if(in_backslash){
                expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64);
                (*out_argv)[*out_argc][argv_str_length++] = '"';
                in_backslash = false;
            } else if(in_quote){
                BREAK_INTO_NEW_ARGV_MACRO();
                in_quote = false;
            } else {
                in_quote = true;
            }
            break;
        case '\\':
            if(in_backslash){
                expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64);
                (*out_argv)[*out_argc][argv_str_length++] = '\\';
                in_backslash = false;
            } else {
                in_backslash = true;
            }
            break;
        case '\0':
            if((*out_argv)[*out_argc] != NULL){
                expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64);
                (*out_argv)[(*out_argc)++][argv_str_length++] = '\0';
            }
            return;
        default:
            expand((void**) &((*out_argv)[*out_argc]), 1, argv_str_length, &argv_str_capacity, 1, 64);
            (*out_argv)[*out_argc][argv_str_length++] = s[i];
            (*out_argv)[*out_argc][argv_str_length] = '\0';
        }
    }
}

void show_help(bool show_advanced_options){
    terminal_set_color(TERMINAL_COLOR_LIGHT_BLUE);
    printf("     /▔▔\\\n");
    printf("    /    \\    \n");
    printf("   /      \\    \n");
    printf("  /   /\\   \\        The Adept Compiler v%s - (c) 2016-2022 Isaac Shelton\n", ADEPT_VERSION_STRING);
    printf(" /   /\\__   \\\n");
    printf("/___/    \\___\\\n\n");
    terminal_set_color(TERMINAL_COLOR_DEFAULT);

    printf("Usage: adept [options] [filename]\n\n");
    printf("Options:\n");
    printf("    -h, --help        Display this message\n");
    printf("    -e                Execute resulting executable\n");
    printf("    -w                Disable compiler warnings\n");

    if(show_advanced_options){
        printf("    -Werror           Turn warnings into errors\n");
        printf("    --short-warnings  Don't show code fragments for warnings\n");
    }

    printf("    -o FILENAME       Output to FILENAME (relative to working directory)\n");
    printf("    -n FILENAME       Output to FILENAME (relative to file)\n");

    if(show_advanced_options){
        printf("    -p, --package     Output a package [OBSOLETE]\n");
        printf("    -d                Include debugging symbols [UNIMPLEMENTED]\n");
    }

    printf("    -c                Emit object file\n");
    if(show_advanced_options){
        printf("    -j                Preserve generated object file\n");
        printf("    -I<PATH>          Add directory to import search path\n");
        printf("    -L<PATH>          Add directory to native library search path\n");
        printf("    -l<LIBRARY>       Link against native library\n");
    }
    
    printf("    -O0,-O1,-O2,-O3   Set optimization level\n");
    printf("    --windowed        Don't open console with executable (only applies to Windows)\n");
    printf("    -std=2.x          Set standard library version\n");
    
    if(show_advanced_options)
        printf("    --fussy           Show insignificant warnings\n");

    printf("    --version         Display compiler version\n");
    printf("    --root            Display root folder\n");
    printf("    --help-advanced   Show lesser used compiler flags\n");

    if(show_advanced_options){
        printf("\nLanguage Options:\n");
        printf("    --no-type-info    Disable runtime type information\n");
        printf("    --no-undef        Force initialize for 'undef'\n");
        printf("    --unsafe-meta     Allow unsafe usage of meta constructs\n");
        printf("    --unsafe-new      Disables zero-initialization of memory allocated with new\n");
        printf("    --null-checks     Enable runtime null-checks\n");
        printf("    --entry           Set the entry point of the program\n");

        printf("\nMachine Code Options:\n");
        printf("    --PIC             Forces PIC relocation model\n");
        printf("    --no-PIC          Forbids PIC relocation model\n");

        printf("\nLinker Options:\n");
        printf("    --libm            Forces linking against libc math library\n");

        printf("\nCross Compilation:\n");
        printf("    --windows         Output Windows Executable (Requires Extension)\n");
        printf("    --macos           Output MacOS Mach-O Object File\n");
        
        printf("\nIgnore Options:\n");
        printf("    --ignore-all                      Enables all ignore options\n");
        printf("    --ignore-deprecation              Ignore deprecation warnings\n");
        printf("    --ignore-early-return             Ignore early return warnings\n");
        printf("    --ignore-obsolete                 Ignore obsolete feature warnings\n");
        printf("    --ignore-partial-support          Ignore partial compiler support warnings\n");
        printf("    --ignore-unrecognized-directives  Ignore unrecognized pragma directives\n");
        printf("    --ignore-unused                   Ignore unused variables\n");
    }

    #ifdef ENABLE_DEBUG_FEATURES
    printf("--------------------------------------------------\n");
    printf("Debug Exclusive Options:\n");
    printf("    --stages          Announce major compilation stages\n");
    printf("    --dump            Dump AST, IAST, & IR to files\n");
    printf("    --llvmir          Show generated LLVM representation\n");
    printf("    --no-verification Don't verify backend output\n");
    printf("    --no-result       Don't create final binary\n");
    #endif // ENABLE_DEBUG_FEATURES
}

void show_how_to_ignore_unused_variables(){
    blueprintf("Choose one or more options to ignore unused variables:\n");
    printf("  A.) prefix the variable with '_'              [single variable]\n");
    printf("  B.) add 'pragma ignore_unused' to your file   [entire project]\n");
    printf("  C.) pass '--ignore-unused' option             [entire project]\n");
}

void show_version(compiler_t *compiler){
    strong_cstr_t compiler_string = compiler_get_string();
    strong_cstr_t import_location = filename_adept_import(compiler->root, "");
    strong_cstr_t stdlib_location = filename_adept_import(compiler->root, ADEPT_VERSION_STRING);

    #ifdef _WIN32
    weak_cstr_t platform = "Windows";
    #elif defined(__APPLE__)
    weak_cstr_t platform = "MacOS";
    #elif defined(__linux__) || defined(__linux) || defined(linux)
    weak_cstr_t platform = "Linux";
    #else
    weak_cstr_t platform = "Other Unix";
    #endif

    #ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    bool package_manager_enabled = true;
    #else
    bool package_manager_enabled = false;
    #endif

    printf("Platform:     \t%s\n\n", platform);
    printf("Adept Build:\t%s\n", compiler_string);
    printf("Adept Version:\t%s\n", ADEPT_VERSION_STRING);
    printf("Pkg Manager:\t%s\n\n", package_manager_enabled ? "enabled" : "disabled");

    // Make more human readable on Windows
    #ifdef _WIN32
    for(char *c = import_location; *c != '\0'; c++)
        if(*c == '/') *c = '\\';
    
    for(char *c = stdlib_location; *c != '\0'; c++)
        if(*c == '/') *c = '\\';
    #endif
    
    printf("Import Folder:\t\"%s\"\n", import_location);
    printf("Stblib Folder:\t\"%s\"\n", stdlib_location);

    // Don't put an additional newline if on Windows
    #ifndef _WIN32
    putchar('\n');
    #endif

    free(compiler_string);
    free(import_location);
    free(stdlib_location);
}

void show_root(compiler_t *compiler){
    printf("%s\n", compiler->root);
}

strong_cstr_t compiler_get_string(void){
    return mallocandsprintf("Adept %s - Built %s %s", ADEPT_VERSION_STRING, __DATE__, __TIME__);
}

void compiler_add_user_linker_option(compiler_t *compiler, weak_cstr_t option){
    string_builder_append_char(&compiler->user_linker_options, ' ');
    string_builder_append(&compiler->user_linker_options, option);
}

void compiler_add_user_search_path(compiler_t *compiler, weak_cstr_t search_path, maybe_null_weak_cstr_t current_file){
    if(current_file != NULL){
        // Add file relative path
        strong_cstr_t current_path = filename_path(current_file);
        cstr_list_append(&compiler->user_search_paths, mallocandsprintf("%s%s", current_path, search_path));
        free(current_path);
    }

    // Add absolute / cwd relative path
    cstr_list_append(&compiler->user_search_paths, strclone(search_path));
}

errorcode_t compiler_create_package(compiler_t *compiler, object_t *object){
    char *package_filename;

    if(compiler->output_filename != NULL){
        filename_auto_ext(&compiler->output_filename, compiler->cross_compile_for, FILENAME_AUTO_PACKAGE);
        package_filename = compiler->output_filename;
    } else {
        package_filename = filename_ext(object->filename, "dep");
    }

    if(pkg_write(package_filename, &object->tokenlist)){
        object_panic_plain(object, "Failed to export to package");
        if(compiler->output_filename == NULL) free(package_filename);
        return FAILURE;
    }

    if(compiler->output_filename == NULL) free(package_filename);
    return SUCCESS;
}

errorcode_t compiler_read_file(compiler_t *compiler, object_t *object){
    length_t filename_length = strlen(object->filename);

    if(filename_length >= 4 && streq(&object->filename[filename_length - 4], ".dep")){
        return pkg_read(object);
    } else {
        return lex(compiler, object);
    }
}

strong_cstr_t compiler_get_stdlib(compiler_t *compiler, object_t *optional_object){
    // Find which standard library to use
    maybe_null_weak_cstr_t standard_library_folder = optional_object ? optional_object->default_stdlib : NULL;

    if(standard_library_folder == NULL) standard_library_folder = compiler->default_stdlib;
    if(standard_library_folder == NULL) standard_library_folder = ADEPT_VERSION_STRING;
    
    length_t length = strlen(standard_library_folder);
    char final_character = length == 0 ? 0x00 : standard_library_folder[length - 1];
    if(final_character == '/' || final_character == '\\') return strclone(standard_library_folder);

    // Otherwise, we need to append '/'
    return mallocandsprintf("%s/", standard_library_folder);
}

void compiler_print_source(compiler_t *compiler, int line, source_t source){
    object_t *relevant_object = compiler->objects[source.object_index];
    if(relevant_object->buffer == NULL || relevant_object->traits & OBJECT_PACKAGE) return;

    if(SOURCE_IS_NULL(source)){
        printf("  N/A| (no source code)\n");
        printf("\n");
        return;
    }

    length_t line_index = 0;
    for(int current_line = 1; current_line != line; line_index++){
        if(relevant_object->buffer[line_index] == '\n') current_line++;
    }

    char prefix[128];
    sprintf(prefix, "  %d| ", line);
    length_t prefix_length = strlen(prefix);
    printf("%s", prefix);

    while(relevant_object->buffer[line_index] == '\t'){
        printf("    "); line_index++; prefix_length += 4;
    }

    length_t line_length = 0;
    while(true){
        if(relevant_object->buffer[line_index + line_length] == '\n') break;
        line_length++;
    }

    char *line_text = malloc(line_length + 1);
    line_text = memcpy(line_text, &relevant_object->buffer[line_index], line_length);
    line_text[line_length] = '\0';
    printf("%s\n", line_text);
    free(line_text);

    for(length_t i = 0; i != prefix_length; i++) printf(" ");
    for(length_t i = line_index; i != source.index; i++) printf(relevant_object->buffer[i] != '\t' ? " " : "    ");

    for(length_t i = 0; i != source.stride; i++)
        whiteprintf("^");
    printf("\n");
}

void compiler_panic(compiler_t *compiler, source_t source, const char *message){
    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;

    if(message == NULL){
        if(relevant_object->traits & OBJECT_PACKAGE){
            printf("%s:?:?:", filename_name_const(relevant_object->filename));
            redprintf(" error:\n");
        } else {
            lex_get_location(relevant_object->buffer, source.index, &line, &column);
            printf("%s:%d:%d:", filename_name_const(relevant_object->filename), line, column);
            redprintf(" error:\n");
            compiler_print_source(compiler, line, source);
        }
        return;
    }

    if(relevant_object->traits & OBJECT_PACKAGE){
        printf("%s:?:?: ", filename_name_const(relevant_object->filename));
        redprintf("error: ");
        printf("%s\n", message);
    } else {
        lex_get_location(relevant_object->buffer, source.index, &line, &column);
        printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
        redprintf("error: ");
        printf("%s\n", message);
        compiler_print_source(compiler, line, source);
    }
    #endif // !ADEPT_INSIGHT_BUILD

    if(compiler->error == NULL){
        compiler->error = adept_error_create(strclone(message), source);
    }
}

void compiler_panicf(compiler_t *compiler, source_t source, const char *format, ...){
    va_list args;
    va_start(args, format);
    compiler_vpanicf(compiler, source, format, args);
    va_end(args);
}

void compiler_vpanicf(compiler_t *compiler, source_t source, const char *format, va_list args){
    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;
    #endif // !ADEPT_INSIGHT_BUILD
    
    va_list error_format_args;
    va_copy(error_format_args, args);

    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)

    if(format == NULL){
        if(relevant_object->traits & OBJECT_PACKAGE){
            printf("%s:?:?: ", filename_name_const(relevant_object->filename));
            redprintf("error: \n");
        } else {
            lex_get_location(relevant_object->buffer, source.index, &line, &column);
            printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
            redprintf("error: \n");
            compiler_print_source(compiler, line, source);
        }
        return;
    }

    if(relevant_object->traits & OBJECT_PACKAGE){
        line = 1;
        column = 1;
        printf("%s:?:?: ", filename_name_const(relevant_object->filename));
    } else {
        lex_get_location(relevant_object->buffer, source.index, &line, &column);
        printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
    }

    redprintf("error: ");
    vprintf(format, args);
    printf("\n");

    compiler_print_source(compiler, line, source);
    #endif // !ADEPT_INSIGHT_BUILD

    if(compiler->error == NULL){
        strong_cstr_t buffer = calloc(1024, 1);
        vsnprintf(buffer, 1024, format, error_format_args);
        compiler->error = adept_error_create(buffer, source);
    }

    va_end(error_format_args);
}

bool compiler_warn(compiler_t *compiler, source_t source, const char *message){
    // Returns whether program should exit

    if(compiler->traits & COMPILER_NO_WARN) return false;

    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)
    if(compiler->traits & COMPILER_WARN_AS_ERROR){
        compiler_panic(compiler, source, message);
        return true;
    }
    
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;
    lex_get_location(relevant_object->buffer, source.index, &line, &column);
    printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
    yellowprintf("warning: ");
    printf("%s\n", message);

    if(!(compiler->traits & COMPILER_SHORT_WARNINGS)){
        compiler_print_source(compiler, line, source);
    }
    #endif

    compiler_create_warning(compiler, strclone(message), source);
    return false;
}

bool compiler_warnf(compiler_t *compiler, source_t source, const char *format, ...){
    if(compiler->traits & COMPILER_NO_WARN) return false;
    
    va_list args;
    va_start(args, format);

    if(compiler->traits & COMPILER_WARN_AS_ERROR){
        compiler_vpanicf(compiler, source, format, args);
        va_end(args);
        return true;
    } else {
        compiler_vwarnf(compiler, source, format, args);
    }

    va_end(args);
    return false;
}

void compiler_vwarnf(compiler_t *compiler, source_t source, const char *format, va_list args){
    if(compiler->traits & COMPILER_NO_WARN) return;

    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;
    #endif

    va_list warning_format_args;
    va_copy(warning_format_args, args);
    
    #if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)

    if(relevant_object->traits & OBJECT_PACKAGE){
        line = 1;
        column = 1;
        printf("%s:?:?: ", filename_name_const(relevant_object->filename));
    } else {
        lex_get_location(relevant_object->buffer, source.index, &line, &column);
        printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
    }

    yellowprintf("warning: ");
    vprintf(format, args);
    printf("\n");

    if(!(compiler->traits & COMPILER_SHORT_WARNINGS)){
        compiler_print_source(compiler, line, source);
    }
    #endif

    strong_cstr_t buffer = calloc(512, 1);
    vsnprintf(buffer, 512, format, warning_format_args);
    compiler_create_warning(compiler, buffer, source);
    va_end(warning_format_args);
}

#if !defined(ADEPT_INSIGHT_BUILD) || defined(__EMSCRIPTEN__)
void compiler_undeclared_function(compiler_t *compiler, object_t *object, source_t source,
        weak_cstr_t name, ast_type_t *types, length_t arity, ast_type_t *gives, bool is_method){
    
    // Allow for '.elements_length' to be zero to indicate no return matching
    if(gives && gives->elements_length == 0) gives = NULL;

    ast_t *ast = &object->ast;
    ast_type_t *type_of_this = is_method ? &types[0] : NULL;
    func_id_list_t possibilities = compiler_possibilities(compiler, object, name, type_of_this);

    if(possibilities.length == 0){
        // No other function with that name exists
        if(is_method){
            ast_type_t dereferenced_view = ast_type_dereferenced_view(type_of_this);

            strong_cstr_t subject = ast_type_str(&dereferenced_view);
            compiler_panicf(compiler, source, "Undeclared method '%s' for type '%s'", name, subject);
            free(subject);
        } else {
            compiler_panicf(compiler, source, "Undeclared function '%s'", name);
        }
        goto success;
    } else {
        // Other functions have the same name

        strong_cstr_t args_string;
        
        if(is_method){
            args_string = strong_cstr_empty_if_null(make_args_string(&types[1], NULL, arity - 1, TRAIT_NONE));
        } else {
            args_string = strong_cstr_empty_if_null(make_args_string(types, NULL, arity, TRAIT_NONE));
        }

        strong_cstr_t gives_string = gives ? ast_type_str(gives) : NULL;

        if(is_method){
            ast_type_t dereferenced_view = ast_type_dereferenced_view(type_of_this);

            strong_cstr_t subject = ast_type_str(&dereferenced_view);
            compiler_panicf(compiler, source, "Undeclared method %s(%s)%s%s for type %s", name, args_string, gives ? " ~> " : "", gives ? gives_string : "", subject);
            free(subject);

        } else {
            compiler_panicf(compiler, source, "Undeclared function %s(%s)%s%s", name, args_string, gives ? " ~> " : "", gives ? gives_string : "");
        }

        free(gives_string);
        free(args_string);

        printf("Potential Candidates:\n");
    }

    for(length_t i = 0; i != possibilities.length; i++){
        print_candidate(&ast->funcs[possibilities.ids[i]]);
    }

success:
    func_id_list_free(&possibilities);
}

// TODO: Refactor/move
// Returns SUCCESS if potential_subject is possible
// Returns FAILURE if potential_subject is not possible
// Returns ALT_FAILURE on serious failure
static errorcode_t method_subject_is_possible(compiler_t *compiler, object_t *object, ast_type_t *subject, ast_type_t *potential_subject){
    if(ast_type_has_polymorph(potential_subject)){
        #ifdef ADEPT_INSIGHT_BUILD
            return SUCCESS;
        #else
            ast_poly_catalog_t catalog;
            ast_poly_catalog_init(&catalog);
            errorcode_t res = ir_gen_polymorphable(compiler, object, NULL, subject, potential_subject, &catalog, false);
            ast_poly_catalog_free(&catalog);
            return res;
        #endif
    }

    return ast_types_identical(potential_subject, subject) ? SUCCESS : FAILURE;
}

func_id_list_t compiler_possibilities(compiler_t *compiler, object_t *object, weak_cstr_t name, ast_type_t *methods_only_type_of_this){
    ast_t *ast = &object->ast;
    func_id_list_t list = {0};

    for(length_t id = 0; id != ast->funcs_length; id++){
        ast_func_t *func = &ast->funcs[id];

        if(streq(func->name, name) && (func->traits & (AST_FUNC_VIRTUAL | AST_FUNC_OVERRIDE | AST_FUNC_NO_SUGGEST)) == TRAIT_NONE){
            if(methods_only_type_of_this){
                if(!ast_func_is_method(func)) continue;

                errorcode_t errorcode = method_subject_is_possible(compiler, object, methods_only_type_of_this, &func->arg_types[0]);
                if(errorcode == ALT_FAILURE) break;
                if(errorcode == FAILURE) continue;
            }
            func_id_list_append(&list, id);
        }
    }

    return list;
}
#endif

void print_candidate(ast_func_t *ast_func){
    strong_cstr_t return_type_string = ast_type_str(&ast_func->return_type);
    strong_cstr_t args_string = make_args_string(ast_func->arg_types, ast_func->arg_defaults, ast_func->arity, ast_func->traits);
    printf("    %s(%s) %s\n", ast_func->name, args_string ? args_string : "", return_type_string);
    free(args_string);
    free(return_type_string);
}

strong_cstr_t make_args_string(ast_type_t *types, ast_expr_t **defaults, length_t arity, trait_t traits){
    string_builder_t builder;
    string_builder_init(&builder);

    for(length_t i = 0; i != arity; i++){
        string_builder_append_type(&builder, &types[i]);

        if(defaults && defaults[i]){
            string_builder_append(&builder, " = ");
            string_builder_append_expr(&builder, defaults[i]);
        }

        if(i + 1 != arity || traits & (AST_FUNC_VARARG | AST_FUNC_VARIADIC)){
            string_builder_append(&builder, ", ");
        }
    }

    if(traits & AST_FUNC_VARARG){
        string_builder_append(&builder, "...");
    } else if(traits & AST_FUNC_VARIADIC){
        string_builder_append(&builder, "..");
    }

    return strong_cstr_empty_if_null(string_builder_finalize(&builder));
}

void object_panic_plain(object_t *object, const char *message){
    #ifndef ADEPT_INSIGHT_BUILD
    redprintf("%s: %s\n", filename_name_const(object->filename), message);
    #endif
}

void object_panicf_plain(object_t *object, const char *format, ...){
    #ifndef ADEPT_INSIGHT_BUILD
    va_list args;

    va_start(args, format);
    terminal_set_color(TERMINAL_COLOR_RED);

    printf("%s: ", filename_name_const(object->filename));

    vprintf(format, args);
    printf("\n");
    terminal_set_color(TERMINAL_COLOR_DEFAULT);

    va_end(args);
    #endif
}

adept_error_t *adept_error_create(strong_cstr_t message, source_t source){
    adept_error_t *error = malloc(sizeof(adept_error_t));
    error->message = message;
    error->source = source;
    return error;
}

void adept_error_free_fully(adept_error_t *error){
    free(error->message);
    free(error);
}

void compiler_create_warning(compiler_t *compiler, strong_cstr_t message, source_t source){
    expand((void**) &compiler->warnings, sizeof(adept_warning_t), compiler->warnings_length, &compiler->warnings_capacity, 1, 4);
    
    adept_warning_t *warning = &compiler->warnings[compiler->warnings_length++];
    warning->message = message;
    warning->source = source;
}

void adept_warnings_free_fully(adept_warning_t *warnings, length_t length){
    for(length_t i = 0; i != length; i++) free(warnings[i].message);
    free(warnings);
}

weak_cstr_t compiler_unnamespaced_name(weak_cstr_t input){
    length_t i = 0;
    length_t beginning = 0;

    while(true){
        char tmp = input[i++];
        if(tmp == '\0') return &input[beginning];
        if(tmp == '\\') beginning = i;
    }

    return NULL; // [unreachable]
}
