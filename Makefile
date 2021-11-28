
# --------------------------- Change These Values ---------------------------
# Whether to include support for built-in adept package manager
# NOTE: Requires libcurl fields to be filled out
# NOTE: Default is 'true'
ENABLE_ADEPT_PACKAGE_MANAGER=true

# --------------------------- Change These Values ---------------------------
# NOTE: For Windows, ensure mingw32-make.exe uses cmd.exe for shell
# (see HOW_TO_COMPILE.md for details)
WINDOWS_CC=x86_64-w64-mingw32-gcc
WINDOWS_CXX=x86_64-w64-mingw32-g++
WINDOWS_WINRES=C:/Users/isaac/Projects/mingw64/bin/windres
WINDOWS_LLVM_LIB=C:/Users/isaac/Projects/llvm-7.0.0.src/mingw-release/lib
WINDOWS_LLVM_INCLUDE=C:/Users/isaac/Projects/llvm-7.0.0.src/include
WINDOWS_LLVM_BUILD_INCLUDE=C:/Users/isaac/Projects/llvm-7.0.0.src/mingw-release/include
WINDOWS_LIBCURL_LIB=C:/Users/isaac/Projects/curl-7.79.1_4-win64-mingw/curl-7.79.1-win64-mingw/lib
WINDOWS_LIBCURL_INCLUDE=C:/Users/isaac/Projects/curl-7.79.1_4-win64-mingw/curl-7.79.1-win64-mingw/include
WINDOWS_LIBCURL_BUILD_INCLUDE=
UNIX_CC=gcc
UNIX_CXX=g++
UNIX_LLVM_LIB=/opt/homebrew/opt/llvm/lib
UNIX_LLVM_INCLUDE=/opt/homebrew/opt/llvm/include
UNIX_LLVM_BUILD_INCLUDE=
UNIX_LIBCURL_LIB=/opt/homebrew/opt/curl/lib
UNIX_LIBCURL_INCLUDE=/opt/homebrew/opt/curl/lib
UNIX_LIBCURL_BUILD_INCLUDE=
DEBUG_ADDRESS_SANITIZE=true
#UNIX_CC=gcc
#UNIX_CXX=g++
#UNIX_LLVM_LIB=/usr/lib/llvm-7/lib
#UNIX_LLVM_INCLUDE=/usr/include/llvm-7 -I/usr/include/llvm-c-7
#UNIX_LLVM_BUILD_INCLUDE=/usr/lib/llvm-7/include

ifeq ($(DEBUG_ADDRESS_SANITIZE),true)
	WINDOWS_CC=clang
	WINDOWS_CXX=clang++
	UNIX_CC=clang
	UNIX_CXX=clang++
endif

INSIGHT_OUT_DIR=INSIGHT
# ---------------------------------------------------------------------------

ifeq ($(OS), Windows_NT)
	CC=$(WINDOWS_CC)
	LINKER=$(WINDOWS_CXX)

	# Depends on where user has llvm
	LLVM_LINKER_FLAGS=-L$(WINDOWS_LLVM_LIB)
	LLVM_INCLUDE_DIRS=-I$(WINDOWS_LLVM_INCLUDE) -I$(WINDOWS_LLVM_BUILD_INCLUDE)

	WIN_ICON=obj/icon.res
	WIN_ICON_SRC=resource/icon.rc
	EXECUTABLE=bin/adept.exe
	DEBUG_EXECUTABLE=bin/adept_debug.exe
	UNITTEST_EXECUTABLE=bin/adept_unittest.exe

	# Libraries that will satisfy mingw64 on Windows
	LLVM_LIBS=-lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts -lLLVMSymbolize -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF -lLLVMMIRParser -lLLVMFuzzMutate -lLLVMCoverage -lLLVMTableGen -lLLVMDlltoolDriver -lLLVMOrcJIT -lLLVMTestingSupport -lLLVMXCoreDisassembler -lLLVMXCoreCodeGen -lLLVMXCoreDesc -lLLVMXCoreInfo -lLLVMXCoreAsmPrinter -lLLVMSystemZDisassembler -lLLVMSystemZCodeGen -lLLVMSystemZAsmParser -lLLVMSystemZDesc -lLLVMSystemZInfo -lLLVMSystemZAsmPrinter -lLLVMSparcDisassembler -lLLVMSparcCodeGen -lLLVMSparcAsmParser -lLLVMSparcDesc -lLLVMSparcInfo -lLLVMSparcAsmPrinter -lLLVMPowerPCDisassembler -lLLVMPowerPCCodeGen -lLLVMPowerPCAsmParser -lLLVMPowerPCDesc -lLLVMPowerPCInfo -lLLVMPowerPCAsmPrinter -lLLVMNVPTXCodeGen -lLLVMNVPTXDesc -lLLVMNVPTXInfo -lLLVMNVPTXAsmPrinter -lLLVMMSP430CodeGen -lLLVMMSP430Desc -lLLVMMSP430Info -lLLVMMSP430AsmPrinter -lLLVMMipsDisassembler -lLLVMMipsCodeGen -lLLVMMipsAsmParser -lLLVMMipsDesc -lLLVMMipsInfo -lLLVMMipsAsmPrinter -lLLVMLanaiDisassembler -lLLVMLanaiCodeGen -lLLVMLanaiAsmParser -lLLVMLanaiDesc -lLLVMLanaiAsmPrinter -lLLVMLanaiInfo -lLLVMHexagonDisassembler -lLLVMHexagonCodeGen -lLLVMHexagonAsmParser -lLLVMHexagonDesc -lLLVMHexagonInfo -lLLVMBPFDisassembler -lLLVMBPFCodeGen -lLLVMBPFAsmParser -lLLVMBPFDesc -lLLVMBPFInfo -lLLVMBPFAsmPrinter -lLLVMARMDisassembler -lLLVMARMCodeGen -lLLVMARMAsmParser -lLLVMARMDesc -lLLVMARMInfo -lLLVMARMAsmPrinter -lLLVMARMUtils -lLLVMAMDGPUDisassembler -lLLVMAMDGPUCodeGen -lLLVMAMDGPUAsmParser -lLLVMAMDGPUDesc -lLLVMAMDGPUInfo -lLLVMAMDGPUAsmPrinter -lLLVMAMDGPUUtils -lLLVMAArch64Disassembler -lLLVMAArch64CodeGen -lLLVMAArch64AsmParser -lLLVMAArch64Desc -lLLVMAArch64Info -lLLVMAArch64AsmPrinter -lLLVMAArch64Utils -lLLVMObjectYAML -lLLVMLibDriver -lLLVMOption -lgtest_main -lgtest -lLLVMWindowsManifest -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils -lLLVMMCJIT -lLLVMLineEditor -lLLVMInterpreter -lLLVMExecutionEngine -lLLVMRuntimeDyld -lLLVMCodeGen -lLLVMTarget -lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMScalarOpts -lLLVMLinker -lLLVMIRReader -lLLVMAsmParser -lLLVMInstCombine -lLLVMBitWriter -lLLVMAggressiveInstCombine -lLLVMTransformUtils -lLLVMAnalysis -lLLVMProfileData -lLLVMObject -lLLVMMCParser -lLLVMMC -lLLVMDebugInfoCodeView -lLLVMDebugInfoMSF -lLLVMBitReader -lLLVMCore -lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle -lz -lpsapi -lshell32 -lole32 -luuid -ladvapi32

	# Libcurl info
	ifeq ($(ENABLE_ADEPT_PACKAGE_MANAGER),true)
		LIBCURL_INCLUDE_FLAGS=-I$(WINDOWS_LIBCURL_INCLUDE) -I$(WINDOWS_LIBCURL_BULID_INCLUDE)
		LIBCURL_LINKER_FLAGS=-L$(WINDOWS_LIBCURL_LIB)
		LIBCURL_LIBS=-lcurl # -lz
	endif
else
	CC=$(UNIX_CC)
	LINKER=$(UNIX_CXX)
	EXECUTABLE=bin/adept
	DEBUG_EXECUTABLE=bin/adept_debug
	UNITTEST_EXECUTABLE=bin/adept_unittest
	UNAME_S := $(shell uname -s)
	llvm_config=$(UNIX_LLVM_LIB)/../bin/llvm-config

	ifeq ($(UNAME_S),Darwin)
		# Depends on where user has llvm
		LLVM_LINKER_FLAGS=-L$(UNIX_LLVM_LIB) -Wl,-search_paths_first -Wl,-headerpad_max_install_names
		LLVM_INCLUDE_DIRS=-I$(UNIX_LLVM_INCLUDE) -I$(UNIX_LLVM_BUILD_INCLUDE)

		# Libraries that will satisfy clang on Mac
		LLVM_LIBS=$(shell $(llvm_config) --libs --link-static)
	else
		# /usr/lib/
		LLVM_LINKER_FLAGS=-L$(UNIX_LLVM_LIB)
		LLVM_INCLUDE_DIRS=-I$(UNIX_LLVM_INCLUDE) -I$(UNIX_LLVM_BUILD_INCLUDE)

		LIBCURL_INCLUDE_FLAGS=-I$(UNIX_LIBCURL_INCLUDE) -I$(UNIX_LIBCURL_BULID_INCLUDE)
	endif

	LLVM_LIBS=$(shell $(llvm_config) --libs --link-static)
	LLVM_LIBS += -lpthread -lz -lcurses

	# Libcurl info
	ifeq ($(ENABLE_ADEPT_PACKAGE_MANAGER),true)
		LIBCURL_INCLUDE_FLAGS=-I$(UNIX_LIBCURL_INCLUDE) -I$(UNIX_LIBCURL_BULID_INCLUDE)
		LIBCURL_LINKER_FLAGS=-L$(UNIX_LIBCURL_LIB)
		LIBCURL_LIBS=-lcurl -lldap # -lz
	endif
endif

# LLVM Flags
LLVM_INCLUDE_FLAGS=$(LLVM_INCLUDE_DIRS) -DNDEBUG -DLLVM_BUILD_GLOBAL_ISEL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
# -lgtest_main -lgtest -lLLVMTestingSupport

# -static-libgcc -static-libstdc++ -static
CFLAGS=-c -Wall -Wextra -I"include" $(LLVM_INCLUDE_FLAGS) $(LIBCURL_INCLUDE_FLAGS) -std=gnu99 -Wall -O0 -DNDEBUG # -fmax-errors=5 -Werror
ADDITIONAL_DEBUG_CFLAGS=-DENABLE_DEBUG_FEATURES -g

ifeq ($(ENABLE_ADEPT_PACKAGE_MANAGER),true)
	CFLAGS+= -DADEPT_ENABLE_PACKAGE_MANAGER
endif

LDFLAGS=$(LIBCURL_LINKER_FLAGS) $(LLVM_LINKER_FLAGS)

ifeq ($(DEBUG_ADDRESS_SANITIZE),true)
	CFLAGS+= -fsanitize=address,undefined
	LDFLAGS+= -fsanitize=address,undefined
endif

ESSENTIAL_SOURCES= src/AST/ast_constant.c src/AST/ast_expr.c src/AST/ast_layout.c src/AST/ast_type.c src/AST/ast.c \
	src/AST/meta_directives.c src/BKEND/backend.c src/BKEND/ir_to_llvm.c src/BRIDGE/any.c \
	src/BRIDGE/bridge.c src/BRIDGE/funcpair.c src/BRIDGE/type_table.c src/BRIDGE/rtti.c src/DRVR/compiler.c \
	src/DRVR/config.c src/DRVR/object.c src/DRVR/repl.c src/INFER/infer.c \
	src/IR/ir_pool.c src/IR/ir_type.c src/IR/ir_type_spec.c src/IR/ir.c src/IR/ir_lowering.c \
	src/IRGEN/ir_builder.c src/IRGEN/ir_cache.c src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find.c \
	src/IRGEN/ir_gen_rtti.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c src/IRGEN/ir_gen.c \
	src/LEX/lex.c src/LEX/pkg.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_ctx.c \
	src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c \
	src/PARSE/parse_global.c src/PARSE/parse_meta.c src/PARSE/parse_namespace.c src/PARSE/parse_pragma.c \
	src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c \
	src/PARSE/parse.c src/TOKEN/token_data.c src/UTIL/color.c src/UTIL/datatypes.c src/UTIL/download.c \
	src/UTIL/aes.c src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/hash.c src/UTIL/jsmn_helper.c src/UTIL/levenshtein.c \
	src/UTIL/memory.c src/UTIL/search.c src/UTIL/stash.c src/UTIL/string_builder.c src/UTIL/tmpbuf.c src/UTIL/util.c
SOURCES= $(ESSENTIAL_SOURCES) src/DRVR/main.c
ADDITIONAL_DEBUG_SOURCES=src/DRVR/debug.c
SRCDIR=src
OBJDIR=obj
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ESSENTIAL_OBJECTS=$(ESSENTIAL_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)
DEBUG_OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o) $(ADDITIONAL_DEBUG_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)
SUITE_NAMES=CuSuite_for_ast_expr.c CuSuite_for_lex.c
SUITE_SOURCES=$(SUITE_NAMES:%.c=unittests/src/%.c)
SUITE_OBJECTS=$(SUITE_SOURCES:unittests/src/%.c=unittests/obj/%.o)

release: $(SOURCES) $(EXECUTABLE)
test: $(ESSENTIAL_SOURCES) $(UNITTEST_EXECUTABLE) unittest

debug: $(SOURCES) $(ADDITIONAL_DEBUG_SOURCES) $(DEBUG_EXECUTABLE)

insight: $(SOURCES)
	@mkdir -p $(INSIGHT_OUT_DIR)
	@mkdir -p $(INSIGHT_OUT_DIR)/include
	@mkdir -p $(INSIGHT_OUT_DIR)/include/AST
	@mkdir -p $(INSIGHT_OUT_DIR)/include/BRIDGE
	@mkdir -p $(INSIGHT_OUT_DIR)/include/DRVR
	@mkdir -p $(INSIGHT_OUT_DIR)/include/LEX
	@mkdir -p $(INSIGHT_OUT_DIR)/include/TOKEN
	@mkdir -p $(INSIGHT_OUT_DIR)/include/PARSE
	@mkdir -p $(INSIGHT_OUT_DIR)/include/UTIL
	@mkdir -p $(INSIGHT_OUT_DIR)/src
	@mkdir -p $(INSIGHT_OUT_DIR)/src/AST
	@mkdir -p $(INSIGHT_OUT_DIR)/src/BRIDGE
	@mkdir -p $(INSIGHT_OUT_DIR)/src/DRVR
	@mkdir -p $(INSIGHT_OUT_DIR)/src/LEX
	@mkdir -p $(INSIGHT_OUT_DIR)/src/TOKEN
	@mkdir -p $(INSIGHT_OUT_DIR)/src/PARSE
	@mkdir -p $(INSIGHT_OUT_DIR)/src/UTIL
	
#   Insight - Required Header Files
	@cp include/AST/ast_constant.h $(INSIGHT_OUT_DIR)/include/AST/ast_constant.h
	@cp include/AST/ast_expr.h $(INSIGHT_OUT_DIR)/include/AST/ast_expr.h
	@cp include/AST/ast_layout.h $(INSIGHT_OUT_DIR)/include/AST/ast_layout.h
	@cp include/AST/ast_type.h $(INSIGHT_OUT_DIR)/include/AST/ast_type.h
	@cp include/AST/ast_type_lean.h $(INSIGHT_OUT_DIR)/include/AST/ast_type_lean.h
	@cp include/AST/ast.h $(INSIGHT_OUT_DIR)/include/AST/ast.h
	@cp include/AST/meta_directives.h $(INSIGHT_OUT_DIR)/include/AST/meta_directives.h
	@cp include/BRIDGE/any.h $(INSIGHT_OUT_DIR)/include/BRIDGE/any.h
	@cp include/BRIDGE/bridge.h $(INSIGHT_OUT_DIR)/include/BRIDGE/bridge.h
	@cp include/BRIDGE/type_table.h $(INSIGHT_OUT_DIR)/include/BRIDGE/type_table.h
	@cp include/DRVR/compiler.h $(INSIGHT_OUT_DIR)/include/DRVR/compiler.h
	@cp include/DRVR/config.h $(INSIGHT_OUT_DIR)/include/DRVR/config.h
	@cp include/DRVR/object.h $(INSIGHT_OUT_DIR)/include/DRVR/object.h
	@cp include/LEX/lex.h $(INSIGHT_OUT_DIR)/include/LEX/lex.h
	@cp include/LEX/pkg.h $(INSIGHT_OUT_DIR)/include/LEX/pkg.h
	@cp include/LEX/token.h $(INSIGHT_OUT_DIR)/include/LEX/token.h
	@cp include/TOKEN/token_data.h $(INSIGHT_OUT_DIR)/include/TOKEN/token_data.h
	@cp include/PARSE/parse_alias.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_alias.h
	@cp include/PARSE/parse_ctx.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_ctx.h
	@cp include/PARSE/parse_dependency.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_dependency.h
	@cp include/PARSE/parse_enum.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_enum.h
	@cp include/PARSE/parse_expr.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_expr.h
	@cp include/PARSE/parse_func.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_func.h
	@cp include/PARSE/parse_global.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_global.h
	@cp include/PARSE/parse_meta.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_meta.h
	@cp include/PARSE/parse_namespace.h $(INSIGHT_OUT_DIR)/include/PARSE/parse_namespace.h
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
	@cp include/UTIL/datatypes.h $(INSIGHT_OUT_DIR)/include/UTIL/datatypes.h
	@cp include/UTIL/download.h $(INSIGHT_OUT_DIR)/include/UTIL/download.h
	@cp include/UTIL/filename.h $(INSIGHT_OUT_DIR)/include/UTIL/filename.h
	@cp include/UTIL/hash.h $(INSIGHT_OUT_DIR)/include/UTIL/hash.h
	@cp include/UTIL/jsmn.h $(INSIGHT_OUT_DIR)/include/UTIL/jsmn.h
	@cp include/UTIL/jsmn_helper.h $(INSIGHT_OUT_DIR)/include/UTIL/jsmn_helper.h
	@cp include/UTIL/ground.h $(INSIGHT_OUT_DIR)/include/UTIL/ground.h
	@cp include/UTIL/levenshtein.h $(INSIGHT_OUT_DIR)/include/UTIL/levenshtein.h
	@cp include/UTIL/memory.h $(INSIGHT_OUT_DIR)/include/UTIL/memory.h
	@cp include/UTIL/search.h $(INSIGHT_OUT_DIR)/include/UTIL/search.h
	@cp include/UTIL/trait.h $(INSIGHT_OUT_DIR)/include/UTIL/trait.h
	@cp include/UTIL/string_builder.h $(INSIGHT_OUT_DIR)/include/UTIL/string_builder.h
	@cp include/UTIL/tmpbuf.h $(INSIGHT_OUT_DIR)/include/UTIL/tmpbuf.h
	@cp include/UTIL/util.h $(INSIGHT_OUT_DIR)/include/UTIL/util.h
	
#   Insight - Required Source Code Files
	@cp src/AST/ast_constant.c $(INSIGHT_OUT_DIR)/src/AST/ast_constant.c
	@cp src/AST/ast_expr.c $(INSIGHT_OUT_DIR)/src/AST/ast_expr.c
	@cp src/AST/ast_layout.c $(INSIGHT_OUT_DIR)/src/AST/ast_layout.c
	@cp src/AST/ast_type.c $(INSIGHT_OUT_DIR)/src/AST/ast_type.c
	@cp src/AST/ast.c $(INSIGHT_OUT_DIR)/src/AST/ast.c
	@cp src/AST/meta_directives.c $(INSIGHT_OUT_DIR)/src/AST/meta_directives.c
	@cp src/BRIDGE/any.c $(INSIGHT_OUT_DIR)/src/BRIDGE/any.c
	@cp src/BRIDGE/bridge.c $(INSIGHT_OUT_DIR)/src/BRIDGE/bridge.c
	@cp src/BRIDGE/type_table.c $(INSIGHT_OUT_DIR)/src/BRIDGE/type_table.c
	@cp src/DRVR/compiler.c $(INSIGHT_OUT_DIR)/src/DRVR/compiler.c
	@cp src/DRVR/config.c $(INSIGHT_OUT_DIR)/src/DRVR/config.c
	@cp src/DRVR/object.c $(INSIGHT_OUT_DIR)/src/DRVR/object.c
	@cp src/LEX/lex.c $(INSIGHT_OUT_DIR)/src/LEX/lex.c
	@cp src/LEX/pkg.c $(INSIGHT_OUT_DIR)/src/LEX/pkg.c
	@cp src/LEX/token.c $(INSIGHT_OUT_DIR)/src/LEX/token.c
	@cp src/TOKEN/token_data.c $(INSIGHT_OUT_DIR)/src/TOKEN/token_data.c
	@cp src/PARSE/parse_alias.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_alias.c
	@cp src/PARSE/parse_ctx.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_ctx.c
	@cp src/PARSE/parse_dependency.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_dependency.c
	@cp src/PARSE/parse_enum.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_enum.c
	@cp src/PARSE/parse_expr.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_expr.c
	@cp src/PARSE/parse_func.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_func.c
	@cp src/PARSE/parse_global.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_global.c
	@cp src/PARSE/parse_meta.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_meta.c
	@cp src/PARSE/parse_namespace.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_namespace.c
	@cp src/PARSE/parse_pragma.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_pragma.c
	@cp src/PARSE/parse_stmt.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_stmt.c
	@cp src/PARSE/parse_struct.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_struct.c
	@cp src/PARSE/parse_type.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_type.c
	@cp src/PARSE/parse_util.c $(INSIGHT_OUT_DIR)/src/PARSE/parse_util.c
	@cp src/PARSE/parse.c $(INSIGHT_OUT_DIR)/src/PARSE/parse.c
	@cp src/UTIL/__insight_overloads.c $(INSIGHT_OUT_DIR)/src/UTIL/__insight_overloads.c
	@cp src/UTIL/builtin_type.c $(INSIGHT_OUT_DIR)/src/UTIL/builtin_type.c
	@cp src/UTIL/color.c $(INSIGHT_OUT_DIR)/src/UTIL/color.c
	@cp src/UTIL/datatypes.c $(INSIGHT_OUT_DIR)/src/UTIL/datatypes.c
	@cp src/UTIL/filename.c $(INSIGHT_OUT_DIR)/src/UTIL/filename.c
	@cp src/UTIL/hash.c $(INSIGHT_OUT_DIR)/src/UTIL/hash.c
	@cp src/UTIL/jsmn_helper.c $(INSIGHT_OUT_DIR)/src/UTIL/jsmn_helper.c
	@cp src/UTIL/levenshtein.c $(INSIGHT_OUT_DIR)/src/UTIL/levenshtein.c
	@cp src/UTIL/search.c $(INSIGHT_OUT_DIR)/src/UTIL/search.c
	@cp src/UTIL/string_builder.c $(INSIGHT_OUT_DIR)/src/UTIL/string_builder.c
	@cp src/UTIL/tmpbuf.c $(INSIGHT_OUT_DIR)/src/UTIL/tmpbuf.c
	@cp src/UTIL/util.c $(INSIGHT_OUT_DIR)/src/UTIL/util.c

	@cp include/UTIL/__insight.h $(INSIGHT_OUT_DIR)/insight.h
	@cp src/UTIL/__insight_Makefile $(INSIGHT_OUT_DIR)/Makefile
	@echo Insight files copied into: $(INSIGHT_OUT_DIR)/

ifeq ($(OS), Windows_NT)
$(EXECUTABLE): $(OBJECTS) $(WIN_ICON)
	@if not exist bin mkdir bin
	$(LINKER) $(LDFLAGS) $(OBJECTS) $(WIN_ICON) $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@

$(UNITTEST_EXECUTABLE): $(ESSENTIAL_OBJECTS) obj/debug/DRVR/debug.o $(SUITE_OBJECTS) unittests/obj/all.o unittests/obj/CuTest.o
	@if not exist bin mkdir bin
	$(LINKER) $(LDFLAGS) $(ESSENTIAL_OBJECTS) obj/debug/DRVR/debug.o $(SUITE_OBJECTS) unittests/obj/all.o unittests/obj/CuTest.o $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@
else
$(EXECUTABLE): $(OBJECTS)
	@mkdir -p bin
	$(LINKER) $(LDFLAGS) $(OBJECTS) $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@

$(UNITTEST_EXECUTABLE): $(ESSENTIAL_OBJECTS) obj/debug/DRVR/debug.o $(SUITE_OBJECTS) unittests/obj/all.o unittests/obj/CuTest.o
	@mkdir -p bin
	$(LINKER) $(LDFLAGS) $(ESSENTIAL_OBJECTS) obj/debug/DRVR/debug.o $(SUITE_OBJECTS) unittests/obj/all.o unittests/obj/CuTest.o $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@
endif

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
ifeq ($(OS), Windows_NT)
	@if not exist obj mkdir obj
	@if not exist obj\debug mkdir obj\debug
	@if not exist "$(@D)" mkdir "$(@D)"
else
	@mkdir -p "$(@D)"
endif
	$(CC) $(CFLAGS) $< -o $@

unittests/obj/all.o: unittests/src/all.c unittests/src/all.h
	$(CC) -c unittests/src/all.c -Iinclude -o unittests/obj/all.o

unittests/obj/CuTest.o: unittests/CuTest.c unittests/CuTest.h
	$(CC) -c unittests/CuTest.c -Iinclude -o unittests/obj/CuTest.o

$(SUITE_OBJECTS): unittests/obj/%.o : unittests/src/%.c unittests/CuTestExtras.h
ifeq ($(OS), Windows_NT)
	@if not exist unittests\obj mkdir unittests\obj
endif
	$(CC) $(CFLAGS) $< -I"unittests" -o $@

ifeq ($(OS), Windows_NT)
$(DEBUG_EXECUTABLE): $(DEBUG_OBJECTS) $(WIN_ICON)
	@if not exist bin mkdir bin
	$(LINKER) $(LDFLAGS) $(DEBUG_OBJECTS) $(WIN_ICON) $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@
else
$(DEBUG_EXECUTABLE): $(DEBUG_OBJECTS)
	@mkdir -p bin
	$(LINKER) $(LDFLAGS) $(DEBUG_OBJECTS) $(LIBCURL_LIBS) $(LLVM_LIBS) -o $@
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
	$(WINDOWS_WINRES) -J rc -O coff -i $(WIN_ICON_SRC) -o $(WIN_ICON)

clean: rm_insight
ifeq ($(OS), Windows_NT)
	@del obj\debug\*.* /Q /S 1> nul 2>&1
	@del obj\*.* /S /Q 1> nul 2>&1
	@del bin\adept.exe /S /Q 1> nul 2>&1
	@del bin\adept_debug.exe /S /Q 1> nul 2>&1
	@if exist unittests\obj @del unittests\obj\*.* /S /Q 1> nul 2>&1
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

unittest:
	$(UNITTEST_EXECUTABLE)

generate:
	python3 include/TOKEN/generate_c.py
