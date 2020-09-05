
#ifndef _ISAAC_COMPILER_H
#define _ISAAC_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================ compiler.h ================================
    Module for encapsulating the compiler
    ----------------------------------------------------------------------------
*/

#include "UTIL/trait.h"
#include "UTIL/ground.h"
#include "DRVR/config.h"
#include "DRVR/object.h"

// Possible compiler trait options
#define COMPILER_SHOW_CONSOLE     TRAIT_1
#define COMPILER_MAKE_PACKAGE     TRAIT_2
#define COMPILER_INFLATE_PACKAGE  TRAIT_3
#define COMPILER_EXECUTE_RESULT   TRAIT_4
#define COMPILER_DEBUG_SYMBOLS    TRAIT_5
#define COMPILER_NO_WARN          TRAIT_6
#define COMPILER_NO_UNDEF         TRAIT_7
#define COMPILER_NO_TYPEINFO      TRAIT_8
#define COMPILER_NO_REMOVE_OBJECT TRAIT_A
#define COMPILER_UNSAFE_META      TRAIT_B
#define COMPILER_UNSAFE_NEW       TRAIT_C
#define COMPILER_FUSSY            TRAIT_D
#define COMPILER_FORCE_STDLIB     TRAIT_E

// Possible compiler trait checks
#define COMPILER_NULL_CHECKS      TRAIT_1
#define COMPILER_LEAK_CHECKS      TRAIT_2
#define COMPILER_BOUNDS_CHECKS    TRAIT_3

// Possible compiler errors/warning to ignore
#define COMPILER_IGNORE_DEPRECATION             TRAIT_1
#define COMPILER_IGNORE_PARTIAL_SUPPORT         TRAIT_2
#define COMPILER_IGNORE_EARLY_RETURN            TRAIT_3
#define COMPILER_IGNORE_UNRECOGNIZED_DIRECTIVES TRAIT_3
#define COMPILER_IGNORE_OBSOLETE                TRAIT_4
#define COMPILER_IGNORE_UNUSED                  TRAIT_5
#define COMPILER_IGNORE_ALL                     TRAIT_ALL

// Possible optimization levels
#define OPTIMIZATION_NONE       0x00
#define OPTIMIZATION_LESS       0x01
#define OPTIMIZATION_DEFAULT    0x02
#define OPTIMIZATION_AGGRESSIVE 0x03

// Possible compiler debug trait options
#ifdef ENABLE_DEBUG_FEATURES
#define COMPILER_DEBUG_STAGES          TRAIT_1
#define COMPILER_DEBUG_DUMP            TRAIT_2
#define COMPILER_DEBUG_LLVMIR          TRAIT_3
#define COMPILER_DEBUG_NO_VERIFICATION TRAIT_4
#endif // ENABLE_DEBUG_FEATURES

// Possible compiler result flags (for internal use)
#define COMPILER_RESULT_NONE                    TRAIT_NONE
#define COMPILER_RESULT_SUCCESS                 TRAIT_1 // Signifies success

// ---------------- adept_error_t ----------------
// Short result error message
typedef struct {
    strong_cstr_t message;
    source_t source;
} adept_error_t;

// ---------------- compiler_t ----------------
// Structure that encapsulates the compiler
typedef struct compiler {
    char *location;      // Compiler Filename
    char *root;          // Compiler Root Directory (with /)

    object_t **objects;
    length_t objects_length;
    length_t objects_capacity;

    // Compiler persistent configuration options
    config_t config;

    // Compiler command-line configuration options
    trait_t traits;            // COMPILER_* options
    char *output_filename;     // owned c-string
    unsigned int optimization; // 0 - 3 using OPTIMIZATION_* constants
    trait_t result_flags;      // Results flag (for internal use)
    trait_t checks;
    trait_t ignore;
    troolean use_pic;          // Generate using PIC relocation model
    bool use_libm;             // Link to libm using '-lm'
    
    #ifdef ENABLE_DEBUG_FEATURES
    trait_t debug_traits;      // COMPILER_DEBUG_* options
    #endif // ENABLE_DEBUG_FEATURES

    // Default standard library to import from (global version)
    // If NULL, then use ADEPT_VERSION_STRING
    maybe_null_weak_cstr_t default_stblib;

    adept_error_t *error;
    bool show_unused_variables_how_to_disable;
} compiler_t;

// ---------------- compiler_run ----------------
// Runs a compiler with the given arguments.
errorcode_t compiler_run(compiler_t *compiler, int argc, char **argv);

// ---------------- compiler_invoke ----------------
// Invokes a compiler with arguments.
// NOTE: This is for internal use!  Use compiler_run() instead!
void compiler_invoke(compiler_t *compiler, int argc, char **argv);

// ---------------- compiler_init ----------------
// Initializes a compiler
void compiler_init(compiler_t *compiler);

// ---------------- compiler_free ----------------
// Frees data within a compiler
void compiler_free(compiler_t *compiler);

// ---------------- compiler_new_object ----------------
// Generates a new object within the compiler
object_t* compiler_new_object(compiler_t *compiler);

// ---------------- compiler_final_words ----------------
// Says any final notes the compiler has
void compiler_final_words(compiler_t *compiler);

// ---------------- parse_arguments ----------------
// Configures a compiler based on program arguments.
// argv[0] is ignored
errorcode_t parse_arguments(compiler_t *compiler, object_t *object, int argc, char **argv);

// ---------------- break_into_arguments ----------------
// Breaks a string into pseudo program arguments
// (*out_argv)[0] will be a blank constant c-string
void break_into_arguments(const char *s, int *out_argc, char ***out_argv);

// ---------------- show_help ----------------
// Displays program help information
void show_help(bool show_advanced_options);

// ---------------- show_version ----------------
// Displays program version information
void show_version(compiler_t *compiler);

// ---------------- compiler_get_string ----------------
// Gets the string identifier of the compiler
strong_cstr_t compiler_get_string();

// ---------------- compiler_create_package ----------------
// Creates and exports a package
errorcode_t compiler_create_package(compiler_t *compiler, object_t *object);

// ---------------- compiler_read_file ----------------
// Reads either a package or adept code file into tokens for an object
errorcode_t compiler_read_file(compiler_t *compiler, object_t *object);

// ---------------- compiler_get_stdlib ----------------
// Returns the current stdlib folder, including the preceeding slash
strong_cstr_t compiler_get_stdlib(compiler_t *compiler, object_t *optional_object);

// ---------------- compiler_print_source ----------------
// Prints the source code at a given 'source_t'
void compiler_print_source(compiler_t *compiler, int line, int column, source_t source);

// ---------------- compiler_panic (and friends) ----------------
// Prints a compiler error message at a given 'source_t'
void compiler_panic(compiler_t *compiler, source_t source, const char *message);
void compiler_panicf(compiler_t *compiler, source_t source, const char *format, ...);

// ---------------- compiler_warn (and friends) ----------------
// Prints a compiler warning at a given 'source_t'
void compiler_warn(compiler_t *compiler, source_t source, const char *message);
void compiler_warnf(compiler_t *compiler, source_t source, const char *format, ...);

#ifndef ADEPT_INSIGHT_BUILD
// ---------------- compiler_undeclared_function ----------------
// Prints an error message for an undeclared function
void compiler_undeclared_function(compiler_t *compiler, object_t *object, source_t source,
    const char *name, ast_type_t *types, length_t arity);

// ---------------- compiler_undeclared_method ----------------
// Prints an error message for an undeclared method
void compiler_undeclared_method(compiler_t *compiler, object_t *object, source_t source,
    const char *name, ast_type_t *types, length_t method_arity);
#endif

// ---------------- print_candidate ----------------
// Prints a function/method candidate
void print_candidate(ast_func_t *ast_func);

// ---------------- make_args_string ----------------
// Helper function for generating a string for function arguments
strong_cstr_t make_args_string(ast_type_t *types, ast_expr_t **defaults, length_t arity, trait_t traits);

// ---------------- object_panic_plain ----------------
// Prints a plain compiler error given an object
void object_panic_plain(object_t *object, const char *message);


// ---------------- object_panic_plain ----------------
// Prints a plain compiler error given an object
void object_panicf_plain(object_t *object, const char *message, ...);

// ---------------- adept_error_create ----------------
// Returns a newly allocated and initialized 'adept_error_t'
adept_error_t *adept_error_create(strong_cstr_t message, source_t source);

// ---------------- adept_error_free_fully ----------------
// Frees the memory owned by an 'adept_error_t' and the memory occupied by it
void adept_error_free_fully(adept_error_t *error);

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_COMPILER_H
