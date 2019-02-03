
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
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
    // A wrapper function around 'compiler_execute'
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
	if(argv == NULL || argv[0] == NULL || strcmp(argv[0], "") == 0){
		redprintf("EXTERNAL ERROR: Compiler was invoked with NULL or empty argv[0]\n");
		return;
	}
	
	compiler->location = filename_absolute(argv[0]);
    #endif

    compiler->root = filename_path(compiler->location);

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
    compiler->traits = TRAIT_NONE;
    compiler->output_filename = NULL;
    compiler->optimization = OPTIMIZATION_NONE;
    compiler->checks = TRAIT_NONE;

    #ifdef ENABLE_DEBUG_FEATURES
    compiler->debug_traits = TRAIT_NONE;
    #endif // ENABLE_DEBUG_FEATURES
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
        case COMPILATION_STAGE_NONE:
            break; // Nothing to free yet so yeah
        case COMPILATION_STAGE_FILENAME:
            free(object->filename);
            free(object->full_filename);
            break;
        case COMPILATION_STAGE_TOKENLIST:
            free(object->filename);
            free(object->full_filename);
            free(object->buffer);
            tokenlist_free(&object->tokenlist);
            break;
        case COMPILATION_STAGE_AST:
            free(object->filename);
            free(object->full_filename);
            free(object->buffer);
            tokenlist_free(&object->tokenlist);
            ast_free(&object->ast);
            break;
        case COMPILATION_STAGE_IR_MODULE:
            free(object->filename);
            free(object->full_filename);
            free(object->buffer);
            tokenlist_free(&object->tokenlist);
            ast_free(&object->ast);
            
            #ifndef ADEPT_INSIGHT_BUILD
            ir_module_free(&object->ir_module);
            #endif
            break;
        default:
            printf("INTERNAL ERROR: Failed to delete object that has an invalid compilation stage\n");
        }

        free(object); // Free memory that the object is stored in
    }

    free(compiler->objects);
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
    return *object_reference;
}

errorcode_t parse_arguments(compiler_t *compiler, object_t *object, int argc, char **argv){
    int arg_index = 1;

    while(arg_index != argc){
        if(argv[arg_index][0] == '-'){
            if(strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0){
                show_help();
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
            } else if(strcmp(argv[arg_index], "--no-undef") == 0){
                compiler->traits |= COMPILER_NO_UNDEF;
            } else if(strcmp(argv[arg_index], "--no-type-info") == 0){
                compiler->traits |= COMPILER_NO_TYPE_INFO;
            } else if(strcmp(argv[arg_index], "--null-checks") == 0){
                compiler->checks |= COMPILER_NULL_CHECKS;
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
            show_help();
            return FAILURE;
        }
    }

    return SUCCESS;
}

void break_into_arguments(const char *s, int *out_argc, char ***out_argv){
    // Breaks a string into arguments (quote and backslashes allowed)
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

void show_help(){
    printf("The Adept Compiler v2.1 - (c) 2016-2019 Isaac Shelton\n\n");
    printf("Usage: adept [options] <filename>\n\n");
    printf("Options:\n");
    printf("    -h, --help        Display this message\n");
    printf("    -n FILENAME       Write output to FILENAME (relative to file)\n");
    printf("    -o FILENAME       Write output to FILENAME (relative to working directory)\n");
    printf("    -e                Execute resulting executable\n");
    printf("    -p, --package     Output a package\n");
    printf("    -d                Include debugging symbols\n");
    printf("    -w                Disable all compiler warnings\n");
    printf("    -j                Preserve generated object file\n");
    printf("    -O                Set optimization level\n");

    printf("\nLanguage Options:\n");
    printf("    --no-undef        Force initialize for 'undef'\n");
    printf("    --no-type-info    Disable runtime type information\n");
    printf("    --null-checks     Enable runtime null-checks\n");

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

void compiler_print_source(compiler_t *compiler, int line, int column, source_t source){
    object_t *object = compiler->objects[source.object_index];
    if(object->buffer == NULL || object->traits & OBJECT_PACKAGE) return;

    length_t line_index = 0;
    for(length_t current_line = 1; current_line != line; line_index++){
        if(object->buffer[line_index] == '\n') current_line++;
    }

    char prefix[128];
    sprintf(prefix, "%d|", line);
    length_t prefix_length = strlen(prefix);
    printf("%s", prefix);

    while(object->buffer[line_index] == '\t'){
        printf("    "); line_index++; prefix_length += 4;
    }

    length_t line_length = 0;
    while(true){
        if(object->buffer[line_index + line_length] == '\n') break;
        line_length++;
    }

    char *line_text = malloc(line_length + 1);
    line_text = memcpy(line_text, &object->buffer[line_index], line_length);
    line_text[line_length] = '\0';
    printf("%s\n", line_text);
    free(line_text);

    for(length_t i = 0; i != prefix_length; i++) printf(" ");
    for(length_t i = line_index; i != source.index; i++) printf(object->buffer[i] != '\t' ? " " : "    ");

    for(length_t i = 0; i != source.stride; i++)
        printf("^");
    printf("\n");
}

void compiler_panic(compiler_t *compiler, source_t source, const char *message){
    #ifndef ADEPT_INSIGHT_BUILD
    object_t *object = compiler->objects[source.object_index];
    int line, column;

    if(object->traits & OBJECT_PACKAGE){
        redprintf("%s: %s!\n", filename_name_const(object->filename), message);
    } else {
        lex_get_location(object->buffer, source.index, &line, &column);
        redprintf("%s:%d:%d: %s!\n", filename_name_const(object->filename), line, column, message);
        compiler_print_source(compiler, line, column, source);
    }
    #endif
}

void compiler_panicf(compiler_t *compiler, source_t source, const char *format, ...){
    #ifndef ADEPT_INSIGHT_BUILD
    object_t *object = compiler->objects[source.object_index];
    int line, column;
    va_list args;

    va_start(args, format);
    terminal_set_color(TERMINAL_COLOR_RED);

    if(object->traits & OBJECT_PACKAGE){
        line = 1;
        column = 1;
        printf("%s: ", filename_name_const(object->filename));
    } else {
        lex_get_location(object->buffer, source.index, &line, &column);
        printf("%s:%d:%d: ", filename_name_const(object->filename), line, column);
    }

    vprintf(format, args);
    printf("!\n");
    terminal_set_color(TERMINAL_COLOR_DEFAULT);

    va_end(args);
    compiler_print_source(compiler, line, column, source);
    #endif
}

void compiler_warn(compiler_t *compiler, source_t source, const char *message){
    if(compiler->traits & COMPILER_NO_WARN) return;

    object_t *object = compiler->objects[source.object_index];
    int line, column;
    lex_get_location(object->buffer, source.index, &line, &column);
    terminal_set_color(TERMINAL_COLOR_YELLOW);
    printf("%s:%d:%d: %s\n", filename_name_const(object->filename), line, column, message);
    terminal_set_color(TERMINAL_COLOR_DEFAULT);
}

void compiler_warnf(compiler_t *compiler, source_t source, const char *format, ...){
    if(compiler->traits & COMPILER_NO_WARN) return;

    object_t *object = compiler->objects[source.object_index];
    va_list args;
    int line, column;

    terminal_set_color(TERMINAL_COLOR_YELLOW);
    va_start(args, format);

    lex_get_location(object->buffer, source.index, &line, &column);
    printf("%s:%d:%d: ", filename_name_const(object->filename), line, column);
    vprintf(format, args);
    printf("\n");

    va_end(args);
    terminal_set_color(TERMINAL_COLOR_DEFAULT);
}

#ifndef ADEPT_INSIGHT_BUILD
void compiler_undeclared_function(compiler_t *compiler, ir_module_t *ir_module, source_t source,
        const char *name, ast_type_t *types, length_t arity){

    maybe_index_t original_index = find_beginning_of_func_group(ir_module->func_mappings, ir_module->funcs_length, name);
    
    if(original_index == -1){
        // No other function with that name exists
        compiler_panicf(compiler, source, "Undeclared function '%s'", name);
        return;
    } else {
        // Other functions have the same name
        char *args_string = make_args_string(types, arity);
        compiler_panicf(compiler, source, "Undeclared function %s(%s)", name, args_string ? args_string : "");
        free(args_string);

        printf("\nPotential Candidates:\n");
    }

    maybe_index_t index = original_index;

    do {
        ir_func_mapping_t *mapping = &ir_module->func_mappings[index];

        if(mapping->is_beginning_of_group == -1){
            mapping->is_beginning_of_group = (strcmp(mapping->name, ir_module->func_mappings[index - 1].name) != 0);
        }
        if(mapping->is_beginning_of_group == 1 && index != original_index) return;

        char *return_type_string = ast_type_str(&mapping->ast_func->return_type);
        char *args_string = make_args_string(mapping->ast_func->arg_types, mapping->ast_func->arity);

        printf("    %s(%s) %s\n", mapping->ast_func->name, args_string, return_type_string);

        free(args_string);
        free(return_type_string);
    } while(++index != ir_module->funcs_length);
}
#endif

strong_cstr_t make_args_string(ast_type_t *types, length_t arity){
    char *args_string = NULL;
    length_t args_string_length = 0;
    length_t args_string_capacity = 0;

    for(length_t i = 0; i != arity; i++){
        char *type_string = ast_type_str(&types[i]);
        length_t type_string_length = strlen(type_string);

        expand((void**) &args_string, sizeof(char), args_string_length, &args_string_capacity, type_string_length + 3, 256);

        memcpy(&args_string[args_string_length], type_string, type_string_length);
        args_string_length += type_string_length;

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
    redprintf("%s: %s\n", filename_name_const(object->filename), message);
}
