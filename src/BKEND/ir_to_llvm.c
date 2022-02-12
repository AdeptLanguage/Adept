
#include <ctype.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/IPO.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AST/ast.h"
#include "BKEND/ir_to_llvm.h"
#include "DRVR/compiler.h"
#include "DRVR/debug.h"
#include "DRVR/object.h"
#include "IR/ir.h"
#include "UTIL/color.h"
#include "UTIL/filename.h"
#include "UTIL/ground.h"
#include "UTIL/list.h"
#include "UTIL/string.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"
#include "llvm-c/Analysis.h" // IWYU pragma: keep
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Types.h"

static char *sanitize_in_place(char *string){
    length_t length = strlen(string);

    for(char *s = string; *s;){
        if(!isalnum(*s) && *s != '-' && *s != '_'){
            memmove(s, s + 1, length-- - (s - string));
        } else {
            s++;
        }
    }

    return string;
}

static void create_static_variables(llvm_context_t *llvm){
    ir_static_variables_t *static_variables = &llvm->object->ir_module.static_variables;

    for(length_t i = 0; i != static_variables->length; i++){
        // Create LLVM global variable for each static variable
        LLVMTypeRef  type   = ir_to_llvm_type(llvm, static_variables->variables[i].type);
        LLVMValueRef global = LLVMAddGlobal(llvm->module, type, "y");
        LLVMSetInitializer(global, LLVMGetUndef(type));

        // Remember LLVM global variable
        list_append(&llvm->static_variables, ((llvm_static_variable_t){
            .global = global,
            .type = type,
        }), llvm_static_variable_t);
    }
}

static char *get_triple(compiler_t *compiler){
    switch(compiler->cross_compile_for){
    case CROSS_COMPILE_WINDOWS:
        return LLVMCreateMessage("x86_64-pc-windows-gnu");
    case CROSS_COMPILE_MACOS:
        return LLVMCreateMessage("x86_64-apple-darwin19.6.0");
    case CROSS_COMPILE_WASM32:
        return LLVMCreateMessage("wasm32-unknown-unknown");
    }

	#ifdef _WIN32
        return LLVMCreateMessage("x86_64-pc-windows-gnu");
    #else
        return LLVMGetDefaultTargetTriple();
    #endif
}

static errorcode_t get_target_from_triple(char *triple, LLVMTargetRef *out_target){
    char *llvm_error;

    if(LLVMGetTargetFromTriple(triple, out_target, &llvm_error)){
        internalerrorprintf("ir_to_llvm() - LLVMGetTargetFromTriple() failed with message: %s\n", llvm_error);

        if(string_starts_with(triple, "wasm")){
            blueprintf("NOTICE: If you built this compiler yourself, make sure that the build of LLVM you linked against includes support for the 'wasm64' target!\n");
        }

        LLVMDisposeMessage(llvm_error);
        return FAILURE;
    }

    return SUCCESS;
}

static void autofill_output_filename(compiler_t *compiler, object_t *object){
    // Auto specify output filename for compiler if one wasn't already given
    if(compiler->output_filename == NULL){
        compiler->output_filename = filename_without_ext(object->filename);
    }
	
    filename_auto_ext(&compiler->output_filename, compiler->cross_compile_for, FILENAME_AUTO_EXECUTABLE);
}

static strong_cstr_t get_objfile_filename(compiler_t *compiler){
    return filename_ext(compiler->output_filename, "o");
}

static strong_cstr_t create_windows_link_command(
    compiler_t *compiler,
    const char *bin_root,
    const char *linker,
    const char *objfile_filename,
    const char *linker_additional,
    const char *libmsvcrt,
    bool cmd_shell
){
    string_builder_t builder;
    string_builder_init(&builder);

    if(cmd_shell){
        string_builder_append_char(&builder, '"');
    }

    // file/path/to/root/ld.exe
    string_builder_append2_quoted(&builder, bin_root, linker);

    // --start-group -static
    string_builder_append(&builder, " --start-group -static ");

    // crt2.o
    string_builder_append2_quoted(&builder, bin_root, "crt2.o");
    string_builder_append_char(&builder, ' ');

    // crt2begin.o
    string_builder_append2_quoted(&builder, bin_root, "crtbegin.o");
    string_builder_append_char(&builder, ' ');

    // Options
    string_builder_append(&builder, linker_additional);
    string_builder_append_char(&builder, ' ');

    // location/of/object/file.o
    string_builder_append_quoted(&builder, objfile_filename);
    string_builder_append_char(&builder, ' ');

    // libdep.a
    string_builder_append2_quoted(&builder, bin_root, "libdep.a");

    // --end-group C:/Windows/System32/msvcrt.dll
    string_builder_append(&builder, " --end-group ");

    string_builder_append_quoted(&builder, libmsvcrt);
    string_builder_append_char(&builder, ' ');

    // --subsystem windows
    if(compiler->traits & COMPILER_WINDOWED){
        string_builder_append(&builder, "--subsystem windows ");
    }

    // -o filename/to/output.exe
    string_builder_append(&builder, "-o ");
    string_builder_append_quoted(&builder, compiler->output_filename);

    if(cmd_shell){
        string_builder_append_char(&builder, '"');
    }

    return string_builder_finalize(&builder);
}

static strong_cstr_t create_unix_link_command(
    compiler_t *compiler,
    const char *linker,
    const char *objfile_filename,
    const char *linker_additional
){
    string_builder_t builder;
    string_builder_init(&builder);

    string_builder_append(&builder, linker);
    string_builder_append_char(&builder, ' ');
    string_builder_append_quoted(&builder, objfile_filename);
    string_builder_append_char(&builder, ' ');
    string_builder_append(&builder, linker_additional);

    if(compiler->use_libm){
        string_builder_append(&builder, " -lm");
    }

    string_builder_append(&builder, " -o ");
    string_builder_append_quoted(&builder, compiler->output_filename);

    return string_builder_finalize(&builder);
}

static strong_cstr_t create_linker_additional(llvm_context_t *llvm){
    string_builder_t builder;
    string_builder_init(&builder);

    compiler_t *compiler = llvm->compiler;
    object_t *object = llvm->object;

    char **libraries = object->ast.libraries;
    char *library_kinds = object->ast.library_kinds;
    length_t libraries_length = object->ast.libraries_length;

    for(length_t i = 0; i != libraries_length; i++){
        char *library = libraries[i];

        switch(library_kinds[i]){
        case LIBRARY_KIND_NONE:
            string_builder_append_quoted(&builder, library);
            break;
        case LIBRARY_KIND_LIBRARY:
            string_builder_append(&builder, "-l");
            string_builder_append(&builder, sanitize_in_place(library));
            break;
        case LIBRARY_KIND_FRAMEWORK:
            string_builder_append(&builder, "-framework ");
            string_builder_append_quoted(&builder, library);
            break;
        default:
            die("create_linker_additional() - Unrecognized library kind %d\n", (int) library_kinds[i]);
        }

        if(i + 1 != libraries_length){
            string_builder_append_char(&builder, ' ');
        }
    }

    if(compiler->user_linker_options.length != 0){
        string_builder_append(&builder, compiler->user_linker_options.buffer);
    }

    return strong_cstr_empty_if_null(string_builder_finalize(&builder));
}

static maybe_null_strong_cstr_t create_link_command_from_parts(llvm_context_t *llvm, const char *objfile_filename, const char *linker_additional){
	#ifdef _WIN32
    // Windows -> ???

    if(llvm->compiler->cross_compile_for == CROSS_COMPILE_MACOS){
        // Windows -> MacOS
        // Even though we can't link it,
        // we will give the user the link command needed to link it on a MacOS machine.
        return create_unix_link_command(llvm->compiler, "gcc", objfile_filename, linker_additional);
    }

    // Windows -> Windows
    return create_windows_link_command(llvm->compiler, llvm->compiler->root, "ld.exe", objfile_filename, linker_additional, "C:/Windows/System32/msvcrt.dll", true);
	#else
    // Unix -> ???

    if(llvm->compiler->cross_compile_for == CROSS_COMPILE_WINDOWS){
        // Unix -> Windows
        const char *linker = "x86_64-w64-mingw32-ld";
        strong_cstr_t alt_bin_root = mallocandsprintf("%scross-compile-windows/", llvm->compiler->root);
        strong_cstr_t cross_linker = mallocandsprintf("%s%s", alt_bin_root, linker);
        strong_cstr_t libmsvcrt = mallocandsprintf("%s%s", alt_bin_root, "libmsvcrt.a");
        strong_cstr_t result = NULL;

        if(file_exists(cross_linker)){
            result = create_windows_link_command(llvm->compiler, alt_bin_root, linker, objfile_filename, linker_additional, libmsvcrt, false);
        } else {
            printf("\n");
            redprintf("Cross compiling for Windows requires the 'cross-compile-windows' extension!\n");
            redprintf("You need to first download and install it from here:\n");
            printf("    https://github.com/IsaacShelton/AdeptCrossCompilation/releases\n");
        }

        free(libmsvcrt);
        free(alt_bin_root);
        free(cross_linker);
        return result;
    }

    // Unix -> Unix
    return create_unix_link_command(llvm->compiler, "gcc", objfile_filename, linker_additional);
	#endif
}

static maybe_null_strong_cstr_t create_link_command(llvm_context_t *llvm, const char *objfile_filename){
    strong_cstr_t linker_additional = create_linker_additional(llvm);

    maybe_null_strong_cstr_t result = create_link_command_from_parts(llvm, objfile_filename, linker_additional);

    free(linker_additional);
    return result;
}

static void execute_result(weak_cstr_t output_filename){
    strong_cstr_t executable = strclone(output_filename);

    #ifdef _WIN32
        // For windows, make sure we change all '/' to '\' before invoking
        length_t executable_length = strlen(executable);
    
        for(length_t i = 0; i != executable_length; i++){
            if(executable[i] == '/') executable[i] = '\\';
        }
    #else
        filename_prepend_dotslash_if_needed(&executable);
    #endif

    system(executable);
    free(executable);	
}

static errorcode_t emit_to_file(
    LLVMModuleRef module,
    LLVMTargetMachineRef target_machine,
    LLVMPassManagerRef pass_manager,
    weak_cstr_t objfile_filename
){
    LLVMCodeGenFileType codegen = LLVMObjectFile;
    
    LLVMAddGlobalOptimizerPass(pass_manager);
    LLVMAddConstantMergePass(pass_manager);
    LLVMRunPassManager(pass_manager, module);

    char *llvm_error;
    if(LLVMTargetMachineEmitToFile(target_machine, module, objfile_filename, codegen, &llvm_error)){
        internalerrorprintf("ir_to_llvm() - LLVMTargetMachineEmitToFile() failed with message: %s\n", llvm_error);
        LLVMDisposeMessage(llvm_error);
        return FAILURE;
    }

    return SUCCESS;
}

errorcode_t ir_to_llvm(compiler_t *compiler, object_t *object){
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    ir_module_t *ir_module = &object->ir_module;
    weak_cstr_t module_name = filename_name_const(object->filename);

    LLVMModuleRef llvm_module = LLVMModuleCreateWithName(module_name);
    char *triple = get_triple(compiler);

    LLVMTargetRef target;
    if(get_target_from_triple(triple, &target)){
        LLVMDisposeMessage(triple);
        return FAILURE;
    }

    LLVMSetTarget(llvm_module, triple);

    weak_cstr_t cpu = "generic";
    weak_cstr_t features = "";
    LLVMCodeGenOptLevel level = ir_to_llvm_config_optlvl(compiler);
    LLVMRelocMode reloc = compiler->use_pic ? LLVMRelocPIC : LLVMRelocDefault;
    LLVMCodeModel code_model = LLVMCodeModelDefault;
    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(target, triple, cpu, features, level, reloc, code_model);

    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    LLVMSetModuleDataLayout(llvm_module, data_layout);

    llvm_context_t llvm = (llvm_context_t){
        .module = llvm_module,
        .builder = (void*) 0xD3ADB33F,
        .catalog = (void*) 0xD3ADB33F,
        .stack = (void*) 0xD3ADB33F,
        .func_skeletons = malloc(sizeof(LLVMValueRef) * ir_module->funcs.length),
        .global_variables = malloc(sizeof(LLVMValueRef) * ir_module->globals_length),
        .anon_global_variables = malloc(sizeof(LLVMValueRef) * ir_module->anon_globals.length),
        .data_layout = data_layout,
        .intrinsics = (llvm_intrinsics_t){0},
        .compiler = compiler,
        .object = object,
        .null_check = (llvm_null_check_t){0},
        .string_table = (llvm_string_table_t){0},
        .relocation_list = (llvm_phi2_relocation_list_t){0},
        .static_variable_info = (llvm_static_variable_info_t){0},
        .i64_type = LLVMInt64Type(),
        .f64_type = LLVMDoubleType(),
    };

    create_static_variables(&llvm);

    if(ir_to_llvm_globals(&llvm, object)
    || ir_to_llvm_functions(&llvm, object)
    || ir_to_llvm_function_bodies(&llvm, object)
    || ir_to_llvm_inject_init_built(&llvm)
    || ir_to_llvm_inject_deinit_built(&llvm)){
        free(llvm.func_skeletons);
        free(llvm.global_variables);
        free(llvm.anon_global_variables);
        free(llvm.string_table.entries);
        free(llvm.static_variables.variables);
        free(llvm.relocation_list.unrelocated);
        LLVMDisposeTargetData(data_layout);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(llvm.module);
        return FAILURE;
    }

    free(llvm.string_table.entries);
    free(llvm.relocation_list.unrelocated);

    #ifdef ENABLE_DEBUG_FEATURES
    if(compiler->debug_traits & COMPILER_DEBUG_LLVMIR) LLVMDumpModule(llvm.module);
    #endif // ENABLE_DEBUG_FEATURES

    // Free reference arrays
    free(llvm.func_skeletons);
    free(llvm.global_variables);
    free(llvm.anon_global_variables);
    free(llvm.static_variables.variables);

    // Figure out object filename
    autofill_output_filename(compiler, object);
    strong_cstr_t objfile_filename = get_objfile_filename(compiler);

    strong_cstr_t link_command = create_link_command(&llvm, objfile_filename);

    if(link_command == NULL){
        LLVMDisposeTargetData(data_layout);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeMessage(triple);
        LLVMDisposeModule(llvm.module);
        free(objfile_filename);
        return FAILURE;
    }

    #ifdef ENABLE_DEBUG_FEATURES
    if(!(llvm.compiler->debug_traits & COMPILER_DEBUG_NO_VERIFICATION) && LLVMVerifyModule(llvm.module, LLVMPrintMessageAction, NULL) == 1){
        yellowprintf("\n========== LLVM Verification Failed! ==========\n");
    }
    #endif

    debug_signal(compiler, DEBUG_SIGNAL_AT_OUT, NULL);
    LLVMPassManagerRef pass_manager = LLVMCreatePassManager();

    #ifdef ENABLE_DEBUG_FEATURES
    bool no_result = compiler->debug_traits & COMPILER_DEBUG_NO_RESULT;
    #else
    bool no_result = false;
    #endif

    if(!no_result){
        if(emit_to_file(llvm.module, target_machine, pass_manager, objfile_filename)){
            LLVMDisposeTargetData(data_layout);
            LLVMDisposeTargetMachine(target_machine);
            LLVMDisposePassManager(pass_manager);
            LLVMDisposeMessage(triple);
            LLVMDisposeModule(llvm.module);
            free(objfile_filename);
        }
    }

    LLVMDisposeTargetData(data_layout);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposePassManager(pass_manager);
    LLVMDisposeMessage(triple);
    LLVMDisposeModule(llvm.module);

    if(compiler->traits & COMPILER_EMIT_OBJECT){
        free(objfile_filename);
        free(link_command);
        return SUCCESS;
    }

    if(compiler->cross_compile_for == CROSS_COMPILE_MACOS){
        // Don't support linking output Mach-O object files
        printf("Mach-O Object File Generated (Requires Manual Linking)\n");
        printf("\nLink Command: '%s'\n", link_command);
        free(objfile_filename);
        free(link_command);
        return SUCCESS;
    }
    
    debug_signal(compiler, DEBUG_SIGNAL_AT_LINKING, NULL);

    if(!no_result){
        // TODO: SECURITY: Stop using system(3) call to invoke linker
        if(system(link_command) != 0){
            redprintf("external-error: ");
            printf("link command failed\n%s\n", link_command);
            free(objfile_filename);
            free(link_command);
            return FAILURE;
        }
    
        if(compiler->traits & COMPILER_EXECUTE_RESULT){
            execute_result(compiler->output_filename);
        }
    }

    if(!(compiler->traits & COMPILER_NO_REMOVE_OBJECT)){
        remove(objfile_filename);
    }
    
    free(objfile_filename);
    free(link_command);
    return SUCCESS;
}
