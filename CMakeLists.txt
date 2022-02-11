cmake_minimum_required(VERSION 3.0)
project(Adept)

option(ADEPT_LINK_LLVM_STATIC "Link against LLVM using static libs" OFF)

find_package(LLVM REQUIRED)
find_package(CURL REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${CURL_INCLUDE_DIR})
link_directories(${CURL_LIBRARY_DIRS})
add_definitions( -DADEPT_ENABLE_PACKAGE_MANAGER )

message(STATUS "Found Curl ${CURL_LIBRARIES}")

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

include_directories(include)

set(app_source_files
	src/AST/EXPR/ast_expr_free.c src/AST/EXPR/ast_expr_str.c
	src/AST/TYPE/ast_type_clone.c src/AST/TYPE/ast_type_free.c
	src/AST/TYPE/ast_type_hash.c src/AST/TYPE/ast_type_helpers.c src/AST/TYPE/ast_type_identical.c
	src/AST/TYPE/ast_type_is.c src/AST/TYPE/ast_type_make.c src/AST/TYPE/ast_type_str.c
	src/AST/UTIL/string_builder_extensions.c
	src/AST/ast_constant.c src/AST/ast_expr.c src/AST/ast_layout.c
	src/AST/ast_poly_catalog.c src/AST/ast.c
	src/AST/meta_directives.c src/BKEND/backend.c src/BKEND/ir_to_llvm.c src/BKEND/ir_to_llvm_impl.c src/BRIDGE/any.c
	src/BRIDGE/bridge.c src/BRIDGE/funcpair.c src/BRIDGE/type_table.c src/BRIDGE/rtti.c src/DRVR/compiler.c
	src/DRVR/config.c src/DRVR/main.c src/DRVR/object.c src/DRVR/repl.c src/INFER/infer.c
	src/IR/ir_pool.c src/IR/ir_proc_map.c src/IR/ir_proc_query.c src/IR/ir_type.c src/IR/ir_type_spec.c src/IR/ir_value_str.c
	src/IR/ir.c src/IR/ir_dump.c src/IR/ir_func_endpoint.c src/IR/ir_lowering.c
	src/IRGEN/ir_builder.c src/IRGEN/ir_cache.c src/IRGEN/ir_gen_args.c src/IRGEN/ir_gen_check_prereq.c
	src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find_sf.c src/IRGEN/ir_gen_find.c
	src/IRGEN/ir_gen_polymorphable.c src/IRGEN/ir_gen_rtti.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c src/IRGEN/ir_gen.c
	src/LEX/lex.c src/LEX/pkg.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_ctx.c
	src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c
	src/PARSE/parse_global.c src/PARSE/parse_meta.c src/PARSE/parse_namespace.c src/PARSE/parse_pragma.c
	src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c
	src/PARSE/parse.c src/TOKEN/token_data.c src/UTIL/color.c src/UTIL/datatypes.c src/UTIL/download.c
	src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/ground.c src/UTIL/hash.c src/UTIL/jsmn_helper.c src/UTIL/levenshtein.c
	src/UTIL/list.c src/UTIL/memory.c src/UTIL/search.c src/UTIL/stash.c src/UTIL/string_builder.c
	src/UTIL/string_list.c src/UTIL/string.c src/UTIL/util.c
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(STATUS "Including debug files")
	list(APPEND app_source_files src/DRVR/debug.c)
	add_definitions( -DENABLE_DEBUG_FEATURES )
else()
	message(STATUS "Excluding debug files")
endif()

add_executable(adept ${app_source_files})

if(ADEPT_LINK_LLVM_STATIC)
	message(STATUS "Linking against LLVM statically")
	message(STATUS "${LLVM_LIBRARY_DIRS}/../bin/llvm-config")
	execute_process(COMMAND ${LLVM_LIBRARY_DIRS}/../bin/llvm-config --link-static --libs OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE llvm_static_libs)
	target_link_libraries(adept ${CURL_LIBRARIES} ${llvm_static_libs} ${EXTRA_REQUIRED_LIBS})
else()
	message(STATUS "Linking against LLVM dynamically")
	target_link_libraries(adept ${CURL_LIBRARIES} -lLLVM)
endif()

set_target_properties(adept PROPERTIES LINKER_LANGUAGE CXX)

# Post compilation steps

if(WIN32)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/crt2.o DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/crtbegin.o DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/ld.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/libcurl-x64.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/libgcc_s_seh-1.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/libstdc++-6.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/windows/libwinpthread-1.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/necessities/adept.config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
