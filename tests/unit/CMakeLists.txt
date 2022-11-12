
enable_testing()

add_executable(UnitTestRunner framework/CuTest.c
    src/ast_expr.test.c
    src/lex.test.c
    src/UnitTestRunner.c)

target_include_directories(UnitTestRunner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include include framework ${CURL_INCLUDE_DIR} ${LLVM_INCLUDE_DIRS})
target_link_directories(UnitTestRunner PRIVATE ${CURL_LIBRARY_DIRS} ${LLVM_LIBRARY_DIRS})

message(STATUS "Linking against LLVM statically")
message(STATUS "${LLVM_LIBRARY_DIRS}/../bin/llvm-config")
execute_process(COMMAND ${LLVM_LIBRARY_DIRS}/../bin/llvm-config --link-static --libs OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE llvm_static_libs)
target_link_libraries(UnitTestRunner libadept ${CURL_LIBRARIES} ${llvm_static_libs} ${extra_libs})

if(WIN32)
	add_custom_command(
		TARGET adept POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/ $<TARGET_FILE_DIR:adept>
	)
endif()

set_target_properties(UnitTestRunner adept PROPERTIES LINKER_LANGUAGE CXX)
add_test(UnitTests UnitTestRunner)
