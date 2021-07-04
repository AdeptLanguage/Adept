cmake_minimum_required(VERSION 3.0)
project(Adept)

option(ADEPT_LINK_LLVM_STATIC "Link against LLVM using static libs" OFF)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(/usr/local/opt/curl/include)
link_directories(/usr/local/opt/curl/lib)
add_definitions( -DADEPT_ENABLE_PACKAGE_MANAGER )

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(include)

add_executable(adept
	src/AST/ast_constant.c src/AST/ast_expr.c src/AST/ast_layout.c src/AST/ast_type.c src/AST/ast.c
	src/AST/meta_directives.c src/BKEND/backend.c src/BKEND/ir_to_llvm.c src/BRIDGE/any.c
	src/BRIDGE/bridge.c src/BRIDGE/type_table.c src/BRIDGE/rtti.c src/DRVR/compiler.c
	src/DRVR/config.c src/DRVR/main.c src/DRVR/object.c src/DRVR/repl.c src/INFER/infer.c
	src/IR/ir_pool.c src/IR/ir_type.c src/IR/ir_type_spec.c src/IR/ir.c src/IR/ir_lowering.c
	src/IRGEN/ir_builder.c src/IRGEN/ir_cache.c src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find.c
	src/IRGEN/ir_gen_rtti.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c src/IRGEN/ir_gen.c
	src/LEX/lex.c src/LEX/pkg.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_ctx.c
	src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c
	src/PARSE/parse_global.c src/PARSE/parse_meta.c src/PARSE/parse_namespace.c src/PARSE/parse_pragma.c
	src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c
	src/PARSE/parse.c src/TOKEN/token_data.c src/UTIL/color.c src/UTIL/datatypes.c src/UTIL/download.c
	src/UTIL/aes.c src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/hash.c src/UTIL/jsmn_helper.c src/UTIL/levenshtein.c
	src/UTIL/memory.c src/UTIL/search.c src/UTIL/stash.c src/UTIL/string_builder.c src/UTIL/tmpbuf.c src/UTIL/util.c
)

if(ADEPT_LINK_LLVM_STATIC)
	execute_process(COMMAND llvm-config --link-static --libs OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE llvm_static_libs)
	target_link_libraries(adept ${llvm_static_libs})
else()
	target_link_libraries(adept LLVM)
endif()

target_link_libraries(adept curl m)
