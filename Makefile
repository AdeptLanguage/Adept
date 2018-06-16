
ifeq ($(OS), Windows_NT)
	CC=x86_64-w64-mingw32-gcc
	LINKER=x86_64-w64-mingw32-g++
	LLVM_LINKER_FLAGS=-LC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/lib
	LLVM_INCLUDE_DIRS=-IC:/.storage/OpenSource/llvm-5.0.0.src/include -IC:/.storage/OpenSource/llvm-5.0.0.src/mingw64-make/include
else
	CC=gcc
	LINKER=g++
endif

# LLVM Flags
LLVM_INCLUDE_FLAGS=$(LLVM_INCLUDE_DIRS) -DNDEBUG -DLLVM_BUILD_GLOBAL_ISEL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# LLVM library dependencies (Probably don't need all of them but whatever)
LLVM_LIBS=-lLLVMCoverage -lgtest_main -lgtest -lLLVMDlltoolDriver -lLLVMLibDriver -lLLVMOption -lLLVMOrcJIT -lLLVMTableGen -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen \
-lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo \
-lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMPowerPCDisassembler \
-lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo \
-lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser \
-lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo \
-lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFDesc -lLLVMBPFInfo \
-lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen \
-lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser \
-lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils -lLLVMTestingSupport -lLLVMInterpreter -lLLVMLineEditor -lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts \
-lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMLinker -lLLVMIRReader -lLLVMObjectYAML -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF \
-lLLVMMIRParser -lLLVMAsmParser -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMDebugInfoCodeView \
-lLLVMDebugInfoMSF -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMBitWriter -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter \
-lLLVMX86Utils -lLLVMMCJIT -lLLVMExecutionEngine -lLLVMTarget -lLLVMAnalysis -lLLVMProfileData -lLLVMRuntimeDyld -lLLVMObject -lLLVMMCParser -lLLVMBitReader -lLLVMMC -lLLVMCore \
-lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle -lpsapi -lshell32 -lole32 -luuid

CFLAGS=-c -Wall -I"include" $(LLVM_INCLUDE_FLAGS) -std=c99 -O3 -fmax-errors=5 -Werror
ADDITIONAL_DEBUG_CFLAGS=-DENABLE_DEBUG_FEATURES -g
LDFLAGS=$(LLVM_LINKER_FLAGS)
SOURCES=src/assemble_expr.c src/assemble_find.c src/assemble_stmt.c src/assemble_type.c src/assemble.c src/ast_expr.c src/ast_type.c src/ast.c src/backend.c src/color.c \
src/compiler.c src/filename.c src/inference.c src/ir_to_llvm.c src/ir.c src/irbuilder.c src/levenshtein.c src/lex.c src/main.c src/memory.c src/parse.c src/pkg.c \
src/pragma.c src/search.c src/token.c src/util.c
ADDITIONAL_DEBUG_SOURCES=src/debug.c
SRCDIR=src
OBJDIR=obj
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEBUG_OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o) $(ADDITIONAL_DEBUG_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)
EXECUTABLE=bin/adept.exe
DEBUG_EXECUTABLE=bin/adept_debug.exe

release: $(SOURCES) $(EXECUTABLE)

debug: $(SOURCES) $(ADDITIONAL_DEBUG_SOURCES) $(DEBUG_EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(LINKER) $(LDFLAGS) $(OBJECTS) $(LLVM_LIBS) -o $@

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

$(DEBUG_EXECUTABLE): $(DEBUG_OBJECTS)
	$(LINKER) $(LDFLAGS) $(DEBUG_OBJECTS) $(LLVM_LIBS) -o $@

$(DEBUG_OBJECTS): $(OBJDIR)/debug/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(ADDITIONAL_DEBUG_CFLAGS) $< -o $@

clean:
ifeq ($(OS), Windows_NT)
	del obj\debug\*.* /Q
	del obj\*.* /Q
	del bin\*.* /Q
else
	rm -f 2> /dev/null obj/debug/*.*
	rm -f 2> /dev/null obj/*.*
	rm -f 2> /dev/null bin/*.*
endif
