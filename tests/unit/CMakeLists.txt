
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
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/crt2.o DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/crtbegin.o DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/curl-ca-bundle.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/ld.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/libcurl-x64.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/libdep.a DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/libgcc_s_seh-1.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/libstdc++-6.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../../res/necessities/windows/libwinpthread-1.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

set_target_properties(UnitTestRunner adept PROPERTIES LINKER_LANGUAGE CXX)
add_test(UnitTests UnitTestRunner)
