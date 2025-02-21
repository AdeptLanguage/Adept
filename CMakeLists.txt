cmake_minimum_required(VERSION 3.13)
project(Adept)

set(ADEPT_LINK_LLVM_STATIC default CACHE STRING "Link against LLVM using static libs")
option(ADEPT_ENABLE_ASAN "Enable runtime address-santization / undefined behavior sanitization for debug builds (only available for Apple clang and LLVM clang)" Off)
option(ADEPT_ENABLE_LSAN "Enable runtime leak-sanitization for debug builds (only available for LLVM clang)" Off)

if(WIN32)
    set(ZLIB_USE_STATIC_LIBS On) # Only affects CMake 3.24+
endif()

find_package(LLVM REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(zstd REQUIRED)

if(ADEPT_LINK_LLVM_STATIC STREQUAL "default")
    if(WIN32)
        set(ADEPT_LINK_LLVM_STATIC On)
    else()
        set(ADEPT_LINK_LLVM_STATIC Off)
    endif()
endif()

if(MINGW AND ADEPT_LINK_LLVM_STATIC)
    set(zstd_LIBRARY "-lzstd")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Found Curl ${CURL_LIBRARIES}")
message(STATUS "Found ZLIB ${ZLIB_LIBRARIES}")
message(STATUS "Found ztd ${zstd_LIBRARY}")

if(MSVC)
    add_compile_options(/W4 /WX)
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    add_compile_options(-Wall -Wextra -pedantic)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(core_source_files
    src/AST/EXPR/ast_expr_free.c src/AST/EXPR/ast_expr_str.c
    src/AST/POLY/ast_resolve.c src/AST/POLY/ast_translate.c
    src/AST/TYPE/ast_type_clone.c src/AST/TYPE/ast_type_free.c
    src/AST/TYPE/ast_type_hash.c src/AST/TYPE/ast_type_helpers.c src/AST/TYPE/ast_type_identical.c
    src/AST/TYPE/ast_type_is.c src/AST/TYPE/ast_type_make.c src/AST/TYPE/ast_type_set.c src/AST/TYPE/ast_type_str.c
    src/AST/UTIL/string_builder_extensions.c src/AST/ast_dump.c
    src/AST/ast_expr.c src/AST/ast_layout.c src/AST/ast_named_expression.c
    src/AST/ast_poly_catalog.c src/AST/ast.c
    src/AST/meta_directives.c src/BKEND/backend.c src/BKEND/ir_to_c.c src/BKEND/ir_to_llvm.c src/BKEND/ir_to_llvm_impl.c src/BRIDGE/any.c
    src/BRIDGE/bridge.c src/BRIDGE/rtti_collector.c src/BRIDGEIR/rtti_table.c src/BRIDGEIR/rtti_table_entry.c src/BRIDGEIR/rtti.c src/DRVR/compiler.c
    src/DRVR/config.c src/DRVR/object.c src/INFER/infer.c
    src/IR/ir_pool.c src/IR/ir_proc_map.c src/IR/ir_type_map.c src/IR/ir_proc_query.c src/IR/ir_type.c src/IR/ir_type_spec.c src/IR/ir_value_str.c
    src/IR/ir.c src/IR/ir_dump.c src/IR/ir_func_endpoint.c src/IR/ir_lowering.c src/IR/ir_module.c src/IRGEN/ir_autogen.c
    src/IRGEN/ir_build_instr.c src/IRGEN/ir_build_literal.c src/IRGEN/ir_builder.c src/IRGEN/ir_cache.c src/IRGEN/ir_gen_args.c src/IRGEN/ir_gen_check_prereq.c
    src/IRGEN/ir_gen_expr.c src/IRGEN/ir_gen_find_sf.c src/IRGEN/ir_gen_find.c
    src/IRGEN/ir_gen_polymorphable.c src/IRGEN/ir_gen_qualifiers.c src/IRGEN/ir_gen_rtti.c src/IRGEN/ir_gen_stmt.c src/IRGEN/ir_gen_type.c
    src/IRGEN/ir_gen_vtree.c src/IRGEN/ir_gen.c src/IRGEN/ir_vtree.c
    src/LEX/lex.c src/LEX/token.c src/PARSE/parse_alias.c src/PARSE/parse_checks.c src/PARSE/parse_ctx.c
    src/PARSE/parse_dependency.c src/PARSE/parse_enum.c src/PARSE/parse_expr.c src/PARSE/parse_func.c
    src/PARSE/parse_global.c src/PARSE/parse_meta.c src/PARSE/parse_namespace.c src/PARSE/parse_pragma.c
    src/PARSE/parse_stmt.c src/PARSE/parse_struct.c src/PARSE/parse_type.c src/PARSE/parse_util.c
    src/PARSE/parse.c src/TOKEN/token_data.c src/UTIL/color.c src/UTIL/datatypes.c src/NET/download.c
    src/UTIL/builtin_type.c src/UTIL/filename.c src/UTIL/func_pair.c src/UTIL/ground.c src/UTIL/hash.c src/UTIL/jsmn_helper.c src/UTIL/levenshtein.c
    src/UTIL/list.c src/UTIL/search.c src/UTIL/set.c src/NET/stash.c src/UTIL/string_builder.c
    src/UTIL/string_list.c src/UTIL/string.c src/UTIL/util.c)

add_executable(adept)
target_include_directories(adept PRIVATE include ${CURL_INCLUDE_DIR} ${LLVM_INCLUDE_DIRS})
target_link_directories(adept PRIVATE ${CURL_LIBRARY_DIRS} ${LLVM_LIBRARY_DIRS})
target_compile_definitions(adept PRIVATE ADEPT_ENABLE_PACKAGE_MANAGER)

if(WIN32)
    target_link_libraries(adept "C:/Windows/System32/ntdll.dll")
endif()

add_library(libadept STATIC)
target_include_directories(libadept PRIVATE include ${CURL_INCLUDE_DIR} ${LLVM_INCLUDE_DIRS})
target_link_directories(libadept PRIVATE ${CURL_LIBRARY_DIRS} ${LLVM_LIBRARY_DIRS})
target_compile_definitions(libadept PRIVATE ADEPT_ENABLE_PACKAGE_MANAGER)

if(WIN32)
    target_link_libraries(libadept "C:/Windows/System32/ntdll.dll")
endif()

message(STATUS "Including debug files")
list(APPEND core_source_files src/DBG/debug.c)
add_definitions(-DENABLE_DEBUG_FEATURES)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Compiling with debug flags")

    if(ADEPT_ENABLE_ASAN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined,implicit-conversion,bounds")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,implicit-conversion,bounds")
    endif()

    if(ADEPT_ENABLE_LSAN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
    endif()
else()
    message(STATUS "Compiling without debug flags")
endif()

set(app_source_files
    ${core_source_files}
    src/MAIN/main.c
)

target_sources(adept PRIVATE ${app_source_files})
target_sources(libadept PRIVATE ${app_source_files})

if(WIN32)
    set(extra_libs ${EXTRA_REQUIRED_LIBS} ${zstd_LIBRARY} ${ZLIB_LIBRARIES} -lpsapi -lshell32 -lole32 -luuid -ladvapi32 -lws2_32)

    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -J rc -O coff")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__USE_MINGW_ANSI_STDIO")
    target_sources(adept PRIVATE res/icon.rc)
endif()

if(APPLE)
    set(extra_libs ${EXTRA_REQUIRED_LIBS} -lldap -lpthread ${zstd_LIBRARY} ${ZLIB_LIBRARIES} -lcurses)
endif()

if(UNIX AND NOT APPLE)
    # Linux
    set(extra_libs ${EXTRA_REQUIRED_LIBS} -ldl -lpthread ${zstd_LIBRARY} ${ZLIB_LIBRARIES} -lcurses -lm)
endif()

set(insight_out_dir ${CMAKE_SOURCE_DIR}/INSIGHT)
add_custom_target(
    insight
    COMMAND ${CMAKE_COMMAND} -E make_directory ${insight_out_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${insight_out_dir}/include
    COMMAND ${CMAKE_COMMAND} -E make_directory ${insight_out_dir}/src

    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/AST ${insight_out_dir}/include/AST
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/BRIDGE ${insight_out_dir}/include/BRIDGE
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/DRVR ${insight_out_dir}/include/DRVR
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/LEX ${insight_out_dir}/include/LEX
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/PARSE ${insight_out_dir}/include/PARSE
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/TOKEN ${insight_out_dir}/include/TOKEN
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include/UTIL ${insight_out_dir}/include/UTIL

    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/AST ${insight_out_dir}/src/AST
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/BRIDGE ${insight_out_dir}/src/BRIDGE
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/DRVR ${insight_out_dir}/src/DRVR
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/LEX ${insight_out_dir}/src/LEX
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/PARSE ${insight_out_dir}/src/PARSE
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/TOKEN ${insight_out_dir}/src/TOKEN
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/UTIL ${insight_out_dir}/src/UTIL

    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/UTIL/__insight.h ${insight_out_dir}/insight.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/UTIL/__insight_Makefile ${insight_out_dir}/Makefile
    COMMENT Insight files copied into ${insight_out_dir}
    VERBATIM
)

add_custom_target(
    generate
    COMMAND python3 include/TOKEN/generate_c.py
)

if(ADEPT_LINK_LLVM_STATIC)
    message(STATUS "Linking against LLVM statically")
    message(STATUS "${LLVM_LIBRARY_DIRS}/../bin/llvm-config")
    execute_process(COMMAND ${LLVM_LIBRARY_DIRS}/../bin/llvm-config --link-static --libs OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE llvm_static_libs)
    target_link_libraries(adept ${CURL_LIBRARIES} ${llvm_static_libs} ${extra_libs})
    target_link_libraries(libadept ${CURL_LIBRARIES} ${llvm_static_libs} ${extra_libs})
else()
    message(STATUS "Linking against LLVM dynamically")
    message(STATUS "${LLVM_LIBRARY_DIRS}/../bin/llvm-config")
    execute_process(COMMAND ${LLVM_LIBRARY_DIRS}/../bin/llvm-config --libs OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE llvm_dynamic_libs)
    target_link_libraries(adept ${CURL_LIBRARIES} ${llvm_dynamic_libs} ${zstd_LIBRARY} ${ZLIB_LIBRARIES})
    target_link_libraries(libadept ${CURL_LIBRARIES} ${llvm_dynamic_libs} ${zstd_LIBRARY} ${ZLIB_LIBRARIES})
endif()

set_target_properties(adept PROPERTIES C_STANDARD 11 LINKER_LANGUAGE CXX)

# Post compilation steps
if(WIN32)
    add_custom_command(
        TARGET adept POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res/necessities/windows/ $<TARGET_FILE_DIR:adept>
    )
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res/necessities/adept.config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

execute_process(COMMAND git clone https://github.com/IsaacShelton/AdeptImport ${CMAKE_CURRENT_BINARY_DIR}/import)

enable_testing()
add_subdirectory(tests)
