
ifeq ($(OS), Windows_NT)
	CC=x86_64-w64-mingw32-gcc
	LINKER=x86_64-w64-mingw32-g++
	LLVM_LINKER_FLAGS=-LC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/lib
	LLVM_INCLUDE_DIRS=-IC:/.storage/OpenSource/llvm-5.0.0.src/include -IC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/include
	RES_C=C:/.storage/MinGW64/bin/windres
	WIN_ICON=obj/icon.res
	WIN_ICON_SRC=resource/icon.rc
	EXECUTABLE=bin/adept.exe
	DEBUG_EXECUTABLE=bin/adept_debug.exe
else
	CC=gcc
	LINKER=g++
	LLVM_LINKER_FLAGS=-L/usr/lib/llvm-5.0/build/lib
	LLVM_INCLUDE_DIRS=-I/usr/lib/llvm-5.0/include -I/usr/lib/llvm-5.0/build/include
	EXECUTABLE=bin/adept
	DEBUG_EXECUTABLE=bin/adept_debug
endif

# LLVM Flags
LLVM_INCLUDE_FLAGS=$(LLVM_INCLUDE_DIRS) -DNDEBUG -DLLVM_BUILD_GLOBAL_ISEL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# -lgtest_main -lgtest -lLLVMTestingSupport

# LLVM library dependencies (Probably don't need all of them but whatever)
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

ifeq ($(OS), Windows_NT)
	LLVM_LIBS += -lpsapi -lshell32 -lole32 -luuid
else
	LLVM_LIBS += -lpthread -lz -lcurses
endif

# -static-libgcc -static-libstdc++ -static

CFLAGS=-c -Wall -I"include" $(LLVM_INCLUDE_FLAGS) -std=c99 -O0 -fmax-errors=5 -Werror
ADDITIONAL_DEBUG_CFLAGS=-DENABLE_DEBUG_FEATURES -g
LDFLAGS=$(LLVM_LINKER_FLAGS) 
SOURCES= src/AST/ast_expr.c src/AST/ast_type.c src/AST/ast.c src/BKEND/backend.c src/BKEND/ir_to_llvm.c src/BRIDGE/any.c src/BRIDGE/bridge.c src/BRIDGE/type_table.c \
	src/DRVR/compiler.c src/DRVR/main.c src/DRVR/object.c src/INFER/infer.c src/IR/ir_pool.c src/IR/ir_type.c src/IR/ir.c src/IRGEN/ir_builder.c \
	src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c src/IRGEN/ir_gen.c \
	src/LEX/lex.c src/LEX/pkg.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_ctx.c src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c src/PARSE/parse_global.c src/PARSE/parse_pragma.c \
	src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c src/PARSE/parse.c src/UTIL/color.c src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/levenshtein.c src/UTIL/memory.c src/UTIL/search.c src/UTIL/util.c
ADDITIONAL_DEBUG_SOURCES=src/DRVR/debug.c
SRCDIR=src
OBJDIR=obj
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEBUG_OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o) $(ADDITIONAL_DEBUG_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)

release: $(SOURCES) $(EXECUTABLE)

debug: $(SOURCES) $(ADDITIONAL_DEBUG_SOURCES) $(DEBUG_EXECUTABLE)

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

clean:
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
