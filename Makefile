
# LLVM Flags
LLVM_INCLUDE_FLAGS=$(LLVM_INCLUDE_DIRS) -DNDEBUG -DLLVM_BUILD_GLOBAL_ISEL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# -lgtest_main -lgtest -lLLVMTestingSupport

# LLVM library dependencies (Probably don't need all of them but whatever)

INSIGHT_OUT_DIR=INSIGHT

ifeq ($(OS), Windows_NT)
	CC=x86_64-w64-mingw32-gcc
	LINKER=x86_64-w64-mingw32-g++

	# Depends on where user has llvm
	LLVM_LINKER_FLAGS=-LC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/lib
	LLVM_INCLUDE_DIRS=-IC:/.storage/OpenSource/llvm-5.0.0.src/include -IC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/include

	RES_C=C:/.storage/MinGW64/bin/windres
	WIN_ICON=obj/icon.res
	WIN_ICON_SRC=resource/icon.rc
	EXECUTABLE=bin/adept.exe
	DEBUG_EXECUTABLE=bin/adept_debug.exe

	# Libraries that will satisfy mingw64 on Windows
	LLVM_LIBS=-lLLVMCoverage -lLLVMDlltoolDriver -lLLVMLibDriver -lLLVMOption -lLLVMOrcJIT -lLLVMTableGen -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen \
		-lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo \
		-lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMPowerPCDisassembler \
		-lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo \
		-lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser \
		-lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo \
		-lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFDesc -lLLVMBPFInfo \
		-lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen \
		-lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser \
		-lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils  -lLLVMInterpreter -lLLVMLineEditor -lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts \
		-lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMLinker -lLLVMIRReader -lLLVMObjectYAML -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF \
		-lLLVMMIRParser -lLLVMAsmParser -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMDebugInfoCodeView \
		-lLLVMDebugInfoMSF -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMBitWriter -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter \
		-lLLVMX86Utils -lLLVMMCJIT -lLLVMExecutionEngine -lLLVMTarget -lLLVMAnalysis -lLLVMProfileData -lLLVMRuntimeDyld -lLLVMObject -lLLVMMCParser -lLLVMBitReader -lLLVMMC -lLLVMCore \
		-lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle -lpsapi -lshell32 -lole32 -luuid
else
	CC=gcc
	LINKER=g++
	EXECUTABLE=bin/adept
	DEBUG_EXECUTABLE=bin/adept_debug
	UNAME_S := $(shell uname -s)

	ifeq ($(UNAME_S),Darwin)
		# Depends on where user has llvm
		LLVM_LINKER_FLAGS=-L/Users/isaac/Projects/llvm-7.0.0.src/build/lib -Wl,-search_paths_first -Wl,-headerpad_max_install_names
		LLVM_INCLUDE_DIRS=-I/Users/isaac/Projects/llvm-7.0.0.src/include -I/Users/isaac/Projects/llvm-7.0.0.src/build/include

		# Libraries that will satisfy clang on Mac
		LLVM_LIBS=-lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF -lLLVMMIRParser -lLLVMFuzzMutate -lLLVMCoverage -lLLVMTableGen -lLLVMDlltoolDriver -lLLVMOrcJIT -lLLVMTestingSupport -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen -lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo -lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMPowerPCDisassembler -lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo -lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser -lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo -lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFAsmParser -lLLVMBPFDesc -lLLVMBPFInfo -lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMARMUtils -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen -lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser -lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils -lLLVMObjectYAML -lLLVMLibDriver -lLLVMOption -lgtest_main -lgtest -lLLVMWindowsManifest -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils -lLLVMMCJIT -lLLVMLineEditor -lLLVMInterpreter -lLLVMExecutionEngine -lLLVMRuntimeDyld -lLLVMCodeGen -lLLVMTarget -lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMScalarOpts -lLLVMLinker -lLLVMIRReader -lLLVMAsmParser -lLLVMInstCombine -lLLVMBitWriter -lLLVMAggressiveInstCombine -lLLVMTransformUtils -lLLVMAnalysis -lLLVMProfileData -lLLVMObject -lLLVMMCParser -lLLVMMC -lLLVMDebugInfoCodeView -lLLVMDebugInfoMSF -lLLVMBitReader -lLLVMCore -lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle -lstdc++
	else
		# /usr/lib/
		LLVM_LINKER_FLAGS=-L/usr/lib/llvm-5.0/build/lib
		LLVM_INCLUDE_DIRS=-I/usr/lib/llvm-5.0/include -I/usr/lib/llvm-5.0/build/include

		# Libraries that will probably satisfy what the compiler needs
		LLVM_LIBS=-lLLVMCoverage -lLLVMDlltoolDriver -lLLVMLibDriver -lLLVMOption -lLLVMOrcJIT -lLLVMTableGen -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen \
			-lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo \
			-lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMPowerPCDisassembler \
			-lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo \
			-lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser \
			-lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo \
			-lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFDesc -lLLVMBPFInfo \
			-lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen \
			-lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser \
			-lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils  -lLLVMInterpreter -lLLVMLineEditor -lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts \
			-lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMLinker -lLLVMIRReader -lLLVMObjectYAML -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF \
			-lLLVMMIRParser -lLLVMAsmParser -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMDebugInfoCodeView \
			-lLLVMDebugInfoMSF -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMBitWriter -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter \
			-lLLVMX86Utils -lLLVMMCJIT -lLLVMExecutionEngine -lLLVMTarget -lLLVMAnalysis -lLLVMProfileData -lLLVMRuntimeDyld -lLLVMObject -lLLVMMCParser -lLLVMBitReader -lLLVMMC -lLLVMCore \
			-lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle
	endif

	LLVM_LIBS += -lpthread -lz -lcurses
endif

# -static-libgcc -static-libstdc++ -static

CFLAGS=-c -Wall -I"include" $(LLVM_INCLUDE_FLAGS) -std=c99 -O0 -DNDEBUG # -fmax-errors=5 -Werror
ADDITIONAL_DEBUG_CFLAGS=-DENABLE_DEBUG_FEATURES -g # -fprofile-instr-generate -fcoverage-mapping
LDFLAGS=$(LLVM_LINKER_FLAGS) # -fprofile-instr-generate
SOURCES= src/AST/ast_expr.c src/AST/ast_type.c src/AST/ast.c src/AST/meta_directives.c src/BKEND/backend.c src/BKEND/ir_to_llvm.c src/BRIDGE/any.c src/BRIDGE/bridge.c src/BRIDGE/type_table.c \
	src/BRIDGE/rtti.c src/DRVR/compiler.c src/DRVR/main.c src/DRVR/object.c src/INFER/infer.c src/IR/ir_pool.c src/IR/ir_type_var.c src/IR/ir_type.c src/IR/ir.c src/IRGEN/ir_builder.c \
	src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c src/IRGEN/ir_gen.c \
	src/LEX/lex.c src/LEX/pkg.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_ctx.c src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c src/PARSE/parse_global.c src/PARSE/parse_meta.c src/PARSE/parse_pragma.c \
	src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c src/PARSE/parse.c src/UTIL/color.c src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/improved_qsort.c src/UTIL/levenshtein.c src/UTIL/memory.c src/UTIL/search.c src/UTIL/util.c
ADDITIONAL_DEBUG_SOURCES=src/DRVR/debug.c
SRCDIR=src
OBJDIR=obj
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEBUG_OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o) $(ADDITIONAL_DEBUG_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)

release: $(SOURCES) $(EXECUTABLE)

debug: $(SOURCES) $(ADDITIONAL_DEBUG_SOURCES) $(DEBUG_EXECUTABLE)

insight: $(SOURCES)
	@mkdir -p $(INSIGHT_OUT_DIR)
	@mkdir -p $(INSIGHT_OUT_DIR)/include
	@mkdir -p $(INSIGHT_OUT_DIR)/include/AST
	@mkdir -p $(INSIGHT_OUT_DIR)/include/BRIDGE
	@mkdir -p $(INSIGHT_OUT_DIR)/include/DRVR
	@mkdir -p $(INSIGHT_OUT_DIR)/include/LEX
	@mkdir -p $(INSIGHT_OUT_DIR)/include/PARSE
	@mkdir -p $(INSIGHT_OUT_DIR)/include/UTIL
	@mkdir -p $(INSIGHT_OUT_DIR)/src
	@mkdir -p $(INSIGHT_OUT_DIR)/src/AST
	@mkdir -p $(INSIGHT_OUT_DIR)/src/BRIDGE
	@mkdir -p $(INSIGHT_OUT_DIR)/src/DRVR
	@mkdir -p $(INSIGHT_OUT_DIR)/src/LEX
	@mkdir -p $(INSIGHT_OUT_DIR)/src/PARSE
	@mkdir -p $(INSIGHT_OUT_DIR)/src/UTIL
	
#   Insight - Required Header Files
	@cp include/AST/ast_expr.h $(INSIGHT_OUT_DIR)/include/AST/ast_expr.h
	@cp include/AST/ast_type.h $(INSIGHT_OUT_DIR)/include/AST/ast_type.h
	@cp include/AST/ast.h $(INSIGHT_OUT_DIR)/include/AST/ast.h
	@cp include/AST/meta_directives.h $(INSIGHT_OUT_DIR)/include/AST/meta_directives.h
	@cp include/BRIDGE/any.h $(INSIGHT_OUT_DIR)/include/BRIDGE/any.h
	@cp include/BRIDGE/bridge.h $(INSIGHT_OUT_DIR)/include/BRIDGE/bridge.h
	@cp include/BRIDGE/type_table.h $(INSIGHT_OUT_DIR)/include/BRIDGE/type_table.h
	@cp include/DRVR/compiler.h $(INSIGHT_OUT_DIR)/include/DRVR/compiler.h
	@cp include/DRVR/object.h $(INSIGHT_OUT_DIR)/include/DRVR/object.h
	@cp include/LEX/lex.h $(INSIGHT_OUT_DIR)/include/LEX/lex.h
	@cp include/LEX/pkg.h $(INSIGHT_OUT_DIR)/include/LEX/pkg.h
	@cp include/LEX/token.h $(INSIGHT_OUT_DIR)/include/LEX/token.h
	@cp include/PARSE/parse_alias.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_alias.h
	@cp include/PARSE/parse_ctx.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_ctx.h
	@cp include/PARSE/parse_dependency.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_dependency.h
	@cp include/PARSE/parse_enum.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_enum.h
	@cp include/PARSE/parse_expr.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_expr.h
	@cp include/PARSE/parse_func.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_func.h
	@cp include/PARSE/parse_global.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_global.h
	@cp include/PARSE/parse_meta.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_meta.h
	@cp include/PARSE/parse_pragma.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_pragma.h
	@cp include/PARSE/parse_stmt.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_stmt.h
	@cp include/PARSE/parse_struct.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_struct.h
	@cp include/PARSE/parse_type.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_type.h
	@cp include/PARSE/parse_util.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_util.h
	@cp include/PARSE/parse.h $(INSIGHT_OUT_DIR)/include/PARSE/parse.h
	@cp include/UTIL/__insight_overloads.h $(INSIGHT_OUT_DIR)/include/UTIL/__insight_overloads.h
	@cp include/UTIL/__insight_undo_overloads.h $(INSIGHT_OUT_DIR)/include/UTIL/__insight_undo_overloads.h
	@cp include/UTIL/builtin_type.h $(INSIGHT_OUT_DIR)/include/UTIL/builtin_type.h
	@cp include/UTIL/color.h $(INSIGHT_OUT_DIR)/include/UTIL/color.h
	@cp include/UTIL/filename.h $(INSIGHT_OUT_DIR)/include/UTIL/filename.h
	@cp include/UTIL/ground.h $(INSIGHT_OUT_DIR)/include/UTIL/ground.h
	@cp include/UTIL/levenshtein.h $(INSIGHT_OUT_DIR)/include/UTIL/levenshtein.h
	@cp include/UTIL/memory.h $(INSIGHT_OUT_DIR)/include/UTIL/memory.h
	@cp include/UTIL/search.h $(INSIGHT_OUT_DIR)/include/UTIL/search.h
	@cp include/UTIL/trait.h $(INSIGHT_OUT_DIR)/include/UTIL/trait.h
	@cp include/UTIL/util.h $(INSIGHT_OUT_DIR)/include/UTIL/util.h
	
#   Insight - Required Source Code Files
	@cp src/AST/ast_expr.c $(INSIGHT_OUT_DIR)/src/AST/ast_expr.c
	@cp src/AST/ast_type.c $(INSIGHT_OUT_DIR)/src/AST/ast_type.c
	@cp src/AST/ast.c $(INSIGHT_OUT_DIR)/src/AST/ast.c
	@cp src/AST/meta_directives.c $(INSIGHT_OUT_DIR)/src/AST/meta_directives.c
	@cp src/BRIDGE/any.c $(INSIGHT_OUT_DIR)/src/BRIDGE/any.c
	@cp src/BRIDGE/bridge.c $(INSIGHT_OUT_DIR)/src/BRIDGE/bridge.c
	@cp src/BRIDGE/type_table.c $(INSIGHT_OUT_DIR)/src/BRIDGE/type_table.c
	@cp src/DRVR/compiler.c $(INSIGHT_OUT_DIR)/src/DRVR/compiler.c
	@cp src/DRVR/object.c $(INSIGHT_OUT_DIR)/src/DRVR/object.c
	@cp src/LEX/lex.c $(INSIGHT_OUT_DIR)/src/LEX/lex.c
	@cp src/LEX/pkg.c $(INSIGHT_OUT_DIR)/src/LEX/pkg.c
	@cp src/LEX/token.c $(INSIGHT_OUT_DIR)/src/LEX/token.c
	@cp src/PARSE/parse_alias.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_alias.c
	@cp src/PARSE/parse_ctx.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_ctx.c
	@cp src/PARSE/parse_dependency.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_dependency.c
	@cp src/PARSE/parse_enum.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_enum.c
	@cp src/PARSE/parse_expr.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_expr.c
	@cp src/PARSE/parse_func.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_func.c
	@cp src/PARSE/parse_global.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_global.c
	@cp src/PARSE/parse_meta.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_meta.c
	@cp src/PARSE/parse_pragma.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_pragma.c
	@cp src/PARSE/parse_stmt.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_stmt.c
	@cp src/PARSE/parse_struct.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_struct.c
	@cp src/PARSE/parse_type.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_type.c
	@cp src/PARSE/parse_util.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_util.c
	@cp src/PARSE/parse.c $(INSIGHT_OUT_DIR)/src/PARSE/parse.c
	@cp src/UTIL/__insight_overloads.c $(INSIGHT_OUT_DIR)/src/UTIL/__insight_overloads.c
	@cp src/UTIL/builtin_type.c $(INSIGHT_OUT_DIR)/src/UTIL/builtin_type.c
	@cp src/UTIL/color.c $(INSIGHT_OUT_DIR)/src/UTIL/color.c
	@cp src/UTIL/filename.c $(INSIGHT_OUT_DIR)/src/UTIL/filename.c
	@cp src/UTIL/levenshtein.c $(INSIGHT_OUT_DIR)/src/UTIL/levenshtein.c
	@cp src/UTIL/search.c $(INSIGHT_OUT_DIR)/src/UTIL/search.c
	@cp src/UTIL/util.c $(INSIGHT_OUT_DIR)/src/UTIL/util.c

	@cp include/UTIL/__insight.h $(INSIGHT_OUT_DIR)/insight.h
	@cp src/UTIL/__insight_Makefile $(INSIGHT_OUT_DIR)/Makefile
	@echo Insight files copied into: $(INSIGHT_OUT_DIR)/

ifeq ($(OS), Windows_NT)
$(EXECUTABLE): $(OBJECTS) $(WIN_ICON)
	@if not exist bin mkdir bin
	$(LINKER) $(LDFLAGS) $(OBJECTS) $(WIN_ICON) $(LLVM_LIBS) -o $@
else
$(EXECUTABLE): $(OBJECTS)
	@mkdir -p bin
	$(LINKER) $(LDFLAGS) $(OBJECTS) $(LLVM_LIBS) -o $@
endif

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
ifeq ($(OS), Windows_NT)
	@if not exist obj mkdir obj
	@if not exist "$(@D)" mkdir "$(@D)"
else
	@mkdir -p "$(@D)"
endif
	$(CC) $(CFLAGS) $< -o $@

ifeq ($(OS), Windows_NT)
$(DEBUG_EXECUTABLE): $(DEBUG_OBJECTS) $(WIN_ICON)
	@if not exist bin mkdir bin
	$(LINKER) $(LDFLAGS) $(DEBUG_OBJECTS) $(WIN_ICON) $(LLVM_LIBS) -o $@
else
$(DEBUG_EXECUTABLE): $(DEBUG_OBJECTS)
	@mkdir -p bin
	$(LINKER) $(LDFLAGS) $(DEBUG_OBJECTS) $(LLVM_LIBS) -o $@
endif

$(DEBUG_OBJECTS): $(OBJDIR)/debug/%.o : $(SRCDIR)/%.c
ifeq ($(OS), Windows_NT)
	@if not exist obj mkdir obj
	@if not exist obj\debug mkdir obj\debug
	@if not exist "$(@D)" mkdir "$(@D)"
else
	@mkdir -p "$(@D)"
endif
	$(CC) $(CFLAGS) $(ADDITIONAL_DEBUG_CFLAGS) $< -o $@

$(WIN_ICON):
	$(RES_C) -J rc -O coff -i $(WIN_ICON_SRC) -o $(WIN_ICON)

clean: rm_insight
ifeq ($(OS), Windows_NT)
	@del obj\debug\*.* /Q /S 1> nul 2>&1
	@del obj\*.* /S /Q 1> nul 2>&1
	@del bin\adept.exe /S /Q 1> nul 2>&1
	@del bin\adept_debug.exe /S /Q 1> nul 2>&1
else
	@find . -name "*.o" -type f -delete 2> /dev/null
	@rm -rf 2> /dev/null bin/adept
	@rm -rf 2> /dev/null bin/adept_debug
endif

rm_insight:
ifeq ($(OS), Windows_NT)
	@if exist $(INSIGHT_OUT_DIR) rmdir /s /q $(INSIGHT_OUT_DIR)
else
	@rm -rf $(INSIGHT_OUT_DIR)
endif
