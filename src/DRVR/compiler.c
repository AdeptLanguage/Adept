
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <linux/limits.h>
#endif

#include <stdarg.h>

#include "LEX/lex.h"
#include "LEX/pkg.h"
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "DRVR/compiler.h"
#include "PARSE/parse.h"

#ifndef ADEPT_INSIGHT_BUILD
#include "IR/ir.h"
#include "DRVR/debug.h"
#include "INFER/infer.h"
#include "IRGEN/ir_gen.h"
#include "IRGEN/ir_gen_find.h"
#include "BKEND/backend.h"
#endif

errorcode_t compiler_run(compiler_t *compiler, int argc, char **argv){
    // A wrapper function around 'compiler_invoke'
    compiler_invoke(compiler, argc, argv);
    return !(compiler->result_flags & COMPILER_RESULT_SUCCESS);
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
        if(argv == NULL || argv[0] == NULL || strcmp(argv[0], "") == 0){
    		redprintf("EXTERNAL ERROR: Compiler was invoked with NULL or empty argv[0]\n");
    	} else {
            compiler->location = filename_absolute(argv[0]);
        }
    }

    if(compiler->location == NULL){
        redprintf("INTERNAL ERROR: Compiler failed to locate itself\n");
        exit(1);
    }
    #endif

    compiler->root = filename_path(compiler->location);

    #ifdef ADEPT_ENABLE_PACKAGE_MANAGER
    {
        // Read persistent config file
        strong_cstr_t config_filename = mallocandsprintf("%sadept.config", compiler->root);
        weak_cstr_t config_warning = NULL;

        if(!config_read(&compiler->config, config_filename, &config_warning) && config_warning){
            yellowprintf("%s\n", config_warning);
        }

        free(config_filename);
    }
    #endif

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

void compiler_init(compiler_t *compiler){
    compiler->location = NULL;
    compiler->objects = malloc(sizeof(object_t*) * 4);
    compiler->objects_length = 0;
    compiler->objects_capacity = 4;
    config_prepare(&compiler->config);
    compiler->traits = TRAIT_NONE;
    compiler->ignore = TRAIT_NONE;
    compiler->output_filename = NULL;
    compiler->optimization = OPTIMIZATION_NONE;
    compiler->checks = TRAIT_NONE;

    #if __linux__
    compiler->use_pic = TROOLEAN_TRUE;
    #else
    compiler->use_pic = TROOLEAN_FALSE;
    #endif

    compiler->use_libm = TROOLEAN_FALSE;

    #ifdef ENABLE_DEBUG_FEATURES
    compiler->debug_traits = TRAIT_NONE;
    #endif // ENABLE_DEBUG_FEATURES
    
    compiler->default_stblib = NULL;
    compiler->error = NULL;
}

void compiler_free(compiler_t *compiler){
    #ifdef TRACK_MEMORY_USAGE
    memory_sort();
    #endif // TRACK_MEMORY_USAGE

    free(compiler->location);
    free(compiler->root);
    free(compiler->output_filename);

    for(length_t i = 0; i != compiler->objects_length; i++){
        object_t *object = compiler->objects[i];

        switch(object->compilation_stage){
        case COMPILATION_STAGE_IR_MODULE:
            #ifndef ADEPT_INSIGHT_BUILD
            ir_module_free(&object->ir_module);
            #endif
            // [fallthrough]
        case COMPILATION_STAGE_AST:    
            ast_free(&object->ast);
            // [fallthrough]
        case COMPILATION_STAGE_TOKENLIST:
            free(object->buffer);
            tokenlist_free(&object->tokenlist);
            // [fallthrough]
        case COMPILATION_STAGE_FILENAME:
            free(object->filename);
            free(object->full_filename);
            // [fallthrough]
        case COMPILATION_STAGE_NONE:
            // Nothing to free up
            break;
        default:
            printf("INTERNAL ERROR: Failed to delete object that has an invalid compilation stage\n");
        }

        free(object); // Free memory that the object is stored in
    }

    if(compiler->error) adept_error_free_fully(compiler->error);
    free(compiler->objects);
    config_free(&compiler->config);
}

object_t* compiler_new_object(compiler_t *compiler){
    // NOTE: This process needs to be made thread-safe eventually
    // NOTE: Returns pointer to object that the compiler will free

    if(compiler->objects_length == compiler->objects_capacity){
        object_t **new_objects = malloc(sizeof(object_t*) * compiler->objects_length * 2);
        memcpy(new_objects, compiler->objects, sizeof(object_t*) * compiler->objects_length);
        free(compiler->objects);
        compiler->objects = new_objects;
        compiler->objects_capacity *= 2;
    }

    object_t **object_reference = &compiler->objects[compiler->objects_length];
    (*object_reference) = malloc(sizeof(object_t));
    (*object_reference)->filename = NULL;
    (*object_reference)->full_filename = NULL;
    (*object_reference)->compilation_stage = COMPILATION_STAGE_NONE;
    (*object_reference)->index = compiler->objects_length++;
    (*object_reference)->traits = OBJECT_NONE;
    (*object_reference)->default_stblib = NULL;
    return *object_reference;
}

errorcode_t parse_arguments(compiler_t *compiler, object_t *object, int argc, char **argv){
    int arg_index = 1;

    while(arg_index != argc){
        if(argv[arg_index][0] == '-'){
            if(strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0){
                show_help(false);
                return FAILURE;
            } else if(strcmp(argv[arg_index], "-H") == 0 || strcmp(argv[arg_index], "--help-advanced") == 0){
                show_help(true);
                return FAILURE;
            } else if(strcmp(argv[arg_index], "-p") == 0 || strcmp(argv[arg_index], "--package") == 0){
                compiler->traits |= COMPILER_MAKE_PACKAGE;
            } else if(strcmp(argv[arg_index], "-o") == 0){
                if(arg_index + 1 == argc){
                    redprintf("Expected output filename after '-o' flag\n");
                    return FAILURE;
                }
                free(compiler->output_filename);
                compiler->output_filename = strclone(argv[++arg_index]);
            } else if(strcmp(argv[arg_index], "-n") == 0){
                if(arg_index + 1 == argc){
                    redprintf("Expected output name after '-n' flag\n");
                    return FAILURE;
                }

                free(compiler->output_filename);
                compiler->output_filename = filename_local(object->filename, argv[++arg_index]);
            } else if(strcmp(argv[arg_index], "-i") == 0 || strcmp(argv[arg_index], "--inflate") == 0){
                compiler->traits |= COMPILER_INFLATE_PACKAGE;
            } else if(strcmp(argv[arg_index], "-d") == 0){
                compiler->traits |= COMPILER_DEBUG_SYMBOLS;
            } else if(strcmp(argv[arg_index], "-e") == 0){
                compiler->traits |= COMPILER_EXECUTE_RESULT;
            } else if(strcmp(argv[arg_index], "-w") == 0){
                compiler->traits |= COMPILER_NO_WARN;
            } else if(strcmp(argv[arg_index], "-j") == 0){
                compiler->traits |= COMPILER_NO_REMOVE_OBJECT;
            } else if(strcmp(argv[arg_index], "-O0") == 0){
                compiler->optimization = OPTIMIZATION_NONE;
            } else if(strcmp(argv[arg_index], "-O1") == 0){
                compiler->optimization = OPTIMIZATION_LESS;
            } else if(strcmp(argv[arg_index], "-O2") == 0){
                compiler->optimization = OPTIMIZATION_DEFAULT;
            } else if(strcmp(argv[arg_index], "-O3") == 0){
                compiler->optimization = OPTIMIZATION_AGGRESSIVE;
            } else if(strcmp(argv[arg_index], "--fussy") == 0){
                compiler->traits |= COMPILER_FUSSY;
            } else if(strcmp(argv[arg_index], "-v") == 0 || strcmp(argv[arg_index], "--version") == 0){
                show_version(compiler);
                return FAILURE;
            } else if(strcmp(argv[arg_index], "--no-undef") == 0){
                compiler->traits |= COMPILER_NO_UNDEF;
            } else if(strcmp(argv[arg_index], "--no-type-info") == 0 || strcmp(argv[arg_index], "--no-typeinfo") == 0){
                compiler->traits |= COMPILER_NO_TYPEINFO;
            } else if(strcmp(argv[arg_index], "--unsafe-meta") == 0){
                compiler->traits |= COMPILER_UNSAFE_META;
            } else if(strcmp(argv[arg_index], "--unsafe-new") == 0){
                compiler->traits |= COMPILER_UNSAFE_NEW;
            } else if(strcmp(argv[arg_index], "--null-checks") == 0){
                compiler->checks |= COMPILER_NULL_CHECKS;
            } else if(strcmp(argv[arg_index], "--ignore-all") == 0){
                compiler->ignore |= COMPILER_IGNORE_ALL;
            } else if(strcmp(argv[arg_index], "--ignore-deprecation") == 0){
                compiler->ignore |= COMPILER_IGNORE_DEPRECATION;
            } else if(strcmp(argv[arg_index], "--ignore-early-return") == 0){
                compiler->ignore |= COMPILER_IGNORE_EARLY_RETURN;
            } else if(strcmp(argv[arg_index], "--ignore-obsolete") == 0){
                compiler->ignore |= COMPILER_IGNORE_OBSOLETE;
            } else if(strcmp(argv[arg_index], "--ignore-partial-support") == 0){
                compiler->ignore |= COMPILER_IGNORE_PARTIAL_SUPPORT;
            } else if(strcmp(argv[arg_index], "--ignore-unrecognized-directives") == 0){
                compiler->ignore |= COMPILER_IGNORE_UNRECOGNIZED_DIRECTIVES;
            } else if(strcmp(argv[arg_index], "--pic") == 0 || strcmp(argv[arg_index], "-fPIC") == 0 ||
                        strcmp(argv[arg_index], "-fpic") == 0){
                // Accessibility versions of --PIC
                yellowprintf("WARNING: Flag '%s' is not valid, assuming you meant to use --PIC\n", argv[arg_index]);
                compiler->use_pic = TROOLEAN_TRUE;
            } else if(strcmp(argv[arg_index], "--PIC") == 0){
                compiler->use_pic = TROOLEAN_TRUE;
            } else if(strcmp(argv[arg_index], "--noPIC") == 0 || strcmp(argv[arg_index], "--no-pic") == 0 ||
                        strcmp(argv[arg_index], "--nopic") == 0 || strcmp(argv[arg_index], "-fno-pic") == 0 ||
                        strcmp(argv[arg_index], "-fno-PIC") == 0){
                // Accessibility versions of --no-PIC
                yellowprintf("WARNING: Flag '%s' is not valid, assuming you meant to use --no-PIC\n", argv[arg_index]);
                compiler->use_pic = TROOLEAN_FALSE;
            } else if(strcmp(argv[arg_index], "--no-PIC") == 0){
                compiler->use_pic = TROOLEAN_FALSE;
            } else if(strcmp(argv[arg_index], "-lm") == 0){
                // Accessibility versions of --libm
                yellowprintf("WARNING: Flag '%s' is not valid, assuming you meant to use --libm\n", argv[arg_index]);
                compiler->use_libm = true;
            } else if(strcmp(argv[arg_index], "--libm") == 0){
                compiler->use_libm = true;
            } else if(strncmp(argv[arg_index], "-std=", 5) == 0){
                compiler->default_stblib = &argv[arg_index][5];
                compiler->traits |= COMPILER_FORCE_STDLIB;
            }  else if(strncmp(argv[arg_index], "--std=", 6) == 0){
                compiler->default_stblib = &argv[arg_index][6];
                compiler->traits |= COMPILER_FORCE_STDLIB;
            }

            #ifdef ENABLE_DEBUG_FEATURES //////////////////////////////////
            else if(strcmp(argv[arg_index], "--run-tests") == 0){
                run_debug_tests();
                return 1;
            } else if(strcmp(argv[arg_index], "--stages") == 0){
                compiler->debug_traits |= COMPILER_DEBUG_STAGES;
            } else if(strcmp(argv[arg_index], "--dump") == 0){
                compiler->debug_traits |= COMPILER_DEBUG_DUMP;
            } else if(strcmp(argv[arg_index], "--llvmir") == 0){
                compiler->debug_traits |= COMPILER_DEBUG_LLVMIR;
            } else if(strcmp(argv[arg_index], "--no-verification") == 0){
                compiler->debug_traits |= COMPILER_DEBUG_NO_VERIFICATION;
            }
            #endif // ENABLE_DEBUG_FEATURES ///////////////////////////////

            else {
                redprintf("Invalid argument: %s\n", argv[arg_index]);
                return FAILURE;
            }
        } else if(object->filename == NULL){
            object->compilation_stage = COMPILATION_STAGE_FILENAME;
            object->filename = malloc(strlen(argv[arg_index]) + 1);
            strcpy(object->filename, argv[arg_index]);

            // Check that there aren't spaces in the filename
            length_t filename_length = strlen(object->filename);
            for(length_t c = 0; c != filename_length; c++){
                if(object->filename[c] == ' '){
                    redprintf("Filename cannot contain spaces! :\\\n");
                    return FAILURE;
                }
            }

            if(access(object->filename, F_OK) == -1){
                object->compilation_stage = COMPILATION_STAGE_NONE;
                redprintf("Can't find file '%s'\n", object->filename);
                free(object->filename);
                return FAILURE;
            }

            // Calculate the absolute path of the filename
            object->full_filename = filename_absolute(object->filename);

            if(object->full_filename == NULL){
                object->compilation_stage = COMPILATION_STAGE_NONE;
                redprintf("INTERNAL ERROR: Failed to get absolute path of filename '%s'\n", object->filename);
                free(object->filename);
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
                object->compilation_stage = COMPILATION_STAGE_NONE;
                redprintf("INTERNAL ERROR: Failed to get absolute path of filename '%s'\n", object->filename);
                free(object->filename);
                return FAILURE;
            }
        } else {
            show_help(false);
            return FAILURE;
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
    printf("The Adept Compiler v%s - (c) 2016-2020 Isaac Shelton\n\n", ADEPT_VERSION_STRING);
    printf("Usage: adept [options] [filename]\n\n");
    printf("Options:\n");
    printf("    -h, --help        Display this message\n");
    printf("    -e                Execute resulting executable\n");
    printf("    -w                Disable compiler warnings\n");
    printf("    -o FILENAME       Output to FILENAME (relative to working directory)\n");
    printf("    -n FILENAME       Output to FILENAME (relative to file)\n");

    if(show_advanced_options){
        printf("    -p, --package     Output a package [OBSOLETE]\n");
        printf("    -d                Include debugging symbols [UNIMPLEMENTED]\n");
    }


    if(show_advanced_options)
        printf("    -j                Preserve generated object file\n");
    
    printf("    -O0,-O1,-O2,-O3   Set optimization level\n");
    printf("    -std=2.x          Set standard library version\n");
    
    if(show_advanced_options)
        printf("    --fussy           Show insignificant warnings\n");
    
    printf("    --version         Display compiler version\n");
    printf("    --help-advanced   Show lesser used compiler flags\n");

    if(show_advanced_options){
        printf("\nLanguage Options:\n");
        printf("    --no-type-info    Disable runtime type information\n");
        printf("    --no-undef        Force initialize for 'undef'\n");
        printf("    --unsafe-meta     Allow unsafe usage of meta constructs\n");
        printf("    --unsafe-new      Disables zero-initialization of memory allocated with new\n");
        printf("    --null-checks     Enable runtime null-checks\n");

        printf("\nMachine Code Options:\n");
        printf("    --PIC             Forces PIC relocation model\n");
        printf("    --no-PIC          Forbids PIC relocation model\n");

        printf("\nLinker Options:\n");
        printf("    --libm            Forces linking against libc math library\n");

        printf("\nIgnore Options:\n");
        printf("    --ignore-all                      Enables all ignore options\n");
        printf("    --ignore-deprecation              Ignore deprecation warnings\n");
        printf("    --ignore-early-return             Ignore early return warnings\n");
        printf("    --ignore-obsolete                 Ignore obsolete feature warnings\n");
        printf("    --ignore-partial-support          Ignore partial compiler support warnings\n");
        printf("    --ignore-unrecognized-directives  Ignore unrecognized pragma directives\n");
    }

    #ifdef ENABLE_DEBUG_FEATURES
    printf("--------------------------------------------------\n");
    printf("Debug Exclusive Options:\n");
    printf("    --run-tests       Test compiler infrastructure\n");
    printf("    --stages          Announce major compilation stages\n");
    printf("    --dump            Dump AST, IAST, & IR to files\n");
    printf("    --llvmir          Show generated LLVM representation\n");
    printf("    --no-verification Don't verify backend output\n");
    #endif // ENABLE_DEBUG_FEATURES
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

strong_cstr_t compiler_get_string(){
    return mallocandsprintf("Adept %s - Build %s %s CDT", ADEPT_VERSION_STRING, __DATE__, __TIME__);
}

errorcode_t compiler_create_package(compiler_t *compiler, object_t *object){
    char *package_filename;

    if(compiler->output_filename != NULL){
        filename_auto_ext(&compiler->output_filename, FILENAME_AUTO_PACKAGE);
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

    if(filename_length >= 4 && strcmp(&object->filename[filename_length - 4], ".dep") == 0){
        return pkg_read(compiler, object);
    } else {
        return lex(compiler, object);
    }
}

strong_cstr_t compiler_get_stdlib(compiler_t *compiler, object_t *optional_object){
    // Find which standard library to use
    maybe_null_weak_cstr_t standard_library_folder = optional_object ? optional_object->default_stblib : NULL;

    if(standard_library_folder == NULL) standard_library_folder = compiler->default_stblib;
    if(standard_library_folder == NULL) standard_library_folder = ADEPT_VERSION_STRING;
    
    length_t length = strlen(standard_library_folder);
    char final_character = length == 0 ? 0x00 : standard_library_folder[length - 1];
    if(final_character == '/' || final_character == '\\') return strclone(standard_library_folder);

    // Otherwise, we need to append '/'
    return mallocandsprintf("%s/", standard_library_folder);
}

void compiler_print_source(compiler_t *compiler, int line, int column, source_t source){
    object_t *relevant_object = compiler->objects[source.object_index];
    if(relevant_object->buffer == NULL || relevant_object->traits & OBJECT_PACKAGE) return;

    length_t line_index = 0;
    for(length_t current_line = 1; current_line != line; line_index++){
        if(relevant_object->buffer[line_index] == '\n') current_line++;
    }

    char prefix[128];
    sprintf(prefix, "%d|", line);
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
        printf("^");
    printf("\n");
}

void compiler_panic(compiler_t *compiler, source_t source, const char *message){
    #ifndef ADEPT_INSIGHT_BUILD
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;

    if(message == NULL){
        if(relevant_object->traits & OBJECT_PACKAGE){
            redprintf("%s:?:?:\n", filename_name_const(relevant_object->filename));
        } else {
            lex_get_location(relevant_object->buffer, source.index, &line, &column);
            redprintf("%s:%d:%d:\n", filename_name_const(relevant_object->filename), line, column);
            compiler_print_source(compiler, line, column, source);
        }
        return;
    }

    if(relevant_object->traits & OBJECT_PACKAGE){
        redprintf("%s:?:?: %s!\n", filename_name_const(relevant_object->filename), message);
    } else {
        lex_get_location(relevant_object->buffer, source.index, &line, &column);
        redprintf("%s:%d:%d: %s!\n", filename_name_const(relevant_object->filename), line, column, message);
        compiler_print_source(compiler, line, column, source);
    }
    #endif // !ADEPT_INSIGHT_BUILDER

    if(compiler->error == NULL){
        strong_cstr_t buffer = calloc(256, 1);
        snprintf(buffer, 256, "%s", message);
        compiler->error = adept_error_create(buffer, source);
    }
}

void compiler_panicf(compiler_t *compiler, source_t source, const char *format, ...){
    #ifndef ADEPT_INSIGHT_BUILD
    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;
    #endif // !ADEPT_INSIGHT_BUILD
    
    va_list args, error_format_args;
    va_start(args, format);
    va_copy(error_format_args, args);

    #ifndef ADEPT_INSIGHT_BUILD
    terminal_set_color(TERMINAL_COLOR_RED);

    if(format == NULL){
        if(relevant_object->traits & OBJECT_PACKAGE){
            redprintf("%s:?:?:\n", filename_name_const(relevant_object->filename));
        } else {
            lex_get_location(relevant_object->buffer, source.index, &line, &column);
            redprintf("%s:%d:%d:\n", filename_name_const(relevant_object->filename), line, column);
            compiler_print_source(compiler, line, column, source);
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

    vprintf(format, args);
    printf("!\n");
    terminal_set_color(TERMINAL_COLOR_DEFAULT);

    compiler_print_source(compiler, line, column, source);
    #endif // !ADEPT_INSIGHT_BUILD

    if(compiler->error == NULL){
        strong_cstr_t buffer = calloc(256, 1);
        vsnprintf(buffer, 256, format, error_format_args);
        compiler->error = adept_error_create(buffer, source);
    }

    va_end(args);
    va_end(error_format_args);
}

void compiler_warn(compiler_t *compiler, source_t source, const char *message){
    if(compiler->traits & COMPILER_NO_WARN) return;

    object_t *relevant_object = compiler->objects[source.object_index];
    int line, column;
    lex_get_location(relevant_object->buffer, source.index, &line, &column);
    yellowprintf("%s:%d:%d: %s\n", filename_name_const(relevant_object->filename), line, column, message);
}

void compiler_warnf(compiler_t *compiler, source_t source, const char *format, ...){
    if(compiler->traits & COMPILER_NO_WARN) return;

    object_t *relevant_object = compiler->objects[source.object_index];
    va_list args;
    int line, column;

    terminal_set_color(TERMINAL_COLOR_YELLOW);
    va_start(args, format);

    lex_get_location(relevant_object->buffer, source.index, &line, &column);
    printf("%s:%d:%d: ", filename_name_const(relevant_object->filename), line, column);
    vprintf(format, args);
    printf("\n");

    va_end(args);
    terminal_set_color(TERMINAL_COLOR_DEFAULT);
}

#ifndef ADEPT_INSIGHT_BUILD
void compiler_undeclared_function(compiler_t *compiler, object_t *object, source_t source,
        const char *name, ast_type_t *types, length_t arity){

    ir_module_t *ir_module = &object->ir_module;
    maybe_index_t original_index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->func_mappings_length, name);
    maybe_index_t poly_index = find_beginning_of_poly_func_group(object->ast.polymorphic_funcs, object->ast.polymorphic_funcs_length, name);

    if(original_index == -1 && poly_index == -1){
        // No other function with that name exists
        compiler_panicf(compiler, source, "Undeclared function '%s'", name);
        return;
    } else {
        // Other functions have the same name
        char *args_string = make_args_string(types, NULL, arity);
        compiler_panicf(compiler, source, "Undeclared function %s(%s)", name, args_string ? args_string : "");
        free(args_string);

        printf("\nPotential Candidates:\n");
    }

    maybe_index_t index = original_index;

    if(index != -1) do {
        ir_func_mapping_t *mapping = &ir_module->func_mappings[index];

        if(mapping->is_beginning_of_group == -1){
            mapping->is_beginning_of_group = index == 0 ? 1 : (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
        }
        if(mapping->is_beginning_of_group == 1 && index != original_index) break;

        print_candidate(&object->ast.funcs[mapping->ast_func_id]);
    } while(++index != ir_module->funcs_length);

    index = poly_index;

    if(index != -1) do {
        ast_polymorphic_func_t *poly = &object->ast.polymorphic_funcs[index];
        
        if(poly->is_beginning_of_group == -1){
            poly->is_beginning_of_group = index == 0 ? 1 : (strcmp(poly->name, object->ast.polymorphic_funcs[index - 1].name) != 0);
        }
        if(poly->is_beginning_of_group == 1 && index != poly_index) break;

        print_candidate(&object->ast.funcs[poly->ast_func_id]);
    } while(++index != object->ast.polymorphic_funcs_length);
}

void compiler_undeclared_method(compiler_t *compiler, object_t *object, source_t source,
        const char *name, ast_type_t *types, length_t method_arity){
    
    // NOTE: Assuming that types_length == method_arity + 1
    ast_type_t this_type = types[0];

    // Ensure the type given for 'this_type' is valid
    if(this_type.elements_length != 2 || this_type.elements[0]->id != AST_ELEM_POINTER ||
        !(this_type.elements[1]->id == AST_ELEM_BASE || this_type.elements[1]->id == AST_ELEM_GENERIC_BASE)
    ){
        printf("INTERNAL ERROR: compiler_undeclared_method received invalid this_type\n");
        return;
    }

    // Modify ast_type_t to remove a pointer element from the front
    // NOTE: We don't take ownership of 'this_type' or its data
    // NOTE: This change doesn't propogate to outside this function
    // NOTE: 'ast_type_dereference' isn't used because 'this_type' doesn't own it's elements
    // DANGEROUS: Manually removing ast_elem_pointer_t
    this_type.elements = &this_type.elements[1];
    this_type.elements_length--; // Reduce length accordingly

    maybe_index_t original_index;
    ir_module_t *ir_module = &object->ir_module;
    unsigned int kind = this_type.elements[0]->id;
    ast_elem_generic_base_t *maybe_generic_base = NULL;

    if(kind == AST_ELEM_BASE){
        original_index = find_beginning_of_method_group(ir_module->methods, ir_module->methods_length, ((ast_elem_base_t*) this_type.elements[0])->base, name);
    } else if(kind == AST_ELEM_GENERIC_BASE){
        maybe_generic_base = (ast_elem_generic_base_t*) this_type.elements[0];
        original_index = find_beginning_of_generic_base_method_group(ir_module->generic_base_methods, ir_module->generic_base_methods_length,
            maybe_generic_base->name, name);
    } else {
        original_index = -1;
    }

    maybe_index_t poly_index = find_beginning_of_poly_func_group(object->ast.polymorphic_funcs, object->ast.polymorphic_funcs_length, name);

    if(original_index == -1 && poly_index == -1){
        // No method with that name exists for that struct
        char *this_core_typename = ast_type_str(&this_type);
        compiler_panicf(compiler, source, "Undeclared method '%s' for type '%s'", name, this_core_typename);
        free(this_core_typename);
        return;
    } else {
        // Other methods for that struct have the same name
        char *args_string = make_args_string(types, NULL, method_arity + 1);
        compiler_panicf(compiler, source, "Undeclared method %s(%s)", name, args_string ? args_string : "");
        free(args_string);

        printf("\nPotential Candidates:\n");
    }

    maybe_index_t index = original_index;

    // Print potential candidates for basic struct
    if(kind == AST_ELEM_BASE) do {
        ir_method_t *method = &ir_module->methods[index];

        if(method->is_beginning_of_group == -1){
            method->is_beginning_of_group = index == 0 ? 1 : (strcmp(method->name, ir_module->methods[index - 1].name) != 0 || strcmp(method->struct_name, ir_module->methods[index - 1].struct_name) != 0);
        }
        if(method->is_beginning_of_group == 1 && index != original_index) break;

        // Print method candidate for basic struct type
        print_candidate(&object->ast.funcs[method->ast_func_id]);
    } while(++index != ir_module->methods_length);

    // Print potential candidates for generic struct
    else if(kind == AST_ELEM_GENERIC_BASE) do {
        ir_generic_base_method_t *generic_base_method = &ir_module->generic_base_methods[index];
        
        if(generic_base_method->is_beginning_of_group == -1){
            generic_base_method->is_beginning_of_group = index == 0 ? 1 : (strcmp(generic_base_method->name, ir_module->generic_base_methods[index - 1].name) != 0 || strcmp(generic_base_method->generic_base, ir_module->generic_base_methods[index - 1].generic_base) != 0);
        }
        if(generic_base_method->is_beginning_of_group == 1 && index != original_index) break;

        // Ensure the generics of the generic base match up
        bool generics_match_up = maybe_generic_base->generics_length == generic_base_method->generics_length;
        if(generics_match_up) for(length_t i = 0; i != maybe_generic_base->generics_length; i++){
            if(!ast_types_identical(&maybe_generic_base->generics[i], &generic_base_method->generics[i])){
                // && !ast_type_has_polymorph(&generic_base_method->generics[i])
                // is unnessary because generic_base_methods my themselves will never contain polymorphic type variables
                generics_match_up = false;
                break;
            }
        }

        // Print method candidate for generic struct type (if the generics match up)
        if(generics_match_up){
            print_candidate(&object->ast.funcs[generic_base_method->ast_func_id]);
        }
    } while(++index != ir_module->generic_base_methods_length);

    index = poly_index;

    if(index != -1) do {
        ast_polymorphic_func_t *poly = &object->ast.polymorphic_funcs[index];

        if(poly->is_beginning_of_group == -1){
            poly->is_beginning_of_group = index == 0 ? 1 : (strcmp(poly->name, object->ast.polymorphic_funcs[index - 1].name) != 0);
        }
        if(poly->is_beginning_of_group == 1 && index != poly_index) break;

        ast_func_t *ast_func = &object->ast.funcs[poly->ast_func_id];

        // Ensure this function could possibly a method
        if(ast_func->arity == 0 || strcmp(ast_func->arg_names[0], "this") != 0) continue;

        // Ensure the first type is valid for a method
        if(ast_func->arity == 0) continue;
        ast_type_t *first_arg_type = &ast_func->arg_types[0];
        if(first_arg_type->elements_length != 2 || first_arg_type->elements[0]->id != AST_ELEM_POINTER) continue;
        unsigned int elem_kind = first_arg_type->elements[1]->id;
        if(kind != elem_kind && elem_kind != AST_ELEM_POLYMORPH) continue;

        // If it's a pointer-to-generic-struct method, then make sure the generics match
        if(elem_kind == AST_ELEM_GENERIC_BASE){
            ast_elem_generic_base_t *first_arg_generic_base = (ast_elem_generic_base_t*) first_arg_type->elements[1];

            // Ensure the generics of the generic base match up
            bool generics_match_up = maybe_generic_base->generics_length == first_arg_generic_base->generics_length;
            if(generics_match_up) for(length_t i = 0; i != maybe_generic_base->generics_length; i++){
                if(!(ast_types_identical(&maybe_generic_base->generics[i], &first_arg_generic_base->generics[i]) || ast_type_has_polymorph(&first_arg_generic_base->generics[i]))){
                    generics_match_up = false;
                    break;
                }
            }

            if(!generics_match_up) continue;
        }

        print_candidate(ast_func);
    } while(++index != object->ast.polymorphic_funcs_length);
}
#endif

void print_candidate(ast_func_t *ast_func){
    // NOTE: If the function is a method, we assume that it was constructed correctly and that
    // the first type is either a pointer to a base or a pointer to a generic base

    char *return_type_string = ast_type_str(&ast_func->return_type);
    char *args_string = make_args_string(ast_func->arg_types, ast_func->arg_defaults, ast_func->arity);
    printf("    %s(%s) %s\n", ast_func->name, args_string ? args_string : "", return_type_string);
    free(args_string);
    free(return_type_string);
}

strong_cstr_t make_args_string(ast_type_t *types, ast_expr_t **defaults, length_t arity){
    char *args_string = NULL;
    length_t args_string_length = 0;
    length_t args_string_capacity = 0;

    for(length_t i = 0; i != arity; i++){
        char *type_string = ast_type_str(&types[i]);
        length_t type_string_length = strlen(type_string);

        expand((void**) &args_string, sizeof(char), args_string_length, &args_string_capacity, type_string_length + 3, 256);
        memcpy(&args_string[args_string_length], type_string, type_string_length);
        args_string_length += type_string_length;

        if(defaults && defaults[i]){
            char *expr_string = ast_expr_str(defaults[i]);
            length_t expr_string_length = strlen(expr_string);

            expand((void**) &args_string, sizeof(char), args_string_length, &args_string_capacity, expr_string_length + 3, 256);

            memcpy(&args_string[args_string_length], " = ", 3);
            args_string_length += 3;
            
            memcpy(&args_string[args_string_length], expr_string, expr_string_length);
            args_string_length += expr_string_length;
            free(expr_string);
        }

        if(i + 1 != arity){
            memcpy(&args_string[args_string_length], ", ", 2);
            args_string_length += 2;
        }

        args_string[args_string_length] = '\0';
        free(type_string);
    }

    if(args_string == NULL) args_string = strclone("");
    return args_string;
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
    printf("!\n");
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
